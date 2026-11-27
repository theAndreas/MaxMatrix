#!/usr/bin/env python3
# Copyright 2026 Andreas Burnickl
# SPDX-License-Identifier: GPL-3.0-or-later
"""Checks that an AVR program's deepest stack fits the RAM the program leaves free.

An AVR has a few KiB of RAM. `.data` and `.bss` take theirs from the bottom, the stack grows
down from the top, and nothing stops the one from running into the other: no MMU, no guard, a
corrupted variable somewhere and a program that does something odd once a week. tools/stack.sh
runs this for every program of every AVR target that CMake builds.

What it adds up, from the call graph avr-gcc writes beside each object when it compiles with
`-fstack-usage -fcallgraph-info=su`, as cmake/toolchains/avr-gcc.cmake has it (an image linked
with -flto takes them at the link, with -save-temps=obj, as LTO inlines across sources; the
image is byte for byte the same with or without them):

- **The deepest path from main()**, each function's frame from the call graph, which counts
  its return address, its saved registers and its locals. The static constructors run before
  main() and are taken as entries of their own.
- **Plus the deepest interrupt handlers that can be on the stack at once.** A classic AVR
  clears the interrupt flag on entering a handler, so one handler at most, as long as none sets
  it again; one that does (`sei`, ISR_NOBLOCK) is refused, as it nests without a bound. An AVR
  of the megaAVR 0 or Dx series has two interrupt levels, so two: one of level 0 and the
  level-1 one on top of it. Taking the deepest, whichever they are, is never less than that.
- **Library code by its disassembly.** avr-libc and libgcc are linked as compiled, so the call
  graph knows their names but not their frames. Those are read off the instructions: every
  push, every `rcall .+0` (avr-gcc's way to reserve a small frame), and the frame a prologue
  cuts out of the stack pointer, plus what they call. And a call into libgcc that the graph
  does not list at all: avr-gcc calls its helpers, as __udivmodsi4 for a division of 32 bits,
  only after it wrote the graph, so each function's own instructions are read for them too.
- **A call through a pointer as a call of every function whose address the program takes**, the
  deepest of them counted. The graph says only that such a call is made, not where it goes, as
  a virtual function, a function_ref or a table of handlers makes it. Where an address is taken
  the linker writes a relocation, in a vtable or in the instructions that load a pointer; linked
  with -Wl,--emit-relocs, as cmake/toolchains/avr-gcc.cmake does, the image keeps them, byte for
  byte the same. An address no function of a size begins at is data, as the bounds of the table
  of constructors, or no pointer at all, as 0.

  That is more than any one call reaches, never less, and at times more than can be: a function
  that calls through a pointer and whose own address is taken, as a trampoline of a function_ref
  or Print::write(), is then taken to call itself, a recursion it never makes. A rule of the file
  --never names says which functions a caller never reaches through a pointer, with its reason,
  in TOML, a caller and a callee as avr-c++filt prints their names, the type a template returns
  first, where * stands for any text. A rule is a fact of the project's programs, not of the
  code in general, and its reason says why it holds there:

      [[never]]
      caller = "* utl::function_ref<*>::Call*"
      callee = "* utl::function_ref<*>::Call*"
      reason = "No function_ref here is made of another, so a trampoline never calls one."

  A rule that does not hold makes the number a lower bound again, which is why each has its
  reason. Where a recursion goes through a pointer, the failure names each such call as a rule
  names it. --cuts adds each call through a pointer a rule cut to a file, for tools/stack_cuts.py,
  which fails where they change: a rule holds of the programs as they are, and one whose cuts
  change has its reason read again.

A call pushes its return address: two bytes, or three on a part with more than 128 KiB of
flash, as avr-gcc's __AVR_3_BYTE_PC__ says of it.

Against the RAM the part has, from avr-libc's header for it, minus `.data` and `.bss` as
avr-size reports them. There is no heap to subtract: this fails if the program calls malloc(),
as a heap is exactly what no static analysis can bound.

It refuses rather than guesses where the sum would not hold: recursion, a call through a
pointer in a program linked without its relocations, a frame of dynamic size, a call into code it
cannot place.
Each would make the number a lower bound, and a lower bound is the one answer this question
cannot use.

Out of Wordclock's tools/avr-stack.py, for the AVR128DA48 alone, made general.

Usage:
    tools/avr_stack.py --mcu atmega328p [--never RULES.toml [--cuts FILE --name NAME]]
                       PROGRAM.elf GRAPH... [--paths]

GRAPH is a .ci file or a folder searched for them.
"""

