#!/usr/bin/env python3
"""Reports every const that stands left of what it qualifies.

CLAUDE.md puts const right of it, as in char const* and MaxMatrixBatch const&, and no
compiler or clang-tidy check says so. clang-format can move it (QualifierAlignment: Right),
but only while it formats the whole file, which this repository does not; and it left out
const struct { ... } and a declaration of maxMatrixReportError() when the tree was moved
over. So this reads the declarations itself: a const at the start of a declaration, of a
parameter or of a template argument, after one of the keywords that may lead one, and
followed by a name. Comments and string literals are blanked first, keeping their line
breaks, so that prose about const is not reported and the line numbers stay right.

Usage:
    tools/east-const.py              every C++ file git knows of
    tools/east-const.py FILE ...     these files
"""

import re
import subprocess
import sys

WEST_CONST = re.compile(r'(^[ \t]*|[(,<{][ \t]*|\b(?:static|inline|extern|typedef|return|mutable|virtual|'
                        r'explicit|friend|constexpr)[ \t]+)const[ \t]+(?:::)?[A-Za-z_]', re.MULTILINE)
COMMENT_OR_STRING = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\\n])*"|\'(?:\\.|[^\'\\\n])*\'', re.DOTALL)


def blanked(text):
    return COMMENT_OR_STRING.sub(lambda match: re.sub(r'[^\n]', ' ', match.group()), text)


def main():
    paths = sys.argv[1:] or subprocess.run(['git', 'ls-files', '*.cpp', '*.h', '*.ino'], capture_output=True,
                                           text=True, check=True).stdout.split()
    found = 0
    for path in paths:
        text = open(path, encoding='utf-8').read()
        code = blanked(text)
        lines = text.splitlines()
        for match in WEST_CONST.finditer(code):
            line = code.count('\n', 0, match.end()) + 1
            print(f'{path}:{line}: const left of what it qualifies: {lines[line - 1].strip()}')
            found += 1
    return 1 if found else 0


if __name__ == '__main__':
    sys.exit(main())
