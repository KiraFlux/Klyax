import sys

sys.path.insert(0, __file__ + "../../Scaffold/Scripts")

from pathlib import Path

from scaffold import Config
from scaffold import Project, ModelInfoJob


def _start():
    klyax_root = Path("../").resolve()
    klyax_config = Config.default(klyax_root)
    klyax_project = Project(klyax_config)

    klyax_quadcopter = klyax_project.get_assembly_unit_model("Klyax-Quadcopter")

    job = ModelInfoJob()
    job.display(klyax_quadcopter)

    return


if __name__ == '__main__':
    _start()
