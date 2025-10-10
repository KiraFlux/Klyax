"""
Entities
"""
from abc import ABC
from pathlib import Path
from typing import Final
from typing import Iterable
from typing import Optional
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

        self.id: Final = Project.id_separator.join(self.path.relative_to(Project.models_folder).parts)
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

        self.models: Final[Sequence[Model]] = self._load_part_model() + self._load_assembly_unit_models()
        """Models inside"""

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
    def get_part(cls, identifier: str) -> Optional[PartModel]:
        """Get Part model by Identifier"""
        path = cls._get_path(identifier, Project.part_model_extension)

        if path is None:
            return None

        return PartModel(path)

    @classmethod
    def get_assembly_unit(cls, identifier: str) -> Optional[AssemblyUnitModel]:
        """Get Part model by Identifier"""
        path = cls._get_path(identifier)

        if path is None:
            return None

        return AssemblyUnitModel(path)

    @classmethod
    def _get_path(cls, identifier: str, suffix: Optional[str] = None) -> Optional[Path]:
        ret = Path(Project.models_folder).joinpath(identifier)

        if suffix:
            ret = ret.with_suffix("." + suffix)

        if ret.exists():
            return ret

        return None


def _test():
    unit = ModelRegistry.get_assembly_unit("Klyax/Frame-Base")

    return


if __name__ == '__main__':
    _test()
