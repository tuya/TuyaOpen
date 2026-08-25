#!/usr/bin/env python3
"""
tools/provider_rename.py - mechanical rewrite of the tal_network `card` names
to their `provider` replacements, for callers who cannot just apply a patch.

Background: docs/netmgr/provider_rename_plan.md. S1 introduced the new names
in src/tal_network/include/tal_network_register.h and turned the old ones into
aliases (typedefs and #defines). S2 mechanically rewrote every caller inside
this tree, in two directories: src/tal_network and
src/tuya_cloud_service/netmgr. S3 marks the aliases deprecated. This script is
the third leg: an out-of-tree caller who still has source using the old names
can point this at their own tree and get the same mechanical rewrite, instead
of retyping docs/netmgr/provider_rename_plan.md's mapping table by hand.

It is deliberately conservative:
  - it only ever touches the directory it is given, never the whole tree;
  - it defaults to dry-run and only writes with --apply;
  - it leaves the one field name with a real collision problem (`card_type`)
    alone unless the rename is unambiguous (a `.card_type` / `->card_type`
    member access), and just warns about bare-word occurrences instead of
    guessing;
  - it warns about, but does not rename, the three zero-caller compatibility
    wrappers that are deleted (not renamed) in S4.

See the self-test instructions in docs/netmgr/provider_rename_plan.md §4.3 for
how this script is expected to behave on a tree that has already migrated:
running it over src/ must report zero changes, because S2 already did the
work it would otherwise do.
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_PATH = Path(__file__).resolve()
REPO_ROOT = SCRIPT_PATH.parent.parent

# Directories we never descend into, wherever they occur.
SKIP_DIR_NAMES = {".git", "build", ".build", "dist"}

# File types this script knows how to read as C/C++ source. Anything else
# (and anything that fails to decode as UTF-8/ASCII, i.e. is probably binary)
# is skipped.
SOURCE_EXTS = {".c", ".h", ".cc", ".cpp", ".hh", ".hpp"}

# ---------------------------------------------------------------------------
# The mapping table, docs/netmgr/provider_rename_plan.md §3.3.
#
# 15 plain identifier renames here, plus the one special-cased member-access
# rename (`card_type` -> `provider`, handled separately below because of the
# collision list in §2.5) makes the 16 entries the plan's table lists.
#
# NOT in this table on purpose, per the plan's own notes on §3.3:
#   - `tal_net_provider_t.type` -> `.id`: that rename was never done, so a
#     script that "corrects" it would be inventing a change nobody asked for.
#   - `TAL_NET_PROVIDER_DEFAULT`, `netconn_desc_t.provider`,
#     `tal_net_route_t.provider`, `netmgr_link_info_t.provider`,
#     `TAL_NETWORK_OPS_T`, and the `tal_network_register.[ch]` / `tal_platform.c`
#     filenames: all listed in §3.3 as "unchanged".
#   - the three zero-caller wrappers: listed in §3.3 as "not renamed, deleted
#     in S4" - see RESERVED_DEPRECATED below.
# ---------------------------------------------------------------------------
IDENTIFIER_MAP = [
    ("TAL_NETWORK_CARD_TYPE_E", "tal_net_provider_id_t"),
    ("TAL_NETWORK_CARD_T", "tal_net_provider_t"),
    ("TAL_NETWORK_CARD_MANAGER_T", "tal_net_provider_registry_t"),
    ("tal_network_card_manager", "s_provider_registry"),
    ("active_card", "providers"),
    ("TAL_NETWORK_CARD_DEFAULT", "TAL_NET_PROVIDER_DEFAULT_OBJ"),
    ("tal_network_card_posix", "tal_net_provider_posix"),
    ("tal_network_card_platform", "tal_net_provider_tkl"),
    ("TAL_NET_TYPE_POSIX", "TAL_NET_PROVIDER_POSIX"),
    ("TAL_NET_TYPE_PLATFORM", "TAL_NET_PROVIDER_TKL"),
    ("TAL_NET_TYPE_AT_MODEM", "TAL_NET_PROVIDER_AT_MODEM"),
    ("TAL_NET_TYPE_MAX", "TAL_NET_PROVIDER_MAX"),
    ("tal_network_card_init", "tal_net_provider_init"),
    ("tal_network_get_active_ops", "tal_net_provider_ops"),
    ("tal_network_card_get_active_ip", "tal_net_route_src_ip"),
]

# `netmgr_conn_base_t.card_type` -> `.provider`, but ONLY as a member access
# (`.card_type` / `->card_type`). A bare-word `card_type` is left alone and
# just reported, because the same bare word means something completely
# different in 26 other files this tree happens to contain (see
# collision_census() below) and neither verification target compiles any of
# them, so a wrong guess here would compile clean and be silently wrong.
MEMBER_FIELD_OLD = "card_type"
MEMBER_FIELD_NEW = "provider"
_MEMBER_ACCESS_RE = re.compile(r"(\.|->)\s*\b" + MEMBER_FIELD_OLD + r"\b")
_BARE_WORD_RE = re.compile(r"\b" + MEMBER_FIELD_OLD + r"\b")

# Zero-caller compatibility wrappers: docs/netmgr/provider_rename_plan.md §3.3
# says explicitly these are "not renamed, deleted in S4" - giving a
# compatibility wrapper a new name would turn it into a new public API, which
# is the opposite of why it exists. The script must not rename these; it only
# warns that they are on their way out.
RESERVED_DEPRECATED = [
    "tal_network_card_set_active",
    "tal_network_card_get_active_type",
    "tal_network_card_set_active_ip",
]

# The one file this script must never rewrite: it is not a caller of the old
# names, it is their definition. S1 turned the old names into aliases *in this
# header* (typedefs and #defines pointing at the new names) precisely so that
# out-of-tree callers keep compiling; S4 deletes them here, and only here. If
# this script "renamed" TAL_NET_TYPE_POSIX inside its own #define here, it
# would turn `#define TAL_NET_TYPE_POSIX TAL_NET_PROVIDER_POSIX` into
# `#define TAL_NET_PROVIDER_POSIX TAL_NET_PROVIDER_POSIX` - a self-referential
# no-op that quietly deletes the compatibility alias this whole script exists
# to let old callers keep using. Every other file in the tree is a caller and
# is fair game; this one is the alias table itself.
EXCLUDED_SUFFIXES = ("src/tal_network/include/tal_network_register.h",)


def is_excluded(path: Path) -> bool:
    posix = path.as_posix()
    return any(posix.endswith(suffix) for suffix in EXCLUDED_SUFFIXES)

_IDENTIFIER_RES = [(re.compile(r"\b" + re.escape(old) + r"\b"), old, new) for old, new in IDENTIFIER_MAP]
_RESERVED_RES = [(re.compile(r"\b" + re.escape(name) + r"\b"), name) for name in RESERVED_DEPRECATED]


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


# ---------------------------------------------------------------------------
# Startup collision warning, docs/netmgr/provider_rename_plan.md §2.5.
# ---------------------------------------------------------------------------


def _categorize(rel_path):
    posix = rel_path.replace(os.sep, "/")
    if posix.startswith("src/tal_network/") or posix.startswith("src/tuya_cloud_service/netmgr/"):
        return "netmgr"
    if "lvgl_games" in posix:
        return "game (bare `card_type` is a card-game enum, e.g. pvz.c)"
    if os.path.basename(posix) == "hdspm.h":
        return "ALSA RME HDSPM sound driver headers"
    if any(k in posix for k in ("sdcard", "sd_card", "sdmmc", "fs_init")):
        return "SD-card drivers (multiple platforms)"
    return "unclassified - NEW since the plan's §2.5 survey, inspect by hand"


def collision_census(repo_root):
    """
    Live-scan repo_root for the bare word `card_type` in .c/.h files, and
    print the breakdown. This is the same grep docs/netmgr/provider_rename_plan.md
    §2.5 ran; it is re-run here (rather than hard-coded) so the warning stays
    true as the tree grows, instead of quietly going stale like the comment
    this repo's fix(netmgr) commit 407bccd2 had to correct.
    """
    print("=" * 78)
    print("collision warning (docs/netmgr/provider_rename_plan.md §2.5):")
    print(
        "  the bare word `card_type` means \"which network provider\" in exactly\n"
        "  one place (netmgr_conn_base_t) and something else everywhere else it\n"
        "  occurs in this tree. Neither verification target (Ubuntu, T5AI) compiles\n"
        "  the \"everywhere else\" files, so a blind rename would compile clean and\n"
        "  still be wrong. That is why this script only rewrites `.card_type` /\n"
        "  `->card_type` member accesses and just warns on the bare word."
    )
    try:
        proc = subprocess.run(
            ["git", "grep", "-lwE", "card_type", "--", "*.c", "*.h"],
            cwd=str(repo_root),
            capture_output=True,
            text=True,
            timeout=60,
        )
    except (OSError, subprocess.SubprocessError) as exc:
        print(f"  (could not run `git grep` to compute a live count: {exc})")
        print("  falling back to docs/netmgr/provider_rename_plan.md's own last survey:")
        print("    31 files on disk, 7 of them netmgr's, 24 of them not:")
        print("    apps/games/lvgl_games/src/pvz/pvz.c, 8 ALSA RME HDSPM headers,")
        print("    and 14 SD-card driver files across five platforms.")
        print("=" * 78)
        return

    if proc.returncode not in (0, 1):
        print(f"  (git grep failed: {proc.stderr.strip()})")
        print("=" * 78)
        return

    files = [f for f in proc.stdout.splitlines() if f]
    by_category = {}
    for f in files:
        by_category.setdefault(_categorize(f), []).append(f)

    netmgr_files = by_category.pop("netmgr", [])
    total = len(files)
    print(f"  live count just now: {total} files contain the bare word `card_type`,")
    print(f"  {len(netmgr_files)} of them under src/tal_network or src/tuya_cloud_service/netmgr.")
    for category, flist in sorted(by_category.items()):
        print(f"    {len(flist)} - {category}")
        for f in flist[:3]:
            print(f"        {f}")
        if len(flist) > 3:
            print(f"        ... and {len(flist) - 3} more")
    print("=" * 78)


# ---------------------------------------------------------------------------
# Directory-safety checks.
# ---------------------------------------------------------------------------


def refuse_if_too_broad(target: Path):
    resolved = target.resolve()
    if resolved == Path(resolved.anchor):
        eprint(f"refusing to run on the filesystem root ({resolved}).")
        eprint("pass a specific subdirectory, e.g. src/tal_network.")
        sys.exit(2)
    if resolved == REPO_ROOT.resolve():
        eprint(f"refusing to run on the repository root ({resolved}).")
        eprint(
            "docs/netmgr/provider_rename_plan.md §2.5: a tree-wide rewrite silently\n"
            "corrupts files this rename has nothing to do with (SD-card drivers, an\n"
            "LVGL game, ALSA headers) because neither verification target compiles\n"
            "them. Pass a specific subdirectory instead, e.g. src/tal_network or\n"
            "src/tuya_cloud_service/netmgr."
        )
        sys.exit(2)


# ---------------------------------------------------------------------------
# The rewrite itself.
# ---------------------------------------------------------------------------


class Change:
    __slots__ = ("path", "lineno", "old", "new")

    def __init__(self, path, lineno, old, new):
        self.path = path
        self.lineno = lineno
        self.old = old
        self.new = new


def iter_source_files(target: Path):
    for dirpath, dirnames, filenames in os.walk(target):
        dirnames[:] = [d for d in dirnames if d not in SKIP_DIR_NAMES]
        for name in filenames:
            if os.path.splitext(name)[1] in SOURCE_EXTS:
                yield Path(dirpath) / name


def rewrite_line(line):
    """Return (new_line, bare_word_hit, reserved_hits) for one source line."""
    reserved_hits = [name for pattern, name in _RESERVED_RES if pattern.search(line)]

    new_line = _MEMBER_ACCESS_RE.sub(lambda m: m.group(1) + MEMBER_FIELD_NEW, line)
    bare_word_hit = bool(_BARE_WORD_RE.search(new_line))

    for pattern, _old, new in _IDENTIFIER_RES:
        new_line = pattern.sub(new, new_line)

    return new_line, bare_word_hit, reserved_hits


def process_file(path: Path, apply: bool):
    try:
        raw = path.read_bytes()
    except OSError as exc:
        eprint(f"skip (unreadable): {path}: {exc}")
        return [], [], []

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return [], [], []  # binary or non-UTF-8: silently skipped, per spec

    lines = text.splitlines(keepends=True)
    changes = []
    bare_word_warnings = []
    reserved_warnings = []
    new_lines = []
    changed = False

    for i, line in enumerate(lines, start=1):
        new_line, bare_hit, reserved_hits = rewrite_line(line)
        if bare_hit:
            bare_word_warnings.append((path, i, line.rstrip("\n")))
        for name in reserved_hits:
            reserved_warnings.append((path, i, name, line.rstrip("\n")))
        if new_line != line:
            changes.append(Change(path, i, line.rstrip("\n"), new_line.rstrip("\n")))
            changed = True
        new_lines.append(new_line)

    if changed and apply:
        path.write_text("".join(new_lines), encoding="utf-8", newline="")

    return changes, bare_word_warnings, reserved_warnings


def main():
    parser = argparse.ArgumentParser(
        description="Rewrite tal_network's old `card` names to their `provider` "
        "replacements (docs/netmgr/provider_rename_plan.md §3.3), scoped to one directory."
    )
    parser.add_argument("directory", help="directory to rewrite (required; the repo root and `/` are refused)")
    parser.add_argument("--apply", action="store_true", help="write changes (default: dry-run, print only)")
    args = parser.parse_args()

    target = Path(args.directory)
    if not target.exists() or not target.is_dir():
        eprint(f"error: not a directory: {args.directory}")
        sys.exit(2)

    refuse_if_too_broad(target)
    collision_census(REPO_ROOT)

    all_changes = []
    all_bare_warnings = []
    all_reserved_warnings = []
    scanned = 0
    excluded = []

    for path in sorted(iter_source_files(target)):
        if is_excluded(path):
            excluded.append(path)
            continue
        scanned += 1
        changes, bare_warnings, reserved_warnings = process_file(path, args.apply)
        all_changes.extend(changes)
        all_bare_warnings.extend(bare_warnings)
        all_reserved_warnings.extend(reserved_warnings)

    if excluded:
        print("-" * 78)
        print("skipping (alias definitions, not callers - see EXCLUDED_SUFFIXES):")
        for path in excluded:
            print(f"  {path}")

    verb = "applied" if args.apply else "would change"
    files_changed = len({c.path for c in all_changes})
    for c in all_changes:
        print(f"{c.path}:{c.lineno}: [{verb}]")
        print(f"  - {c.old}")
        print(f"  + {c.new}")

    if all_bare_warnings:
        print("-" * 78)
        print(f"WARN: {len(all_bare_warnings)} bare-word `card_type` occurrence(s) left untouched")
        print("      (not a `.card_type` / `->card_type` member access - inspect by hand):")
        for path, lineno, line in all_bare_warnings:
            print(f"  {path}:{lineno}: {line.strip()}")

    if all_reserved_warnings:
        print("-" * 78)
        seen = set()
        print("WARN: reserved compatibility wrappers found (not renamed; deleted in S4):")
        for path, lineno, name, line in all_reserved_warnings:
            key = (path, lineno, name)
            if key in seen:
                continue
            seen.add(key)
            print(f"  {path}:{lineno}: {name} - {line.strip()}")

    print("-" * 78)
    print(
        f"scanned {scanned} file(s) under {target}, "
        f"{len(all_changes)} line change(s) in {files_changed} file(s) "
        f"({'applied' if args.apply else 'dry-run, nothing written'})."
    )


if __name__ == "__main__":
    main()
