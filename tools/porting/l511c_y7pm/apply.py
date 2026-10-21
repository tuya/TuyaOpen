#!/usr/bin/env python3
"""Apply the versioned TuyaOpen bridge overlay to the pinned L511C vendor SDK."""

import argparse
import hashlib
import json
import os
import subprocess
import tempfile
from pathlib import Path

PIN = "350c04d12d8b8f50463134c2c2e966bf43d60f3b"
REMOTE = "https://github.com/tuya/TuyaOpen-LYNQ.git"
BRANCH = "l511c-y7pm"
HERE = Path(__file__).resolve().parent
PATCH = HERE / "patches" / "0001-tuyaopen-build-bridge.patch"
OVERLAY = HERE / "overlay"
MANIFEST_REL = Path(".tuyaopen/l511c_y7pm-overlay.json")


def git(repo: Path, *args: str, check: bool = True):
    return subprocess.run(["git", *args], cwd=repo, check=check, text=True, capture_output=True)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def patch_state(repo: Path) -> str:
    forward = git(repo, "apply", "--check", "--ignore-whitespace", str(PATCH), check=False)
    if forward.returncode == 0:
        return "pending"
    reverse = git(repo, "apply", "--reverse", "--check", "--ignore-whitespace", str(PATCH), check=False)
    if reverse.returncode == 0:
        return "applied"
    detail = (forward.stderr or reverse.stderr).strip()
    raise SystemExit("vendor patch cannot be applied cleanly:\n" + detail)


def clone_pinned_vendor(vendor: Path) -> None:
    """Clone the branch, then pin the new checkout even if its tip advanced."""
    subprocess.run(
        ["git", "clone", "--depth", "1", "--single-branch", "--branch", BRANCH, REMOTE, str(vendor)],
        check=True,
    )
    head = git(vendor, "rev-parse", "HEAD").stdout.strip()
    if head != PIN:
        git(vendor, "fetch", "--depth", "1", "origin", PIN)
        git(vendor, "checkout", "--detach", PIN)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sdk-root", type=Path, default=HERE.parents[2], help="TuyaOpenSDK root")
    args = parser.parse_args()
    vendor = args.sdk_root.resolve() / "platform" / "L511C_Y7PM"
    if not vendor.exists():
        vendor.parent.mkdir(parents=True, exist_ok=True)
        clone_pinned_vendor(vendor)
    elif not (vendor / ".git").exists():
        raise SystemExit(f"refusing to alter existing non-checkout path: {vendor}")
    head = git(vendor, "rev-parse", "HEAD").stdout.strip()
    if head != PIN:
        raise SystemExit(f"expected vendor commit {PIN}, found {head}")

    state = patch_state(vendor)
    manifest_path = vendor / MANIFEST_REL
    previous = {}
    if manifest_path.exists():
        previous = json.loads(manifest_path.read_text(encoding="utf-8")).get("files", {})

    sources = [p for p in sorted(OVERLAY.rglob("*")) if p.is_file()]
    conflicts = []
    for source in sources:
        rel = source.relative_to(OVERLAY).as_posix()
        target = vendor / rel
        if not target.exists() or target.read_bytes() == source.read_bytes():
            continue
        recorded = previous.get(rel)
        if recorded is None or sha256(target) != recorded:
            conflicts.append(rel)
    if conflicts:
        raise SystemExit("overlay target differs from source and recorded prior apply; refusing overwrite:\n  " + "\n  ".join(conflicts))

    # All conflicts and patch applicability are checked before the first write.
    updated = []
    for source in sources:
        rel = source.relative_to(OVERLAY)
        target = vendor / rel
        if target.exists() and target.read_bytes() == source.read_bytes():
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.NamedTemporaryFile(dir=target.parent, delete=False) as stream:
            temp = Path(stream.name)
            stream.write(source.read_bytes())
        os.replace(temp, target)
        updated.append(rel.as_posix())

    if state == "pending":
        git(vendor, "apply", "--ignore-whitespace", str(PATCH))
    manifest = {"vendor_commit": PIN, "files": {
        source.relative_to(OVERLAY).as_posix(): sha256(vendor / source.relative_to(OVERLAY))
        for source in sources
    }}
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Vendor SDK: {vendor} @ {PIN}")
    print("Patch: " + ("applied" if state == "pending" else "already applied"))
    print("Overlay files: " + (", ".join(updated) if updated else "already current"))
    print(f"Manifest: {manifest_path.relative_to(vendor)}")


if __name__ == "__main__":
    main()
