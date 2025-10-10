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
from typing import Final
from typing import Iterable
from typing import Iterator
from typing import Optional
from typing import Sequence


class Project:
    """Project tools"""

    root_folder: Final = Path(__file__).resolve().parent.parent  # This File -> Scripts Folder -> Root
    """Project Folder"""

    models_folder: Final = root_folder / "Models"
    """Models Folder"""

    def __init__(self):
        raise TypeError(f"Cannot create instance of {self.__class__.__name__}")

    @staticmethod
    def get_files_by_mask(folder: Path, masks: Sequence[str]) -> Iterator[Path]:
        """Yield files in folder (recursively) matching any of the provided glob masks."""
        for mask in masks:
            yield from folder.rglob(mask)


class CommandRunner(ABC):
    """Command Mode Runner"""

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


class CommandLineInterface:
    """CLI"""

    def __init__(self, runner_implementations: Iterable[type[CommandRunner]] = tuple()) -> None:
        self.__parser: Final = ArgumentParser(
            description="Klyax project organizer tool"
        )

        self.__sub_parsers: Final = self.__parser.add_subparsers(
            dest='mode',
            required=True,
        )

        self.__command_runner_implementations: Final[MutableMapping[str, Callable[[], CommandRunner]]] = dict()

        for runner_impl in runner_implementations:
            self.register(runner_impl)

    def parse_args(self, args: Optional[Sequence[str]]) -> Namespace:
        """Get arguments"""
        return self.__parser.parse_args(args)

    def get(self, mode: str) -> Callable[[], CommandRunner]:
        """Get command mode implementation"""
        return self.__command_runner_implementations[mode]

    def register(self, runner_impl: type[CommandRunner]) -> None:
        """Register command mode"""
        self.__command_runner_implementations[runner_impl.name()] = runner_impl

        runner_impl.configure_parser(self.__sub_parsers.add_parser(
            runner_impl.name(),
        ))


class CleanupCommandRunner(CommandRunner):

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
        self._cleanup(Project.models_folder, tuple(args.masks), dry_run=not args.delete)

    @classmethod
    def _cleanup(cls, folder: Path, masks: Sequence[str], *, dry_run: bool = True) -> None:
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

        files = tuple(Project.get_files_by_mask(folder, masks))
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


def _start():
    cli = CommandLineInterface((
        CleanupCommandRunner,
    ))

    args = cli.parse_args(None)
    command_runner = cli.get(args.mode)()

    try:
        command_runner.run(args)

    except Exception as e:
        sys.stderr.write(str(e))
        sys.exit(1)


if __name__ == "__main__":
    _start()
