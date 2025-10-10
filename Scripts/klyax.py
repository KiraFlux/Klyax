"""Klyax Project Python script"""

from __future__ import annotations

import sys
from abc import ABC
from abc import abstractmethod
from argparse import ArgumentParser
from argparse import Namespace
from collections.abc import MutableMapping
from dataclasses import dataclass
from pathlib import Path
from typing import ClassVar
from typing import Final
from typing import Iterable
from typing import Iterator
from typing import Optional
from typing import Sequence
from typing import final


@final
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
    def create(cls, args: Namespace) -> CommandRunner:
        """Create instance of command runner and pass cli args"""

    @classmethod
    @abstractmethod
    def name(cls) -> str:
        """Get Command Mode name"""

    @classmethod
    @abstractmethod
    def configure_parser(cls, p: ArgumentParser) -> None:
        """Configure mode subparser"""

    @abstractmethod
    def run(self) -> None:
        """Execute command"""

    @final
    def log_info(self, message: str) -> None:
        """Write an Info-level log to stdout"""
        sys.stdout.write(self._format_log("info", message))

    @final
    def log_error(self, message: str) -> None:
        """Write an Error-level log to stderr"""
        sys.stderr.write(self._format_log("error", message))

    @final
    def _format_log(self, prefix: str, message: str) -> str:
        return f"[{self.name()}:{prefix}] {message}\n"


@final
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

        self.__command_runner_implementations: Final[MutableMapping[str, type[CommandRunner]]] = dict()

        for runner_impl in runner_implementations:
            self.register(runner_impl)

    def parse_args(self, args: Optional[Sequence[str]]) -> Namespace:
        """Parse args to attributes object"""
        return self.__parser.parse_args(args)

    def get(self, mode: str) -> type[CommandRunner]:
        """Get command mode implementation"""
        return self.__command_runner_implementations[mode]

    def register(self, runner_impl: type[CommandRunner]) -> None:
        """Register command mode"""
        self.__command_runner_implementations[runner_impl.name()] = runner_impl

        runner_impl.configure_parser(self.__sub_parsers.add_parser(
            runner_impl.name(),
        ))


@final
@dataclass(kw_only=True, frozen=True)
class CleanupCommandRunner(CommandRunner):
    """Uses to clean up models folder"""

    __delete_flag: ClassVar = "--delete"
    __short_delete_flag: ClassVar = "-d"

    masks: Sequence[str]
    """File Masks"""

    dry: bool
    """Selected files will Delete if False"""

    @classmethod
    def create(cls, args: Namespace) -> CommandRunner:
        return CleanupCommandRunner(
            masks=args.masks,
            dry=not args.delete,
        )

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
            cls.__short_delete_flag, cls.__delete_flag,
            action='store_true',
            help='Actually delete files'
        )

    def run(self) -> None:
        self._cleanup(Project.models_folder)

    def _cleanup(self, folder: Path) -> None:
        """
        Searching for all files by `masks` in `folder`
        :param folder Target folder
        :raises FileNotFoundError if the Models folder does not exist.
        """
        if len(self.masks) == 0:
            self.log_info(f"{self.masks=}. Exit")
            return

        self.log_info(f"Working in {folder=}")

        if not folder.exists() or not folder.is_dir():
            raise FileNotFoundError(f"Models folder not found: {folder}")

        files = tuple(Project.get_files_by_mask(folder, self.masks))
        files_founded = len(files)

        if files_founded == 0:
            self.log_info(f"No files found for {self.masks=} (Everything in {folder=} is clean)")
            return

        self.log_info(f"{files_founded=} for {self.masks=}")
        for i, file in enumerate(files):
            self.log_info(f"{i + 1:>3} -> {file}")

        if self.dry:
            self.log_info(f"Dry run: no files will be deleted. Re-run with {self.__short_delete_flag} ({self.__delete_flag}) to remove them.")
            return

        files_removed = sum(map(self._remove_file, files))

        self.log_info(f"{files_removed=} / {files_founded}")

    def _remove_file(self, file: Path) -> bool:
        try:
            file.unlink()
            self.log_info(f'Removed: {file}')
            return True

        except Exception as e:
            self.log_error(f'Failed to remove {file} : {e}\n')
            return False


def _start():
    cli = CommandLineInterface((
        CleanupCommandRunner,
    ))

    args = cli.parse_args(None)
    command_runner = cli.get(args.mode).create(args)

    try:
        command_runner.run()

    except Exception as e:
        sys.stderr.write(str(e))
        sys.exit(1)


if __name__ == "__main__":
    _start()
