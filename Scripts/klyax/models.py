"""
Entities
"""
from abc import ABC
from pathlib import Path
from typing import Final
from typing import Iterable
from typing import Sequence
from typing import final

from klyax.project import Project


class Model(ABC):
    """Model"""

    def __init__(self, path: Path) -> None:
        self.path: Final = path
        """Relative to Project Models folder path"""

        self.words: Final[Sequence[str]] = self.name.split(Project.words_separator)
        """Words from filename"""

        self.id: Final = Project.id_separator.join((
            p.name
            for p in self.path.relative_to(Project.models_folder).parents
        )) + Project.id_separator + self.name
        """Model Identifier"""

        self.images: Final[Sequence[Path]] = tuple(self._search_by_extensions(Project.image_extensions))
        """Model Image files"""

    @property
    def name(self) -> str:
        """Model Name"""
        return self.path.stem

    @property
    def folder(self) -> Path:
        """Folder with Model files"""
        return self.path.parent

    @final
    def _search_by_extensions(self, extensions: Iterable[str]) -> Iterable[Path]:
        """Returns entity files with extension"""
        return Project.search_by_masks(self.folder, (
            f"{self.name}*.{e}"
            for e in extensions
        ))


@final
class PartModel(Model):
    """Part Model"""

    def __init__(self, path: Path) -> None:
        super().__init__(path)

        self.transitions: Final[Sequence[Path]] = tuple(self._search_by_extensions(Project.part_transition_extensions))
        """Part Transition files"""


@final
class AssemblyUnitModel(Model):
    """Assembly Unit Model"""

    def __init__(self, path: Path) -> None:
        """
        Assembly Unit Model
        :param path Folder with Unit model file
        """
        if not (path / f"{path.name}.{Project.assembly_unit_model_extension}").exists():
            raise FileNotFoundError("Assembly unit Model file not found")

        super().__init__(path)

        self.entities: Final[Sequence[Model]] = self._load_part_model() + self._load_assembly_unit_models()

    def _load_assembly_unit_models(self):
        return tuple(map(AssemblyUnitModel, Project.iter_folders(self.folder)))

    def _load_part_model(self):
        return tuple(map(PartModel, Project.search_by_masks(self.folder, Project.parts_masks)))

    @property
    def folder(self) -> Path:
        return self.path


@final
class ModelRegistry:
    """Provides access to model objects via ID"""

    def __init__(self):
        raise TypeError(f"Cannot create instance of {self.__class__.__name__}")

    @classmethod
    def get_part(cls, identifier: str) -> PartModel:
        """Get Part model by Identifier"""
        return PartModel(cls._get_path(identifier).with_suffix(Project.part_model_extension))

    @classmethod
    def get_assembly_unit(cls, identifier: str) -> AssemblyUnitModel:
        """Get Part model by Identifier"""
        return AssemblyUnitModel(cls._get_path(identifier))

    @classmethod
    def _get_path(cls, identifier: str) -> Path:
        return Path(Project.models_folder).joinpath(*identifier.split(Project.id_separator))


def _test():
    unit = ModelRegistry.get_assembly_unit("Klyax")

    return


if __name__ == '__main__':
    _test()
