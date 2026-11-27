#!/usr/bin/env python3
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes the compilation database PlatformIO wrote for one example of a library over again, for
every source of the library and of its examples.

PlatformIO builds one sketch at a time, and its database names that sketch, the files of the
framework and those of the library the sketch uses: a source of src/ that no example includes, and
the other examples, are not in it, and clang-tidy would read them with no flags at all. So the
database is replaced by one entry per file given, each with the arguments of the sketch's own
entry, the file PlatformIO made of it as <sketch>.ino.cpp, as arduino-cli's database of a library
is written in tools/lib/arduino_cli.sh:

- its output, -o and the object file, is dropped, and the file named is the one given;
- an include folder outside the repository, the core's and the board's, becomes -isystem, so that
  nothing is reported from headers we do not own, as arduino_flags() of tools/lib/arduino_cli.sh
  makes it: a check that reports in a header, as misc-header-include-cycle, reports there
  whatever HeaderFilterRegex says;
- a sketch, *.ino, is read as C++ with Arduino.h included first, as the Arduino IDE and
  PlatformIO read it before they compile it.

The database stays one of GCC's, which tools/self_contained_headers.py compiles each header with;
tools/lib/clang_database.py rewrites it for clang-tidy after.

Usage:
    tools/lib/sketch_database.py BUILD ROOT FILE...
"""

import argparse
import json
import os
import shlex
import sys
from pathlib import Path


def arguments_of(entry):
    return entry['arguments'] if 'arguments' in entry else shlex.split(entry['command'])


def sketch_entry(database):
    """The entry of the sketch PlatformIO built, the one file it made of a .ino."""
    for entry in database:
        if entry['file'].endswith('.ino.cpp'):
            return entry
    raise SystemExit('sketch_database.py: no sketch, *.ino.cpp, in the database')


def include_flags(folder, directory, root):
    """-I of a folder in the repository, -isystem of one outside it, by its whole path."""
    folder = os.path.normpath(os.path.join(directory, folder))
    inside = os.path.commonpath([folder, root]) == root
    return ['-I' + folder] if inside else ['-isystem', folder]


def flags_of(entry, root):
    """The arguments without the file and the output, the compiler first."""
    flags = []
    skip = None
    for argument in arguments_of(entry):
        if skip == '-o':
            skip = None
        elif skip == '-I':
            skip = None
            flags += include_flags(argument, entry['directory'], root)
        elif argument in ('-o', '-I'):
            skip = argument
        elif argument.startswith('-I'):
            flags += include_flags(argument[2:], entry['directory'], root)
        elif argument != entry['file'] and not argument.endswith('.ino.cpp'):
            flags.append(argument)
    return flags


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('build', type=Path, help='the folder of the compile_commands.json to rewrite')
    parser.add_argument('root', type=Path, help='the repository')
    parser.add_argument('files', nargs='+', type=Path, help='the sources, by their whole path')
    arguments = parser.parse_args()

    path = arguments.build / 'compile_commands.json'
    sketch = sketch_entry(json.loads(path.read_text()))
    flags = flags_of(sketch, os.path.normpath(arguments.root))
    database = []
    for file in arguments.files:
        read_as = ['-x', 'c++', '-include', 'Arduino.h'] if file.suffix == '.ino' else []
        database.append({
            'directory': sketch['directory'],
            'file': str(file),
            'arguments': flags + read_as + [str(file)],
        })
    path.write_text(json.dumps(database, indent=1))
    return 0


if __name__ == '__main__':
    sys.exit(main())
