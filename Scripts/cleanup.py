#!/usr/bin/env python3.12
"""

Removes files by given glob masks from the Models folder (recursive).
Raises FileNotFoundError if the Models folder does not exist.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path
from typing import Final
from typing import Iterator
from typing import Sequence

_default_masks: Final[Sequence[str]] = (
    "*.bak",
    "*.log"
)


def get_files_by_mask(folder: Path, masks: Sequence[str]) -> Iterator[Path]:
    """Yield files in folder (recursively) matching any of the provided glob masks."""
    for mask in masks:
        yield from folder.rglob(mask)


def _cleanup(masks: Sequence[str] = ("*.bak",), *, dry_run: bool = True) -> None:
    root = Path(__file__).resolve().parent.parent  # This File -> Scripts Folder -> Root
    models_folder = root / "Models"

    if not models_folder.exists() or not models_folder.is_dir():
        raise FileNotFoundError(f"Models folder not found: {models_folder}")

    files = tuple(get_files_by_mask(models_folder, masks))

    if not files:
        print(f"No files found for masks: {masks}")
        return

    print(f"Found {len(files)} file(s) for masks: {masks}")
    for p in files:
        print(f" - {p}")

    if dry_run:
        print("Dry run: no files will be deleted. Re-run with --delete to remove them.")
        return

    removed = 0

    for p in files:

        try:
            p.unlink()
            removed += 1
            print(f'Removed: {p}')

        except Exception as e:
            sys.stderr.write(f'Failed to remove {p} : {e}\n')

    print(f"Removed {removed} of {len(files)} file(s).")


def _parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    p = argparse.ArgumentParser(description="Remove files by glob masks from the Models folder.")
    p.add_argument("masks", nargs="*", default=_default_masks, help='Glob masks (e.g. "*.bak" "*.tmp")')
    p.add_argument("-d", "--delete", action="store_true", help="Actually delete files (default: dry run)")
    return p.parse_args(argv)


def _start():
    args = _parse_args()

    try:
        _cleanup(tuple(args.masks), dry_run=not args.delete)

    except FileNotFoundError as e:
        # Exit with non-zero code as requested (program should crash if Models missing)
        sys.stderr.write(str(e))
        sys.exit(2)


if __name__ == "__main__":
    _start()
