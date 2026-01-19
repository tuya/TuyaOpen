#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: pdf_to_pages.py /path/to/book.pdf", file=sys.stderr)
        return 2

    pdf = Path(sys.argv[1])
    if not pdf.is_file():
        print(f"Not found: {pdf}", file=sys.stderr)
        return 2

    out_dir = pdf.with_suffix("")
    out_dir = out_dir.parent / f"{out_dir.name}_pages"
    out_dir.mkdir(parents=True, exist_ok=True)

    if shutil.which("pdftoppm") is None:
        print("Missing tool: pdftoppm (poppler-utils). Install it and retry.", file=sys.stderr)
        return 2

    prefix = out_dir / "page"
    try:
        subprocess.check_call([
            "pdftoppm",
            "-jpeg",
            "-jpegopt",
            "quality=85",
            "-r",
            "120",
            str(pdf),
            str(prefix),
        ])
    except subprocess.CalledProcessError as e:
        return e.returncode

    pages = sorted(out_dir.glob("page-*.jpg"), key=lambda p: int(p.stem.split("-")[-1]))
    for i, p in enumerate(pages, start=1):
        dst = out_dir / f"{i:04d}.jpg"
        if dst.exists():
            continue
        os.replace(p, dst)

    for p in out_dir.glob("page-*.jpg"):
        p.unlink(missing_ok=True)

    print(out_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
