"""
Klyax CLI

- Command Runner ABC
- Command Line Interface Wrapper
- Command Runner Implementations
"""

from __future__ import annotations

import sys
from abc import ABC
from abc import abstractmethod
from argparse import ArgumentParser
from argparse import Namespace
from dataclasses import dataclass
from dataclasses import field
from pathlib import Path
from typing import ClassVar
from typing import Final
from typing import Iterable
from typing import MutableMapping
from typing import Optional
from typing import Sequence
from typing import final

from klyax.models import AssemblyUnitModel
from klyax.models import Model
from klyax.models import ModelRegistry
from klyax.models import PartModel
from klyax.project import Project


@dataclass(kw_only=True)
class CommandRunner(ABC):
    """Command Mode Runner"""

    __intend_size: ClassVar = 4

    __intend: int = field(init=False, default=0)

    @classmethod
    @abstractmethod
    def name(cls) -> str:
        """Get Command Mode name"""

    @classmethod
    @abstractmethod
    def create(cls, args: Namespace) -> CommandRunner:
        """Create instance of command runner and pass cli args"""

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
    def push(self) -> None:
        """Push intend"""
        self.__intend += 1

    @final
    def pop(self) -> None:
        """Pop intend"""
        self.__intend -= 1
        assert self.__intend >= 0

    @final
    def _format_log(self, prefix: str, message: str) -> str:
        return f"[{self.name()}:{prefix}]{(' ' * self.__intend_size) * self.__intend} {message}\n"


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
@dataclass(kw_only=True)
class CleanupCommandRunner(CommandRunner):
    """Uses to clean up project folders"""

    __delete_flag: ClassVar = "--delete"
    __short_delete_flag: ClassVar = "-d"

    __target_folders: ClassVar = (
        Project.images_folder,
        Project.models_folder,
    )

    masks: Sequence[str]
    """File Masks"""

    dry: bool
    """Selected files will Delete if False"""

    @classmethod
    def name(cls) -> str:
        return "cleanup"

    @classmethod
    def create(cls, args: Namespace) -> CommandRunner:
        return cls(
            masks=args.masks,
            dry=not args.delete,
        )

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
        if len(self.masks) == 0:
            self.log_info(f"{self.masks=}. Exit")
            return

        files_removed = sum(map(self._cleanup, self.__target_folders))
        self.log_info(f"{files_removed=}")

    def _cleanup(self, folder: Path) -> int:
        """
        Searching for all files by `masks` in `folder`
        :param folder Target folder
        :raises FileNotFoundError if the Models folder does not exist.
        """

        self.log_info(f"Working in {folder=}")

        if not folder.exists() or not folder.is_dir():
            self.log_error(f"Models folder not found: {folder}")
            return 0

        files = tuple(Project.search_by_masks_recursive(folder, self.masks))
        files_founded = len(files)

        if files_founded == 0:
            self.log_info(f"No files found for {self.masks=} (Everything in {folder=} is clean)")
            return 0

        self.log_info(f"{files_founded=} for {self.masks=}")
        for i, file in enumerate(files):
            self.log_info(f"{i + 1:>3} -> {file}")

        if self.dry:
            self.log_info(f"Dry run: no files will be deleted. Re-run with {self.__short_delete_flag} ({self.__delete_flag}) to remove them.")
            return 0

        return sum(map(self._remove_file, files))

    def _remove_file(self, file: Path) -> bool:
        try:
            file.unlink()
            self.log_info(f'Removed: {file}')
            return True

        except Exception as e:
            self.log_error(f'Failed to remove {file} : {e}\n')
            return False


@final
@dataclass(kw_only=True)
class DisplayModelCommandRunner(CommandRunner):
    """Displays Project Model Info"""

    identifier: str

    @classmethod
    def name(cls) -> str:
        return "display"

    @classmethod
    def configure_parser(cls, p: ArgumentParser) -> None:
        p.add_argument(
            "identifier",
            help="Unix-like path from models folder to target file name (without extension)"
        )

    @classmethod
    def create(cls, args: Namespace) -> CommandRunner:
        return cls(
            identifier=args.identifier,
        )

    def run(self) -> None:
        part = ModelRegistry.get_part(self.identifier)

        if part is not None:
            self._display_part(part)
            return
        del part

        unit = ModelRegistry.get_assembly_unit(self.identifier)

        if unit is not None:
            self._display_assembly_unit(unit)
            return
        del unit

        self.log_error(f"Cannot find model with {self.identifier=}")

    def _display_model(self, model: Model) -> None:
        self.log_info("")
        self.log_info(f"{model.__class__.__name__} '{model.id}':")

        image_count = len(model.images)
        if image_count == 0:
            return

        self.push()
        self.log_info(f"Images: {image_count}")

        self.push()
        for i in model.images:
            self.log_info(i.name)
        self.pop()

        self.pop()

    def _display_part(self, part: PartModel) -> None:
        self._display_model(part)

        c = len(part.transitions)
        if c == 0:
            return

        self.push()
        self.log_info(f"Transitions: {c}")

        self.push()
        for t in part.transitions:
            self.log_info(t.name)
        self.pop()

        self.pop()

    def _display_assembly_unit(self, unit: AssemblyUnitModel) -> None:
        self._display_model(unit)

        i = len(unit.models)
        if i == 0:
            return

        self.push()
        self.log_info(f"Models: {i}")

        self.push()
        for m in unit.models:

            if isinstance(m, PartModel):
                self._display_part(m)

            elif isinstance(m, AssemblyUnitModel):
                self._display_assembly_unit(m)

            else:
                raise TypeError(m)
        self.pop()

        self.pop()


@final
class UpdateReadmeCommandRunner(CommandRunner):
    """Updates project readme files"""

    @classmethod
    def name(cls) -> str:
        return "update-readme"

    @classmethod
    def create(cls, args: Namespace) -> CommandRunner:
        return cls()

    @classmethod
    def configure_parser(cls, p: ArgumentParser) -> None:
        pass

    def run(self) -> None:
        pass
