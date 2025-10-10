"""Project tools"""

from __future__ import annotations

import os
from pathlib import Path
from typing import Final
from typing import Iterator
from typing import Sequence
from typing import final


@final
class Project:
    """Project tools"""

    root_folder: Final = Path(os.getcwd()).resolve().parent  # Scripts Folder -> Root
    """Project Folder"""

    models_folder: Final = root_folder / "Models"
    """Models Folder"""

    images_folder: Final = root_folder / "Images"
    """Images folder"""

    def __init__(self):
        raise TypeError(f"Cannot create instance of {self.__class__.__name__}")

    @staticmethod
    def search_by_mask_recursive(root_folder: Path, masks: Sequence[str]) -> Iterator[Path]:
        """Yield files in folder (recursively) matching any of the provided glob masks."""
        for mask in masks:
            yield from root_folder.rglob(mask)

    @staticmethod
    def search_by_mask(target_folder: Path, masks: Sequence[str]) -> Iterator[Path]:
        """Yield files in target folder matching any of the provided glob masks"""
        for mask in masks:
            yield from target_folder.glob(mask)
