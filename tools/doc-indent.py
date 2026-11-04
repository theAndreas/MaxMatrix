#!/usr/bin/env python3
"""Indents the lines that continue a command of a Doxygen block, or reports where they are not.

docu/Documentation.md starts a line that continues \\param, \\return, \\pre and the others where
the text after the command starts:

     * \\return The column, one bit per row, or MaxMatrixError::kOutOfRange if `column`
     *         lies off the panel.

With --fix, the paragraph after each command is rewrapped to 100 columns so; a code span in
backticks is kept whole, and \\code ... \\endcode and the lines of a list are left as they are.
Without it, the files are only read, and a file in which a line is not so fails. The tool from
Utl, tools/doc_indent.pl, does the same there after clang-format.

The headers and sources of src/, simulator/ and docu/ are read; the tests keep their // comments.

Usage:
    tools/doc-indent.py [--fix]              every header and source of these git knows of
    tools/doc-indent.py [--fix] FILE ...     these files
"""

import re
import subprocess
import sys

WIDTH = 100
FOLDERS = ('src', 'simulator', 'docu')
COMMAND = re.compile(r'^(\s*)\* (\\(?:param(?:\[[a-z,]+\])? +\S+|tparam +\S+|retval +\S+'
                     r'|return|pre|post|note|warning|see|brief)) +(\S.*)$')
CONTINUED = re.compile(r'^\s*\* +(\S.*)$')
WORD = re.compile(r'(?:`[^`]*`|[^\s`])+')


def indented(lines):
    """Returns the lines with every command's paragraph indented under its text."""
    out = []
    in_block = in_code = False
    i = 0
    while i < len(lines):
        line = lines[i]
        if re.match(r'^\s*/\*!(\s|$)', line) and '*/' not in line:
            in_block = True
        if not in_block:
            out.append(line)
            i += 1
            continue
        if re.match(r'^\s*\*/', line):
            in_block = False
            out.append(line)
            i += 1
            continue
        if '\\code' in line:
            in_code = True
        if in_code:
            in_code = '\\endcode' not in line
            out.append(line)
            i += 1
            continue
        match = COMMAND.match(line)
        if not match:
            out.append(line)
            i += 1
            continue
        margin, command, text = match.groups()
        # One space between a command, its direction and its name; nothing is aligned in columns.
        command = re.sub(r' +', ' ', command)
        # The paragraph goes on to an empty line, the next command, a list or the end of the block.
        j = i + 1
        while j < len(lines):
            more = CONTINUED.match(lines[j])
            if not more or re.match(r'^\s*\* +(\\|- )', lines[j]) or re.match(r'^\s*\*/', lines[j]):
                break
            text += ' ' + more.group(1)
            j += 1
        first = f'{margin}* {command} '
        continued = f'{margin}* ' + ' ' * (len(command) + 1)
        current = first
        empty = True
        for word in WORD.findall(text):
            if not empty and len(current) + 1 + len(word) > WIDTH:
                out.append(current + '\n')
                current = continued + word
            else:
                current += ('' if empty else ' ') + word
            empty = False
        out.append(current + '\n')
        i = j
    return out


def main():
    arguments = sys.argv[1:]
    fix = '--fix' in arguments
    files = [argument for argument in arguments if argument != '--fix']
    if not files:
        patterns = [f'{folder}/*.{suffix}' for folder in FOLDERS for suffix in ('h', 'cpp')]
        files = subprocess.run(['git', 'ls-files', *patterns], capture_output=True, text=True,
                               check=True).stdout.split()
    failed = False
    for path in files:
        with open(path, encoding='utf-8') as source:
            lines = source.readlines()
        result = indented(lines)
        if result == lines:
            continue
        if fix:
            with open(path, 'w', encoding='utf-8') as source:
                source.writelines(result)
        else:
            print(f'{path}: a line that continues a Doxygen command is not indented under its text;'
                  ' tools/doc-indent.py --fix indents it')
            failed = True
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main())
