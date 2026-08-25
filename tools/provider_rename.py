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
  - it leaves the field names with a real collision problem (`card_type`,
    `active_card`) alone unless the rename is unambiguous (a `.field` /
    `->field` member access), and just warns about bare-word occurrences
    instead of guessing;
  - it warns about, but does not rename, the three zero-caller compatibility
    wrappers that are deleted (not renamed) in S4.

This script follows the tree, not the plan document, wherever the two
disagree - see the tal_network_card_manager entry in IDENTIFIER_MAP below for
the one place that matters.

See the self-test instructions in docs/netmgr/provider_rename_plan.md §4.3 for
how this script is expected to behave on a tree that has already migrated:
running it over src/ must report zero changes, because S2 already did the
work it would otherwise do.
"""

import argparse
import os
import re
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
# The mapping table, docs/netmgr/provider_rename_plan.md §3.3 - with one
# deliberate departure from that document, noted inline below.
#
# 14 plain identifier renames here, plus the two special-cased member-access
# renames (`card_type` -> `provider`, `active_card` -> `providers`, handled
# separately below) makes the 16 entries the plan's table lists.
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
    # The plan's own table (§3.3) says this becomes `s_provider_registry`.
    # That is NOT what S2 shipped: S2a rejected that name (413fe17d) because
    # the object is a non-static global today, and an `s_` prefix on a symbol
    # with external linkage is a lie about its linkage. `static` is a linkage
    # change, out of scope until S4 - that is the commit that will actually
    # rename this to `s_provider_registry`. Until then the tree has
    # `tal_net_provider_registry`, and this script follows the tree: a script
    # that rewrites a caller onto a symbol that does not exist is worse than
    # no script at all.
    ("tal_network_card_manager", "tal_net_provider_registry"),
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

# Fields renamed ONLY as a member access (`.field` / `->field`), never as a
# bare word:
#
#   `netmgr_conn_base_t.card_type` -> `.provider`
#       The same bare word means something else in every one of the other
#       places it occurs on disk - an LVGL game's card-type enum, the ALSA RME
#       HDSPM sound driver headers, SD-card drivers across five platforms.
#       Neither verification target (Ubuntu, T5AI) compiles any of those
#       files, so a wrong guess here would compile clean and still be wrong.
#       print_collision_warning() below names the families; the real numbers
#       for a given caller's tree come from the bare-word scan this script
#       already runs over the directory it is given.
#
#   `active_card` -> `providers`
#       Unlike card_type, this one has no known collision problem: it and the
#       struct that holds it (TAL_NETWORK_CARD_MANAGER_T) are private to
#       tal_network_register.c - the struct is declared in the .c - so no
#       out-of-tree caller can even name them. That is exactly why a
#       whole-word rename here has no upside (there is no caller to fix) and
#       one downside: `providers` is a common enough word that corrupting an
#       unrelated local by mistake would not necessarily fail to compile.
#       Restricted to member access out of caution, not a known collision.
MEMBER_ACCESS_FIELDS = [
    ("card_type", "provider"),
    ("active_card", "providers"),
]
# No \s* between the operator and the field: a real member access is written
# `.field` / `->field` with no gap. Allowing a gap here made this misfire on
# ordinary prose - a sentence ending "...anything. active_card[] is" was read
# as a member access on the word "anything" and mangled into
# "...anything.providers[] is", swallowing the space along with it.
_MEMBER_ACCESS_RES = [
    (re.compile(r"(\.|->)" + re.escape(old) + r"\b"), old, new) for old, new in MEMBER_ACCESS_FIELDS
]
_BARE_WORD_RES = [(re.compile(r"\b" + re.escape(old) + r"\b"), old) for old, new in MEMBER_ACCESS_FIELDS]

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

# The alias-definition file (src/tal_network/include/tal_network_register.h)
# must never be rewritten: it is not a caller of the old names, it is their
# definition. S1 turned the old names into aliases *there* (typedefs and
# #defines pointing at the new names) precisely so that out-of-tree callers
# keep compiling; S4 deletes them there, and only there. If this script
# "renamed" TAL_NET_TYPE_POSIX inside its own #define, it would turn
# `#define TAL_NET_TYPE_POSIX TAL_NET_PROVIDER_POSIX` into
# `#define TAL_NET_PROVIDER_POSIX TAL_NET_PROVIDER_POSIX` - a self-referential
# no-op that quietly deletes the compatibility alias this whole script exists
# to let old callers keep using.
#
# Matched by content, not by path: S3 put a `[deprecated-s4]` marker in front
# of both #define blocks in that header for exactly this reason. A path match
# breaks silently the moment the header is moved, renamed, or vendored under a
# different tree layout by an out-of-tree caller; the marker travels with the
# file no matter where it ends up.
ALIAS_FILE_MARKER = "[deprecated-s4]"

_IDENTIFIER_RES = [(re.compile(r"\b" + re.escape(old) + r"\b"), old, new) for old, new in IDENTIFIER_MAP]
_RESERVED_RES = [(re.compile(r"\b" + re.escape(name) + r"\b"), name) for name in RESERVED_DEPRECATED]


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


# ---------------------------------------------------------------------------
# Startup collision warning, docs/netmgr/provider_rename_plan.md §2.5.
# ---------------------------------------------------------------------------


def print_collision_warning():
    """
    Names the families of files where the bare word `card_type` means
    something other than "network provider", without a file count.

    A count is deliberately not printed here, in either form: a hardcoded
    count is a snapshot of THIS tree that goes stale (31/7/24 in the plan,
    26/0/26 by the time S2 finished, and still moving), and a live count of
    THIS tree is meaningless to a caller running this script somewhere else -
    the number that matters to them is about their own tree, and the
    bare-word scan below already produces exactly that, for the directory
    they gave it.
    """
    print("=" * 78)
    print("collision warning (docs/netmgr/provider_rename_plan.md §2.5):")
    print(
        "  the bare word `card_type` means \"which network provider\" in exactly\n"
        "  one place (netmgr_conn_base_t) and something else in every one of these\n"
        "  families, wherever they occur in a tree that vendors this SDK:\n"
        "    - an LVGL game's card-type enum\n"
        "    - the ALSA RME HDSPM sound driver headers\n"
        "    - SD-card drivers across five platforms\n"
        "  Neither verification target (Ubuntu, T5AI) compiles any of those files,\n"
        "  so a blind rename would compile clean and still be wrong. That is why\n"
        "  this script only rewrites `.card_type` / `->card_type` member accesses\n"
        "  and just warns on the bare word - see the WARN lines below for the real\n"
        "  count in the directory you gave it."
    )
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
    """Return (new_line, bare_word_hits, reserved_hits) for one source line."""
    reserved_hits = [name for pattern, name in _RESERVED_RES if pattern.search(line)]

    new_line = line
    for pattern, _old, new in _MEMBER_ACCESS_RES:
        new_line = pattern.sub(lambda m, new=new: m.group(1) + new, new_line)

    bare_word_hits = [old for pattern, old in _BARE_WORD_RES if pattern.search(new_line)]

    for pattern, _old, new in _IDENTIFIER_RES:
        new_line = pattern.sub(new, new_line)

    return new_line, bare_word_hits, reserved_hits


def process_file(path: Path, apply: bool):
    """Return (changes, bare_word_warnings, reserved_warnings, is_alias_file)."""
    try:
        raw = path.read_bytes()
    except OSError as exc:
        eprint(f"skip (unreadable): {path}: {exc}")
        return [], [], [], False

    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError:
        return [], [], [], False  # binary or non-UTF-8: silently skipped, per spec

    if ALIAS_FILE_MARKER in text:
        return [], [], [], True  # the alias-definition file itself - see ALIAS_FILE_MARKER

    lines = text.splitlines(keepends=True)
    changes = []
    bare_word_warnings = []
    reserved_warnings = []
    new_lines = []
    changed = False

    for i, line in enumerate(lines, start=1):
        new_line, bare_hits, reserved_hits = rewrite_line(line)
        for old in bare_hits:
            bare_word_warnings.append((path, i, old, line.rstrip("\n")))
        for name in reserved_hits:
            reserved_warnings.append((path, i, name, line.rstrip("\n")))
        if new_line != line:
            changes.append(Change(path, i, line.rstrip("\n"), new_line.rstrip("\n")))
            changed = True
        new_lines.append(new_line)

    if changed and apply:
        path.write_text("".join(new_lines), encoding="utf-8", newline="")

    return changes, bare_word_warnings, reserved_warnings, False


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
    print_collision_warning()

    all_changes = []
    all_bare_warnings = []
    all_reserved_warnings = []
    scanned = 0
    excluded = []

    for path in sorted(iter_source_files(target)):
        changes, bare_warnings, reserved_warnings, is_alias_file = process_file(path, args.apply)
        if is_alias_file:
            excluded.append(path)
            continue
        scanned += 1
        all_changes.extend(changes)
        all_bare_warnings.extend(bare_warnings)
        all_reserved_warnings.extend(reserved_warnings)

    print("-" * 78)
    if excluded:
        print(f"skipping (contains the `{ALIAS_FILE_MARKER}` alias-definition marker, not a caller):")
        for path in excluded:
            print(f"  {path}")
    else:
        print(
            f"note: no file under {target} carries the `{ALIAS_FILE_MARKER}` alias-definition "
            "marker. That's expected for a caller's tree - it doesn't ship "
            "tal_network_register.h - just confirming this run didn't silently miss it."
        )

    verb = "applied" if args.apply else "would change"
    files_changed = len({c.path for c in all_changes})
    for c in all_changes:
        print(f"{c.path}:{c.lineno}: [{verb}]")
        print(f"  - {c.old}")
        print(f"  + {c.new}")

    if all_bare_warnings:
        print("-" * 78)
        print(f"WARN: {len(all_bare_warnings)} bare-word occurrence(s) left untouched")
        print("      (not a `.field` / `->field` member access - inspect by hand):")
        for path, lineno, old, line in all_bare_warnings:
            print(f"  {path}:{lineno}: `{old}` - {line.strip()}")

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
