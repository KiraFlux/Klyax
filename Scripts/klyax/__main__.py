"""Klyax Project Python script"""

import sys

from klyax.cli import CleanupCommandRunner
from klyax.cli import CommandLineInterface
from klyax.cli import DisplayModelCommandRunner
from klyax.cli import UpdateReadmeCommandRunner


def _start():
    cli = CommandLineInterface((
        CleanupCommandRunner,
        UpdateReadmeCommandRunner,
        DisplayModelCommandRunner,
    ))

    args = cli.parse_args(None)
    command_runner = cli.get(args.mode).create(args)

    try:
        command_runner.run()

    except Exception as e:
        sys.stderr.write(str(e))
        sys.exit(1)


_start()
