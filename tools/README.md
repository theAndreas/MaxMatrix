# tools/

The scripts behind the targets of the `Makefile`, which a developer and CI alike call them
through.

## What lies where

| File | What it is |
|---|---|
| `setup.sh` | `make setup`: what each target needs, as its pinned Arduino core, and what `prepare()` of `project.sh` builds |
| `lint.sh` | `make lint` and `make fmt`: clang-format, cpplint, the Doxygen indent, shellcheck, actionlint, ruff |
| `compile_examples.sh` | `make examples`: every example for each target and variant; the table of flash and RAM |
| `documented_sizes.py` | `make sizes`: the README's table against the last `make examples` |
| `self_check.sh` | `make self-check`: examples/SelfCheck in simavr, on each target simavr runs |
| `fuzz.sh` | `make fuzz`: every fuzzer of `test/fuzz/` side by side, `FUZZ_SECONDS` each, from the corpus of the last run |
| `stack.sh` | `make stack`: the deepest stack of each program on each AVR target CMake builds, by `avr_stack.py` |
| `avr_stack.py` | The deepest stack of one AVR program, from avr-gcc's call graphs and its disassembly, against the RAM it leaves; a call through a pointer reaches every function whose address the program takes, but what a rule of `stack.toml` excludes |
| `stack.toml` | The project's, where it has one: what a call through a pointer never reaches, each rule with its reason, for `avr_stack.py` |
| `stack_cuts.py` | `make stack`, after every program passed: what each rule of `stack.toml` cut against `stack_cuts.txt`, which `FIX=1` writes |
| `stack_cuts.txt` | The project's, beside its `stack.toml`: the calls through a pointer each rule cuts, in which programs, as `make stack FIX=1` wrote them |
| `coverage.sh` | `make coverage`: the tests with --coverage, failing on a line, branch or condition of `src/` that no test reaches |
| `tidy.sh` | `make tidy`: clang-tidy, and each target's compilation database |
| `cppcheck.sh` | `make cppcheck` |
| `self_contained_headers.py` | `make headers`: every header compiled on its own |
| `docs.sh` | `make docs`: Doxygen, failing on a gap |
| `arduino_lint.sh` | `make arduino-lint` |
| `common.sh` | What the shell scripts share: the paths, the defaults of `project.sh`, the targets, `select_target()`, the `target_` functions each build system answers, and `download`, which tries a download three times. A script of the project's own sources it as well |
| `project.sh` | The project's: the targets, each with its board and core, the variants and the hooks |
| `project.mk` | The project's: its own targets of the `Makefile` |
| `lib/` | What the scripts above call, a target of none: `arduino_cli.sh`, `cmake.sh` and `platformio.sh`, the build systems of those names, for the scripts that build a target, and `clang_database.py`, which rewrites the database of a CMake or a PlatformIO build for clang-tidy, after `sketch_database.py` has written that of one example of a library built by PlatformIO for each of its sources; `doc_indent.py` and `cpplint_house.py`, cpplint with the NOLINT of a check of our own, for `lint.sh`, `tidy_module.sh` and the clang-tidy module in `tidy_module/` for `tidy.sh`, and `tidy_cache.py`, through which it calls clang-tidy and which skips a file that passed before on the same input |

`project.sh`, `project.mk`, `stack.toml` and `stack_cuts.txt` are the project's own: the template wrote the first
two once, and `copier update` leaves them alone. Everything else here comes from the template and is brought up
to date with `copier update`; a change of the project's own goes into `project.sh`, `project.mk`
or a script of its own beside them.

## Shell or Python

- **Shell drives the tools:** a script that calls arduino-cli, cmake, clang-tidy or simavr,
  passes their flags on and reports their status. Bash, with `set -euo pipefail`; it sources
  `common.sh`.
- **Python reads and writes data:** a compilation database, a table, the text of a source. The
  standard library only, so that it runs without `uv sync`.

shellcheck reads every `.sh` here, with the checks `.shellcheckrc` turns on: the output of a
command goes into a variable before it is used, as `mapfile < <(...)` never sees the command
fail; a list of files comes from `list_files` in `common.sh`. ruff reads every `.py`.