import argparse
import bisect
import re
import subprocess
import sys
import tomllib
from pathlib import Path

# What a call pushes: set by main() for the part, two bytes or three.
RETURN_ADDRESS_BYTES = 2

# Where the call graph says a call goes through a pointer.
INDIRECT_CALL = '__indirect_call'
# What Stack.resolve() returns for it.
INDIRECT = object()

NODE = re.compile(r'node: \{ title: "([^"]+)" label: "([^"]*)"')
EDGE = re.compile(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"')
FRAME = re.compile(r'(\d+) bytes \((\w+)')
CHUNK = re.compile(r'^([0-9a-f]+) <(.+)>:$')
INSN = re.compile(r'^\s+[0-9a-f]+:\t(\S+)\t?(.*)$')
TARGET_ADDRESS = re.compile(r'; 0x([0-9a-f]+) <')
# A relocation of a program address, gs() or pm(), as avr-readelf -rW prints it: the value of
# its symbol, the symbol, and the addend.
TAKEN = re.compile(r'^\s*[0-9a-f]+\s+[0-9a-f]+\s+R_AVR_\w+_(?:PM|GS)\s+([0-9a-f]+)\s+\S+\s*'
                   r'([+-])\s*([0-9a-f]+)')
RULE_KEYS = {'caller', 'callee', 'reason'}


def fail(message):
    raise SystemExit(f'avr-stack.py: {message}')


def run(command):
    return subprocess.run(command, capture_output=True, text=True, check=True).stdout


def symbol_name(title):
    """The call graph prefixes a function of the image with its object file; a library one not."""
    return title.split(':')[-1]


class CallGraph:
    """The functions LTO compiled, with their frames, and the calls between them."""

    def __init__(self, paths):
        self.frames = {}
        self.labels = {}
        self.calls = {}
        for path in paths:
            text = path.read_text()
            for title, label in NODE.findall(text):
                self.labels[title] = label.split('\\n')[0]
                frame = FRAME.search(label)
                if frame is None:
                    continue
                if frame.group(2) != 'static':
                    fail(f'{self.labels[title]} has a frame of {frame.group(2)} size')
                self.frames[title] = int(frame.group(1))
            for source, target in EDGE.findall(text):
                self.calls.setdefault(source, []).append(target)
        if not self.frames:
            fail('the call graph is empty - was the program compiled with -fcallgraph-info=su?')


class Library:
    """The image's code as avr-objdump prints it, cut at every symbol into chunks.

    By address and not by name: local labels such as .L2 are in hundreds of functions, and a
    chunk found by its name would be somebody else's. A jump's target is the address objdump
    writes beside it.
    """

    def __init__(self, elf):
        self.chunks = {}
        self.names = {}
        current = None
        for line in run(['avr-objdump', '-d', '--no-show-raw-insn', str(elf)]).splitlines():
            header = CHUNK.match(line)
            if header:
                current = int(header.group(1), 16)
                self.chunks[current] = []
                self.names.setdefault(header.group(2), current)
                continue
            insn = INSN.match(line)
            if insn and current is not None:
                self.chunks[current].append((insn.group(1), insn.group(2)))
        self.starts = sorted(self.chunks)
        self.name_at = {address: name for name, address in self.names.items()}
        self.depths = {}

    def chunk_at(self, address):
        """The chunk an address falls in, a jump into the middle of one included."""
        index = bisect.bisect_right(self.starts, address) - 1
        return self.starts[index] if index >= 0 else None

    def frame(self, chunk):
        """What a chunk itself puts on the stack: its pushes, its rcall .+0 and the frame a
        prologue cuts out of the stack pointer - the sbiw or subi/sbci on r28 that follows
        reading it with in r28, 0x3d. An epilogue gives the frame back without reading the
        stack pointer first, so it is not counted a second time."""
        size = 0
        reading_frame = False
        for mnemonic, operands in self.chunks[chunk]:
            immediate = operands.split(',')[1].split()[0] if ',' in operands else None
            if mnemonic == 'push':
                size += 1
            elif (mnemonic == 'rcall') and operands.startswith('.+0'):
                size += 2
            elif (mnemonic == 'in') and operands.startswith('r28, 0x3d'):
                reading_frame = True
            elif reading_frame and (mnemonic in ('sbiw', 'subi')) and operands.startswith('r28'):
                size += int(immediate, 0) & 0xFF
                reading_frame = (mnemonic == 'subi')
            elif reading_frame and (mnemonic == 'sbci') and operands.startswith('r29'):
                size += (int(immediate, 0) & 0xFF) << 8
                reading_frame = False
        return size

    def successors(self, chunk):
        """Calls, which push a return address; jumps and the fall into the next chunk, which
        do not."""
        last = None
        for mnemonic, operands in self.chunks[chunk]:
            last = mnemonic
            if mnemonic in ('icall', 'eicall', 'ijmp', 'eijmp'):
                fail(f'library code at {chunk:#x} calls through a pointer')
            target = TARGET_ADDRESS.search(operands)
            if (target is None) or operands.startswith('.+0'):
                continue
            callee = self.chunk_at(int(target.group(1), 16))
            if (callee is None) or (callee == chunk):
                continue
            if mnemonic in ('call', 'rcall'):
                yield callee, RETURN_ADDRESS_BYTES
            elif mnemonic in ('jmp', 'rjmp') or mnemonic.startswith('br'):
                yield callee, 0
        if last not in ('jmp', 'rjmp', 'ret', 'reti', None):
            index = self.starts.index(chunk) + 1
            if index < len(self.starts):
                yield self.starts[index], 0

    def depth_of(self, name):
        """The deepest a library function goes, its return address left out."""
        return self.depth(self.names[name])

    def depth(self, chunk):
        """The deepest a chunk goes, its return address left out - the caller's to count.

        A loop in library code jumps back to a label, which is a chunk of its own, so the jumps
        between chunks have cycles. Each cycle is taken as one piece whose frame is the sum of
        all of its chunks' - more than any one way round it pushes, never less. A call inside
        such a piece is recursion, and refused.
        """
        if chunk not in self.depths:
            self.measure(chunk)
        return self.depths[chunk]

    def measure(self, root):
        """Tarjan's strongly connected components over the chunks below `root`, deepest first,
        so that every piece is measured after everything it leads to."""
        index, low, on_stack, stack, counter = {}, {}, set(), [], [0]
        edges = {}

        def visit(chunk):
            index[chunk] = low[chunk] = counter[0]
            counter[0] += 1
            stack.append(chunk)
            on_stack.add(chunk)
            edges[chunk] = list(self.successors(chunk))
            for callee, _ in edges[chunk]:
                if callee in self.depths:
                    continue
                if callee not in index:
                    visit(callee)
                    low[chunk] = min(low[chunk], low[callee])
                elif callee in on_stack:
                    low[chunk] = min(low[chunk], index[callee])
            if low[chunk] == index[chunk]:
                piece = []
                while True:
                    member = stack.pop()
                    on_stack.discard(member)
                    piece.append(member)
                    if member == chunk:
                        break
                frame = sum(self.frame(member) for member in piece)
                below = 0
                for member in piece:
                    for callee, pushed in edges[member]:
                        if callee in piece:
                            if pushed:
                                fail(f'library code at {member:#x} calls {callee:#x} recursively')
                            continue
                        below = max(below, pushed + self.depths[callee])
                for member in piece:
                    self.depths[member] = frame + below

        visit(root)


class Stack:
    """The deepest stack below each function, through the call graph and into the library."""

    def __init__(self, graph, library, symbols, taken, never):
        self.graph = graph
        self.library = library
        self.by_name = {symbol_name(title): title for title in graph.frames}
        self.by_address = {}
        for title in graph.frames:
            address = symbols.get(symbol_name(title))
            if address is not None:
                self.by_address.setdefault(address, title)
        self.symbols = symbols
        self.depths = {}
        self.deepest_callee = {}
        self.never = never
        self.taken = self.taken_functions(taken)
        self.demangled = {}
        if self.taken:
            self.demangled = demangle([symbol_name(t) for t in graph.frames] + self.taken)
        # The calls through a pointer followed so far, as (caller, callee) pairs.
        self.through_pointer = set()
        # The calls through a pointer a rule cut, as (rule, caller, callee), the rule by its index
        # and the functions by their names.
        self.cut = set()

    def taken_functions(self, taken):
        """The functions a call through a pointer may reach, those of the graph by their title
        and those of the library by their name: every function at an address of `taken`."""
        functions = []
        for address in sorted(taken):
            if address in self.by_address:
                functions.append(self.by_address[address])
            elif address in self.library.chunks:
                functions.append(self.library.name_at[address])
        return functions

    def name(self, function):
        """A function of the graph or the library as its name reads demangled."""
        name = symbol_name(function)
        return self.demangled.get(name, name)

    def reached_through_pointer(self, title):
        """What a call through a pointer of `title` may reach: every function whose address the
        program takes, but those a rule of --never says it does not."""
        caller = self.name(title)
        for callee in self.taken:
            rule = self.never.rule_for(caller, self.name(callee))
            if rule is None:
                self.through_pointer.add((title, callee))
                yield callee
            else:
                self.cut.add((rule, caller, self.name(callee)))

    def callees(self, title):
        """What `title` calls: functions of the graph by their title, of the library by their
        name."""
        for target in self.graph.calls.get(title, []) + self.unlisted_calls(title):
            callee = self.resolve(target)
            if callee is INDIRECT:
                yield from self.reached_through_pointer(title)
            elif callee is None:
                yield symbol_name(target)
            elif callee != '':
                yield callee

    def refuse_recursion(self, title, active):
        """Fails on the recursion of `title`, naming each call through a pointer it takes, as a
        rule of --never names them, as it may be one that cannot happen."""
        message = f'{self.graph.labels[title]} is recursive: ' + ' -> '.join(
            self.graph.labels[t] for t in active)
        cycle = active[active.index(title):] + (title,)
        hops = [(caller, callee) for caller, callee in zip(cycle, cycle[1:])
                if (caller, callee) in self.through_pointer]
        if hops:
            message += ('.\nThrough a pointer, taken to reach every function whose address the '
                        'program takes; a rule of --never says where one does not:')
            for caller, callee in hops:
                message += f'\n  caller = "{self.name(caller)}"\n  callee = "{self.name(callee)}"'
        fail(message)

    def resolve(self, target):
        """Where a call goes. Identical functions are folded into one, and the graph then
        keeps the node of one of them while calls still name the others; the address they
        share in the image tells which node is meant."""
        if target in self.graph.frames:
            return target
        name = symbol_name(target)
        if name == INDIRECT_CALL:
            return INDIRECT
        if name in self.by_name:
            return self.by_name[name]
        address = self.symbols.get(name)
        if address is not None and address in self.by_address:
            return self.by_address[address]
        if name in self.library.names:
            return None
        # Named in the graph, not in the image: a builtin the compiler expanded in place.
        return ''

    def unlisted_calls(self, title):
        """The library functions a function calls that its call graph does not list: avr-gcc
        calls libgcc's helpers, as __udivmodsi4 for a division of 32 bits, only after it wrote the
        graph. Read off the function's own instructions, as names of the library."""
        address = self.symbols.get(symbol_name(title))
        if address not in self.library.chunks:
            return []
        names = []
        for mnemonic, operands in self.library.chunks[address]:
            target = TARGET_ADDRESS.search(operands)
            if mnemonic not in ('call', 'rcall') or target is None or operands.startswith('.+0'):
                continue
            callee = self.library.chunk_at(int(target.group(1), 16))
            if callee is None or callee == address or callee in self.by_address:
                continue
            names.append(self.library.name_at[callee])
        return names

    def depth(self, title, active=()):
        if title in self.depths:
            return self.depths[title]
        if title in active:
            self.refuse_recursion(title, active)
        deepest, deepest_callee = 0, None
        for callee in self.callees(title):
            if callee in self.graph.frames:
                below = self.depth(callee, active + (title,))
            else:
                below = RETURN_ADDRESS_BYTES + self.library.depth_of(callee)
            if below > deepest:
                deepest, deepest_callee = below, callee
        self.depths[title] = self.graph.frames[title] + deepest
        self.deepest_callee[title] = deepest_callee
        return self.depths[title]

    def path(self, title):
        """The frames along the deepest path below `title`, as (function, bytes) pairs."""
        steps = []
        while title is not None:
            if title in self.graph.frames:
                steps.append((self.graph.labels[title], self.graph.frames[title]))
                title = self.deepest_callee.get(title)
            else:
                steps.append((title + ' (library)', RETURN_ADDRESS_BYTES + self.library.depth_of(title)))
                title = None
        return steps


def preprocess(mcu, text):
    """`text` as avr-gcc's preprocessor leaves it for the part, avr/io.h included."""
    return subprocess.run(['avr-gcc', f'-mmcu={mcu}', '-E', '-P', '-include', 'avr/io.h', '-x',
                           'c', '-'], input=text + '\n', capture_output=True, text=True,
                          check=True).stdout.strip().splitlines()[-1]


def device_ram(mcu):
    """What the part has, as avr-libc's header for it says - not a number kept here: from
    RAMSTART to RAMEND, which every part's header defines, as INTERNAL_SRAM_SIZE only some do."""
    expression = preprocess(mcu, '(RAMEND) - (RAMSTART) + 1')
    expression = re.sub(r'(?<=[0-9a-fA-F])[uUlL]+\b', '', expression)
    if not re.fullmatch(r'[0-9a-fA-Fx()+\-\s]+', expression):
        fail(f'avr-libc does not say how much RAM {mcu} has: {expression}')
    return int(eval(expression, {'__builtins__': {}}))  # noqa: S307 - numbers, + and - only


def predefined(mcu, macro):
    """Whether avr-gcc predefines `macro` for the part."""
    return preprocess(mcu, f'#ifdef {macro}\nyes\n#else\nno\n#endif') == 'yes'


def used_ram(elf, mcu):
    """.data and .bss, as the link rule prints them under Data."""
    out = run(['avr-size', '--format=avr', f'--mcu={mcu}', str(elf)])
    data = re.search(r'Data:\s+(\d+) bytes', out)
    if data is None:
        fail(f'avr-size said something unexpected:\n{out}')
    return int(data.group(1))


def image_symbols(elf):
    symbols = {}
    for line in run(['avr-nm', '--defined-only', str(elf)]).splitlines():
        parts = line.split()
        if len(parts) == 3:
            symbols[parts[2]] = int(parts[0], 16)
    return symbols


def taken_addresses(elf):
    """The addresses of functions the program takes, as its relocations of a program address
    name them: those where a symbol of a size begins, as a function does; the rest is data or
    0. None where it was linked without its relocations."""
    relocations = run(['avr-readelf', '-rW', str(elf)])
    if 'Relocation section' not in relocations:
        return None
    taken = set()
    for line in relocations.splitlines():
        relocation = TAKEN.match(line)
        if relocation is None:
            continue
        value, sign, addend = relocation.groups()
        offset = int(addend, 16) if sign == '+' else -int(addend, 16)
        taken.add(int(value, 16) + offset)
    begins = set()
    for line in run(['avr-nm', '--defined-only', '-S', str(elf)]).splitlines():
        parts = line.split()
        if (len(parts) == 4) and (int(parts[1], 16) > 0):
            begins.add(int(parts[0], 16))
    return taken & begins


def demangle(names):
    """The names given, each mapped to itself demangled, as avr-c++filt reads them."""
    names = sorted(set(names))
    done = subprocess.run(['avr-c++filt'], input='\n'.join(names) + '\n', capture_output=True,
                          text=True, check=True)
    return dict(zip(names, done.stdout.splitlines()))


def pattern(text):
    """A name with * for any text, as a regular expression of the whole name."""
    return re.compile('.*'.join(re.escape(part) for part in text.split('*')))


class Never:
    """The rules of --never: which functions a caller never reaches through a pointer."""

    def __init__(self, path):
        # Each rule's caller and callee as the file writes them, and as patterns.
        self.rules = []
        self.patterns = []
        if path is None:
            return
        try:
            tables = tomllib.loads(path.read_text())
        except (OSError, tomllib.TOMLDecodeError) as error:
            fail(f'{path}: {error}')
        rules = tables.pop('never', [])
        if tables or not isinstance(rules, list) or not all(isinstance(r, dict) for r in rules):
            fail(f'{path}: holds nothing but [[never]] rules, each a table of its own')
        for number, rule in enumerate(rules, 1):
            if set(rule) != RULE_KEYS:
                fail(f'{path}: rule {number} has {", ".join(sorted(rule))}; a rule has a caller, '
                     'a callee and the reason why the one never reaches the other')
            if not all(isinstance(rule[key], str) and rule[key].strip() for key in RULE_KEYS):
                fail(f'{path}: rule {number}: its caller, callee and reason are texts, none empty')
            self.rules.append((rule['caller'], rule['callee']))
            self.patterns.append((pattern(rule['caller']), pattern(rule['callee'])))

    def rule_for(self, caller, callee):
        """The index of the first rule that says `caller` never reaches `callee` through a
        pointer; None where none does."""
        for index, (rule_caller, rule_callee) in enumerate(self.patterns):
            if rule_caller.fullmatch(caller) and rule_callee.fullmatch(callee):
                return index
        return None


def graph_files(paths):
    """The .ci files named, and those below the folders named."""
    files = []
    for path in paths:
        files += sorted(path.rglob('*.ci')) if path.is_dir() else [path]
    return files


def main():
    global RETURN_ADDRESS_BYTES  # noqa: PLW0603 - the part's, set once before anything reads it
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--mcu', required=True, help="the part, as avr-gcc's -mmcu names it")
    parser.add_argument('elf', type=Path, help='the program')
    parser.add_argument('graphs', type=Path, nargs='+', help='its .ci files, or folders of them')
    parser.add_argument('--paths', action='store_true', help='print the deepest paths frame by frame')
    parser.add_argument('--never', type=Path,
                        help='a TOML file of what a caller never reaches through a pointer')
    parser.add_argument('--cuts', type=Path,
                        help='a file to add the calls through a pointer each rule of --never cut '
                        'to, a line each: --name, the rule\'s number, caller and callee, by tabs')
    parser.add_argument('--name', default='', help='what names the program in --cuts')
    args = parser.parse_args()

    if not args.elf.is_file():
        fail(f'no program at {args.elf} - build first')
    graphs = graph_files(args.graphs)
    if not graphs:
        fail(f'no call graph for {args.elf} - it needs -fstack-usage -fcallgraph-info=su')

    symbols = image_symbols(args.elf)
    if 'malloc' in symbols:
        fail('the program calls malloc(): a heap grows towards the stack, and nothing here bounds it')

    if predefined(args.mcu, '__AVR_3_BYTE_PC__'):
        RETURN_ADDRESS_BYTES = 3
    nested_handlers = 2 if predefined(args.mcu, '__AVR_XMEGA__') else 1

    graph = CallGraph(graphs)
    library = Library(args.elf)
    taken = set()
    if any(INDIRECT_CALL in targets for targets in graph.calls.values()):
        taken = taken_addresses(args.elf)
        if taken is None:
            fail('a call goes through a pointer, and the program was linked without the '
                 'relocations that say which addresses it takes: link it with -Wl,--emit-relocs')
    stack = Stack(graph, library, symbols, taken, Never(args.never))

    entries = [t for t in graph.frames
               if symbol_name(t) == 'main' or symbol_name(t).startswith('_GLOBAL__sub_I')]
    handlers = [t for t in graph.frames if re.fullmatch(r'__vector_\d+', symbol_name(t))]
    if not any(symbol_name(t) == 'main' for t in entries):
        fail('main() is not in the call graph')
    if nested_handlers == 1:
        for handler in handlers:
            name = symbol_name(handler)
            if name in library.names and any(mnemonic == 'sei' for mnemonic, _ in
                                             library.chunks[library.names[name]]):
                fail(f'{name} sets the interrupt flag, and handlers can then nest without a bound')

    deepest_entry = max(entries, key=stack.depth)
    deepest_handlers = sorted(handlers, key=stack.depth, reverse=True)[:nested_handlers]
    worst = stack.depth(deepest_entry) + sum(stack.depth(t) for t in deepest_handlers)
    ram = device_ram(args.mcu)
    used = used_ram(args.elf, args.mcu)
    free = ram - used

    print(f'{graph.labels[deepest_entry]}: {stack.depth(deepest_entry)} bytes at the deepest')
    for handler in deepest_handlers:
        print(f'{graph.labels[handler]}: {stack.depth(handler)} bytes on top')
    print(f'worst case {worst} bytes of {free} free ({ram} on the {args.mcu} '
          f'less {used} of .data and .bss): {free - worst} to spare')

    if args.cuts is not None:
        with args.cuts.open('a') as cuts:
            cuts.writelines(f'{args.name}\t{rule + 1}\t{caller}\t{callee}\n'
                            for rule, caller, callee in sorted(stack.cut))

    if args.paths:
        for title in [deepest_entry] + deepest_handlers:
            print(f'\n{graph.labels[title]}:')
            for function, size in stack.path(title):
                print(f'  {size:5d}  {function}')

    if worst > free:
        print('\nThe stack can run into the variables.', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main())
