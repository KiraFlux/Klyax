"""Project tools"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Final
from typing import Iterable
from typing import Iterator
from typing import final


@final
class Project:
    """Project tools"""

    id_separator: Final = '/'
    """Uses to separate Identifiers"""

    words_separator: Final = '-'
    """Uses to separate Words in Identifier"""

    root_folder: Final = Path(os.getcwd()).resolve().parent  # klyax module -> Scripts Folder -> Root
    """Project Folder"""

    models_folder: Final = root_folder / "Models"
    """Models Folder"""

    images_folder: Final = root_folder / "Images"
    """Images folder"""

    image_extensions: Final = (
        "jpg",
        "jpeg",
        "png",
    )

    part_model_extension: Final = "m3d"

    parts_masks: Final = tuple(
        f"*.{e}"
        for e in (part_model_extension,)
    )

    part_transition_extensions: Final = (
        "stp",
        "step",
        "3mf",
        "gcode",
    )

    assembly_unit_model_extension: Final = "a3d"

    def __init__(self):
        raise TypeError(f"Cannot create instance of {self.__class__.__name__}")

    @staticmethod
    def search_by_masks_recursive(root_folder: Path, masks: Iterable[str]) -> Iterator[Path]:
        """Yield files in folder (recursively) matching any of the provided glob masks."""
        for mask in masks:
            yield from root_folder.rglob(mask)

    @staticmethod
    def search_by_masks(target_folder: Path, masks: Iterable[str]) -> Iterator[Path]:
        """Yield files in target folder matching any of the provided glob masks"""
        for mask in masks:
            yield from target_folder.glob(mask)

    @staticmethod
    def iter_folders(folder: Path) -> Iterator[Path]:
        """Yields folders from target folder"""
        if not folder.is_dir():
            raise NotADirectoryError(folder)

        for entry in folder.iterdir():
            if entry.is_dir():
                yield entry


if __name__ == '__main__':
    print(f"{os.getcwd()}")
    print(f"{Project.root_folder=}")
    print(f"{Project.models_folder=}")
