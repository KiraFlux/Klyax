"""
Removes files by given glob masks from the Models folder (recursive).
Raises FileNotFoundError if the Models folder does not exist.
"""

from __future__ import annotations

import argparse
import sys
from argparse import ArgumentParser
from pathlib import Path
from typing import Final
from typing import Iterator
from typing import Sequence

_root_folder: Final = Path(__file__).resolve().parent.parent  # This File -> Scripts Folder -> Root
"""Project Folder"""

_models_folder: Final = _root_folder / "Models"
"""Models Folder"""


def get_files_by_mask(folder: Path, masks: Sequence[str]) -> Iterator[Path]:
    """Yield files in folder (recursively) matching any of the provided glob masks."""
    for mask in masks:
        yield from folder.rglob(mask)


def cleanup(folder: Path, masks: Sequence[str], *, dry_run: bool = True) -> None:
    """
    Searching for all files by `masks` in `folder`
    Deletes Selected files if `dry_run`
    """
    if len(masks) == 0:
        print(f"{masks=}. Exit")
        return

    print(f"Working in {folder=}")

    if not folder.exists() or not folder.is_dir():
        raise FileNotFoundError(f"Models folder not found: {folder}")

    files = tuple(get_files_by_mask(folder, masks))
    files_founded = len(files)

    if files_founded == 0:
        print(f"No files found for {masks=} (Everything in {folder=!r} is clean)")
        return

    print(f"{files_founded=} for {masks=}")
    for i, file in enumerate(files):
        print(f"{i + 1:>3} -> {file}")
    print()

    if dry_run:
        print("Dry run: no files will be deleted. Re-run with --delete to remove them.")
        return

    files_removed = 0

    for file in files:
        try:
            file.unlink()
            files_removed += 1
            print(f'Removed: {file}')

        except Exception as e:
            sys.stderr.write(f'Failed to remove {file} : {e}\n')

    print(f"{files_removed=} / {files_founded}")


def _create_cleanup_parser(p: ArgumentParser) -> None:
    p.add_argument(
        'masks',
        nargs='*',
        help='Glob masks'
    )

    p.add_argument(
        '-d',
        '--delete',
        action='store_true',
        help='Actually delete files'
    )


def _create_parser() -> ArgumentParser:
    p = argparse.ArgumentParser(description="Klyax project organizer tool")

    sp = p.add_subparsers(
        dest='mode',
        required=True,
    )
    _create_cleanup_parser(sp.add_parser(
        'cleanup',
        help='Remove junk files by glob masks from Models folder'
    ))

    return p


def _start():
    p = _create_parser()
    args = p.parse_args()

    try:
        match args.mode:  # type: ignore
            case 'cleanup':
                cleanup(_models_folder, tuple(args.masks), dry_run=not args.delete)


    except FileNotFoundError as e:
        # Exit with non-zero code as requested (program should crash if Models missing)
        sys.stderr.write(str(e))
        sys.exit(2)


if __name__ == "__main__":
    _start()
