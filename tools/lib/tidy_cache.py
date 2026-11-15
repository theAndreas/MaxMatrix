#!/usr/bin/env python3
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
"""Runs clang-tidy on one file, unless it passed before on exactly the same input.

tools/tidy.sh hands it to run-clang-tidy.py as the clang-tidy to call, with the arguments
clang-tidy takes, the file last. A pass leaves a stamp named by the hash of everything that
decides the result, and the same hash later is a pass again without clang-tidy:

- the file as the compiler sees it: preprocessed by the command of its entry in the database,
  with its comments, as a NOLINT is one, and with every header it reads;
- that command, and the arguments clang-tidy gets: the header filter, the module of our own
  checks;
- the bytes of that module, the version of clang-tidy and the configuration it reads for the
  file, as --dump-config prints it, so that a change of .clang-tidy anywhere above the file
  counts.

The project's folder is replaced by a placeholder before hashing, so that a clone or a worktree
elsewhere hits what another one passed; every path the header filter reads is relative to it.
Only a pass is kept: a finding is found again on every run, and printed. A file without an
entry, or one the compiler cannot preprocess, is checked as without the cache.

The stamps are empty files in $XDG_CACHE_HOME/embedded-cpp-template/tidy, ~/.cache by default,
outside of build/, so that `make clean` keeps them; removing the folder empties the cache.

Usage:
    run-clang-tidy.py -clang-tidy-binary tools/lib/tidy_cache.py ...
"""

import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# What -E must not get from a compile command: the output and the dependency files, which -E
# would write into the project instead of the object's folder.
DROPPED_FLAGS = {'-c', '-MD', '-MMD', '-MP'}
DROPPED_WITH_VALUE = {'-o', '-MF', '-MT', '-MQ'}


def cache_folder():
    base = os.environ.get('XDG_CACHE_HOME') or Path.home() / '.cache'
    return Path(base) / 'embedded-cpp-template' / 'tidy'


def relative(text):
    """The project's folder as a placeholder, in a str or in bytes."""
    if isinstance(text, bytes):
        return text.replace(str(ROOT).encode(), b'<root>')
    return text.replace(str(ROOT), '<root>')


def source_of(arguments):
    """The file to check: run-clang-tidy.py names it last, and asks without one for the checks."""
    if arguments and not arguments[-1].startswith('-'):
        return arguments[-1]
    return None


def option_values(arguments, name):
    prefix = f'{name}='
    return [argument[len(prefix):] for argument in arguments if argument.startswith(prefix)]


def entry_of(database_folder, source):
    """The source's entry in the compilation database, by its whole path."""
    wanted = os.path.normpath(os.path.abspath(source))
    database = json.loads((Path(database_folder) / 'compile_commands.json').read_text())
    for entry in database:
        if os.path.normpath(os.path.join(entry['directory'], entry['file'])) == wanted:
            return entry
    return None


def preprocess_command(entry):
    arguments = entry['arguments'] if 'arguments' in entry else shlex.split(entry['command'])
    command = []
    skip = False
    for argument in arguments:
        if skip:
            skip = False
        elif argument in DROPPED_WITH_VALUE:
            skip = True
        elif argument not in DROPPED_FLAGS:
            command.append(argument)
    return command + ['-E', '-C']


def output_of(command, cwd=None):
    """What the command prints, or None where it fails."""
    result = subprocess.run(command, cwd=cwd, capture_output=True, check=False)
    return result.stdout if result.returncode == 0 else None


def key_of(clang_tidy, arguments, source):
    """The hash of all that decides clang-tidy's result for the source, or None where something
    of it cannot be had."""
    databases = option_values(arguments, '-p')
    if len(databases) != 1:
        return None
    entry = entry_of(databases[0], source)
    if entry is None:
        return None
    command = preprocess_command(entry)
    preprocessed = output_of(command, cwd=entry['directory'])
    configuration = output_of([clang_tidy, '--dump-config']
                              + [f'--config-file={path}'
                                 for path in option_values(arguments, '--config-file')]
                              + [source])
    version = output_of([clang_tidy, '--version'])
    if preprocessed is None or configuration is None or version is None:
        return None
    parts = [relative('\0'.join(arguments)).encode(), relative('\0'.join(command)).encode(),
             relative(preprocessed), configuration, version]
    parts += [Path(module).read_bytes() for module in option_values(arguments, '-load')]
    digest = hashlib.sha256()
    for part in parts:
        digest.update(len(part).to_bytes(8, 'little'))
        digest.update(part)
    return digest.hexdigest()


def main():
    clang_tidy = shutil.which('clang-tidy')
    if clang_tidy is None:
        raise SystemExit('tidy_cache.py: no clang-tidy on the PATH')
    arguments = sys.argv[1:]
    source = source_of(arguments)
    if source is None:
        os.execv(clang_tidy, [clang_tidy] + arguments)
    key = key_of(clang_tidy, arguments, source)
    stamp = cache_folder() / key[:2] / key[2:] if key else None
    if stamp is not None and stamp.exists():
        return 0
    result = subprocess.run([clang_tidy] + arguments, capture_output=True, check=False)
    sys.stdout.buffer.write(result.stdout)
    sys.stderr.buffer.write(result.stderr)
    # A pass is a clean one: a warning that does not fail the run is printed again next time.
    if stamp is not None and result.returncode == 0 and not result.stdout.strip():
        stamp.parent.mkdir(parents=True, exist_ok=True)
        stamp.touch()
    return result.returncode


if __name__ == '__main__':
    sys.exit(main())
