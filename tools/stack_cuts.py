#!/usr/bin/env python3
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks what each rule of tools/stack.toml cuts against the list the project keeps of it.

A rule of tools/stack.toml says that a call through a pointer never reaches some functions, and
tools/avr_stack.py then leaves those calls out of the deepest stack. The rule is a fact of the
project's programs as they are, and its reason says why it holds. A change of the programs can
make it false without a word: an example that makes a function_ref of a function the rule says
no function_ref is made of has a real call cut, and the bound make stack reports is too low, and
passes.

So what each rule cuts is kept in tools/stack_cuts.txt, and has to be what the last make stack
found, line for line: each call a rule cut, its caller and its callee as avr-c++filt prints
them, with the programs it was cut in. Where a rule cuts something new, or no longer cuts what
it did, make stack fails, and its reason is read again before `make stack FIX=1` writes the list
anew. A rule that cuts nothing in any program says so, as it is stale or covered by another.

tools/stack.sh runs this after every program passed, with the file avr_stack.py --cuts filled.
A project without tools/stack.toml has no rules, and nothing to keep.

Usage:
    tools/stack_cuts.py [--fix] CUTS
"""

import argparse
import difflib
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RULES = ROOT / 'tools' / 'stack.toml'
LIST = ROOT / 'tools' / 'stack_cuts.txt'
HEADER = """\
# What each rule of tools/stack.toml cuts: the calls through a pointer it says never happen, each
# with the programs it is cut in, as make stack found them. make stack fails where it finds other
# cuts, so that the rule's reason is read again; make stack FIX=1 writes what it found.
"""


def listed(rules, cuts):
    """The list of what each rule cuts, as tools/stack_cuts.txt keeps it."""
    # The programs each cut is made in, in the order make stack ran them: for each rule, by
    # caller and callee and the program's name.
    found = {}
    for line in cuts.splitlines():
        name, rule, caller, callee = line.split('\t')
        program, _, label = name.rpartition(' (')
        labels = found.setdefault(int(rule), {}).setdefault((program, caller, callee), [])
        labels.append(label.removesuffix(')'))
    text = HEADER
    for number, rule in enumerate(rules, 1):
        text += f'\nrule {number}: caller = "{rule["caller"]}", callee = "{rule["callee"]}"\n'
        made = found.get(number, {})
        if not made:
            text += '  nothing\n'
        for (program, caller, callee), labels in sorted(made.items()):
            text += f'  {program} ({", ".join(labels)}): {caller} -> {callee}\n'
    return text


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument('cuts', type=Path, help='the file avr_stack.py --cuts filled')
    parser.add_argument('--fix', action='store_true', help='write what was found into the list')
    arguments = parser.parse_args()

    if not RULES.is_file():
        if LIST.is_file():
            print('stack_cuts.py: tools/stack_cuts.txt lists the cuts of rules, and there is no '
                  'tools/stack.toml.', file=sys.stderr)
            if arguments.fix:
                LIST.unlink()
                print('stack_cuts.py: tools/stack_cuts.txt is removed.')
                return 0
            print('`make stack FIX=1` removes it.', file=sys.stderr)
            return 1
        return 0
    # avr_stack.py has read the rules already, and failed on one it could not.
    rules = tomllib.loads(RULES.read_text()).get('never', [])
    found = listed(rules, arguments.cuts.read_text() if arguments.cuts.is_file() else '')
    kept = LIST.read_text(encoding='utf-8') if LIST.is_file() else ''
    if kept == found:
        print('stack_cuts.py: each rule of tools/stack.toml cuts what tools/stack_cuts.txt says.')
        return 0

    diff = difflib.unified_diff(kept.splitlines(keepends=True), found.splitlines(keepends=True),
                                'tools/stack_cuts.txt, as kept', 'as make stack found them')
    sys.stdout.writelines(diff)
    # Before the verdict on stderr, which is not buffered.
    sys.stdout.flush()
    if arguments.fix:
        LIST.write_text(found, encoding='utf-8')
        print('stack_cuts.py: what make stack found is written into tools/stack_cuts.txt.')
        return 0
    print('\nstack_cuts.py: the rules of tools/stack.toml cut other calls than tools/stack_cuts.txt '
          'says. Read the reason of each rule whose cuts changed: does it still hold? Then '
          '`make stack FIX=1` writes what was found.', file=sys.stderr)
    return 1


if __name__ == '__main__':
    sys.exit(main())
