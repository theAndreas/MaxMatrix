#!/usr/bin/env python3
"""Writes the reference from the Doxygen blocks into build/docu/html and fails on a warning.

Every warning fails but two kinds. A member left undocumented below the facade - in the parts
under src/Max7219, src/Matrix, src/Font and src/Text - or in a .cpp, where only the library
reads it. And a missing \\return: a one-line block whose \\brief says what comes back,
"Returns the width of the panel.", needs none, while every parameter needs its \\param.

What a sketch reads, the headers in src/ and src/Command, has to be documented whole; so are
the types the facade takes from the parts, such as MaxMatrixSprite and the orientations,
though this rule cannot tell them from the parts' own members. See docu/Documentation.md.

Doxygen cannot be told to fail on some warnings and not others, so they go into a log and
are sorted here.

Usage:
    tools/docs.py
"""

import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOG = ROOT / 'build' / 'docu' / 'warnings.log'
INTERNAL = re.compile(r'^(src/(Max7219|Matrix|Font|Text)/[^:]+|[^:]+\.cpp):\d+: warning: .* is not documented\.$')
RETURN = re.compile(r'^[^:]+:\d+: warning: return type of member .* is not documented$')


def main():
    LOG.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(['doxygen', 'Doxyfile'], cwd=ROOT, check=True)
    warnings = [line.replace(f'{ROOT}/', '') for line in LOG.read_text().splitlines() if line.strip()]
    failing = [line for line in warnings if not INTERNAL.match(line) and not RETURN.match(line)]
    for line in failing:
        print(line)
    print(f'docs.py: {len(failing)} warnings, {len(warnings) - len(failing)} members undocumented below the facade or returns of one-line blocks')
    return 1 if failing else 0


if __name__ == '__main__':
    sys.exit(main())
