"""Klyax Project Python script"""

from __future__ import annotations

import sys
from abc import ABC
from abc import abstractmethod
from argparse import ArgumentParser
from argparse import Namespace
from collections.abc import MutableMapping
from pathlib import Path
from typing import Callable
from typing import ClassVar
from typing import Final
from typing import Iterator
from typing import Optional
from typing import Sequence

_root_folder: Final = Path(__file__).resolve().parent.parent  # This File -> Scripts Folder -> Root
"""Project Folder"""

_models_folder: Final = _root_folder / "Models"
"""Models Folder"""


class CommandMode(ABC):
    """Command Mode Runner"""

    __command_modes: ClassVar[MutableMapping[str, Callable[[], CommandMode]]] = dict()

    __parser: ClassVar = ArgumentParser(description="Klyax project organizer tool")

    __sub_parsers: ClassVar = __parser.add_subparsers(
        dest='mode',
        required=True,
    )

    @classmethod
    def get(cls, mode: str) -> Callable[[], CommandMode]:
        """Get command mode implementation"""
        return cls.__command_modes[mode]

    @classmethod
    def parse_args(cls, args: Optional[Sequence[str]]) -> Namespace:
        """Get arguments"""
        return cls.__parser.parse_args(args)

    @classmethod
    def register(cls, command_mode: type[CommandMode]) -> None:
        """Register command mode"""
        cls.__command_modes[command_mode.name()] = command_mode

        command_mode.configure_parser(cls.__sub_parsers.add_parser(
            command_mode.name(),
        ))

    @classmethod
    @abstractmethod
    def name(cls) -> str:
        """Get Command Mode name"""

    @classmethod
    @abstractmethod
    def configure_parser(cls, p: ArgumentParser) -> None:
        """Configure mode subparser"""

    @abstractmethod
    def run(self, args: Namespace) -> None:
        """Execute command"""


class CleanupCommandMode(CommandMode):

    @classmethod
    def name(cls) -> str:
        return "cleanup"

    @classmethod
    def configure_parser(cls, p: ArgumentParser) -> None:
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

    def run(self, args: Namespace) -> None:
        self._cleanup(_models_folder, tuple(args.masks), dry_run=not args.delete)

    def _cleanup(self, folder: Path, masks: Sequence[str], *, dry_run: bool = True) -> None:
        """
        Searching for all files by `masks` in `folder`
        :param dry_run: Selected files will Delete if False
        :param masks:
        :param folder Target folder
        :raises FileNotFoundError if the Models folder does not exist.
        """
        if len(masks) == 0:
            print(f"{masks=}. Exit")
            return

        print(f"Working in {folder=}")

        if not folder.exists() or not folder.is_dir():
            raise FileNotFoundError(f"Models folder not found: {folder}")

        files = tuple(self._get_files_by_mask(folder, masks))
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

    @staticmethod
    def _get_files_by_mask(folder: Path, masks: Sequence[str]) -> Iterator[Path]:
        """Yield files in folder (recursively) matching any of the provided glob masks."""
        for mask in masks:
            yield from folder.rglob(mask)


def _start():
    CommandMode.register(CleanupCommandMode)

    args = CommandMode.parse_args(None)
    command_mode_factory = CommandMode.get(args.mode)

    try:
        command_mode_factory().run(args)

    except Exception as e:
        sys.stderr.write(str(e))
        sys.exit(1)


if __name__ == "__main__":
    _start()
