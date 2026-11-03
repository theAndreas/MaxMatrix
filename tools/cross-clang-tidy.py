#!/usr/bin/env python3
"""Runs clang-tidy over the compile database arduino-cli writes for the Uno.

clang-tidy is clang, and the database is a list of what avr-gcc was told. Read as it is,
clang finds none of the toolchain's own headers - avr-libc, <stdint.h> for the AVR - and
takes the host's instead: every translation unit then stops part-way or is read with a
32-bit int, and what is left over is reported as findings that are not there on the board.

So the database is rewritten first, into a directory of its own, and only then read:

- the target is named, taken from the compiler's own name (avr-g++ -> avr);
- the compiler is asked where its headers are, and those directories are passed instead
  of clang's guesses for the host.

The checks are .clang-tidy's, the same ones the host build is read with. Taken from
Wordclock's tools/cross-clang-tidy.py, without what its ESP32 and Pico builds need.

Usage:
    arduino-cli compile --fqbn arduino:avr:uno --library . --only-compilation-database \\
                        --build-path build/avr examples/SerialControl
    tools/cross-clang-tidy.py build/avr "$PWD/src/"
"""

import argparse
import json
import os
import shlex
import subprocess
import sys
import tempfile
from pathlib import Path


def arguments_of(entry):
    return entry['arguments'] if 'arguments' in entry else shlex.split(entry['command'])


def cxx_compiler(database):
    """The C++ compiler, by full path, as arduino-cli names it."""
    for entry in database:
        compiler = arguments_of(entry)[0]
        if compiler.endswith('g++') and os.path.isabs(compiler):
            return compiler
    raise SystemExit('cross-clang-tidy.py: no C++ compiler with a full path in the database')


def clang_target(compiler):
    """avr-g++ -> avr, a target clang takes as it is."""
    return os.path.basename(compiler).removesuffix('-g++')


def system_includes(compiler):
    """Exactly the list GCC prints for -v, in its order: the C++ library first and the C
    library last, which is the order #include_next depends on."""
    output = subprocess.run([compiler, '-xc++', '-E', '-v', '-'], input='',
                            capture_output=True, text=True, check=True).stderr.splitlines()
    start = output.index('#include <...> search starts here:') + 1
    end = output.index('End of search list.')
    return [os.path.normpath(line.strip()) for line in output[start:end]]


def clang_arguments(arguments, target, includes):
    """-nostdlibinc rather than -nostdinc: it drops the host's /usr/include and keeps
    clang's own builtin headers, which GCC's include directory relies on for stdarg and
    friends the same way clang's does."""
    flags = [flag for directory in includes for flag in ('-isystem', directory)]
    return [arguments[0], f'--target={target}', '-nostdlibinc', '-nostdinc++'] + flags + arguments[1:]


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('build', type=Path, help='the directory holding compile_commands.json')
    parser.add_argument('files', help="which translation units to read, as run-clang-tidy's regex")
    arguments = parser.parse_args()

    database = json.loads((arguments.build / 'compile_commands.json').read_text())
    compiler = cxx_compiler(database)
    target = clang_target(compiler)
    includes = system_includes(compiler)
    print(f'cross-clang-tidy.py: {compiler} as --target={target}', flush=True)

    for entry in database:
        entry['arguments'] = clang_arguments(arguments_of(entry), target, includes)
        entry.pop('command', None)

    with tempfile.TemporaryDirectory() as directory:
        (Path(directory) / 'compile_commands.json').write_text(json.dumps(database))
        return subprocess.run(['run-clang-tidy', '-p', directory, '-quiet', '-j', str(os.cpu_count()),
                               arguments.files]).returncode


if __name__ == '__main__':
    sys.exit(main())
