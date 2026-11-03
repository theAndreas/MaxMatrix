#!/usr/bin/env python3
"""Builds an example for Wokwi, into the files wokwi.toml names.

arduino-cli names its output after the sketch, Basics.ino.hex, while wokwi.toml names one
file; so the example is built into a directory of its own and copied to build/wokwi/sketch.*,
and wokwi.toml stays the same whichever example runs.

Usage:
    tools/wokwi.py Basics            bit banging, as the examples are configured
    tools/wokwi.py Basics --spi      hardware SPI on the same pins, 11 and 13
"""

import argparse
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUTPUT = ROOT / 'build' / 'wokwi'


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('example', help='the name of a folder in examples/')
    parser.add_argument('--spi', action='store_true', help='build with MAXMATRIX_USE_SPI on')
    arguments = parser.parse_args()

    sketch = ROOT / 'examples' / arguments.example
    if not (sketch / f'{arguments.example}.ino').is_file():
        raise SystemExit(f'wokwi.py: no example {arguments.example} in examples/')
    build = OUTPUT / arguments.example
    command = ['arduino-cli', 'compile', '--fqbn', 'arduino:avr:uno', '--library', str(ROOT),
               '--output-dir', str(build), str(sketch)]
    if arguments.spi:
        command[-1:-1] = ['--build-property', 'compiler.cpp.extra_flags=-DMAXMATRIX_USE_SPI=STD_ON']
    subprocess.run(command, check=True)
    for suffix in ('hex', 'elf'):
        shutil.copyfile(build / f'{arguments.example}.ino.{suffix}', OUTPUT / f'sketch.{suffix}')
    print(f'wokwi.py: {arguments.example}{" with SPI" if arguments.spi else ""} in build/wokwi/sketch.hex')
    return 0


if __name__ == '__main__':
    sys.exit(main())
