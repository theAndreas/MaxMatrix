# MaxMatrix: for a coding agent
Drive chained 8x8 LED dot matrix modules with MAX7219. An embedded C++ library for targets from an 8-bit microcontroller to the host:
C++11 as the least standard, without the standard library. This project builds and checks it on
the host by GCC and clang, and for each target of `tools/project.sh` through arduino-cli and simavr.

## Before writing code, read

- [docs/CodingStyle.md](docs/CodingStyle.md): how the code is written. Every rule has an id;
  name it where you deviate, in the `NOLINT` or in the comment that gives the reason.
- [docs/Documentation.md](docs/Documentation.md): how a comment and a Doxygen block are written.
  Everything a sketch can use is documented.
- [docs/Testing.md](docs/Testing.md): which kind of test proves what, and how a test is named
  and written.
- [docs/targets/avr.md](docs/targets/avr.md): what holds on the AVR only, beyond the rules
  that hold for every target.

They hold without exception. Where they and your habits differ, they win.

## How to work

**Quality before speed.** A change that is right and small is worth more than one that is
large and quick. Read the code you change and what calls it before you write; check what you
assume, as an API, a signature or a size, instead of guessing; run every check, not the first;
and where a decision is not yours to make, or the rules leave it open, ask rather than pick.

1. **A test first.** Write the test of the behaviour, see it fail, then write the code.
2. **Small steps.** One concern per change; what you find on the way is a note in your answer,
   not a change of its own.
   A commit holds one concern, and its message says what changed and why.
   A pull request's description says what changes for a user of the library, why, and how it
   was checked; not the code: no function names, no walk through the files, no snippets. The
   diff shows those.
3. **Only what was asked.** No refactoring, renaming or reformatting of code you were not asked
   to touch.

After every step:

```bash
make lint      # clang-format, cpplint, shellcheck, actionlint, ruff: seconds
make tidy      # clang-tidy, for the host and the board
make tests     # GoogleTest on the host
```

`make fmt` formats the code and the Doxygen blocks in place.

## Never

- **Nothing written anew that exists already**: look in the project first, then the standard
  library where every target has it, then the library like it that the project uses, as the
  ETL or Utl (M-3). Only what none has is written, once and for reuse, in a module folder of its
  own with its test; say in your answer what you wrote and why nothing existing would do.
- **No heap, no exceptions, no RTTI**: no `new`, `malloc`, `throw`, `try`, `dynamic_cast`,
  `typeid` (M-1, M-2).
- **No blocking and no `float` without its reason**: no busy wait in the library, no `double`
  at all (H-1, H-7). An interrupt's shared data is read atomically (H-6).
- **No long expressions**: split a computation or a condition into named steps (T-18).
- **No invented API.** A function of the Arduino core or of a library is used only if you have
  seen its declaration; `test/mock/` shows what the tests can call.
- **No code for later**: no unused parameter, function, overload or option "in case", no
  `TODO`, no commented-out code.
- **No comment that repeats the code.** A comment says why, Documentation.md says how.
- **No test weakened to pass**: a failing test is fixed in the code, unless the test is wrong,
  and then you say why in your answer.
- **No check silenced to pass**: no `NOLINT`, `#pragma` or change of `.clang-tidy`,
  `.clang-format`, `CPPLINT.cfg`, the `Makefile` or the CMake files to make a finding go away.
  Where a finding is wrong, suppress it for the one line, naming the check, the rule of
  CodingStyle.md and the reason (S-1), and say so in your answer.
- **No change to the template's files** (below) unless asked.

## Done

A change is done when all of these hold, not when it compiles:

- `make check` passes: it is what CI runs, all of it.
- Every line, branch and condition the change adds is reached by a test: `make coverage` fails
  otherwise and lists the lines missed per file.
- What a sketch can use is documented (Documentation.md), and `make docs` passes.
- The README's table of flash and RAM, if it has one, is what the build measures: after a change
  that moves them, `make examples` and `make sizes FIX=1`.
- What a user of the library notices is in `CHANGELOG.md`, if the project keeps one.
- Your answer names every rule you deviated from, and why.

## Where things are

| What | Where |
|---|---|
| The library | `src/`, the header a sketch includes is `src/MaxMatrix.h` |
| Tests | `test/`, GoogleTest against a mock of the Arduino core in `test/mock/` |
| Examples | `examples/`, sketches; `SelfCheck` runs on the board in simavr |
| The project's settings | `tools/project.sh` (targets, variants, hooks), `tools/project.mk` (own targets) |
| The scripts behind `make` | `tools/`; `tools/README.md` says what each is, and when a script is shell or Python |

`.clang-tidy`, `.clang-format`, `CPPLINT.cfg`, the `Makefile`, `tools/` and `.github/` come from
the template and are updated by `copier update`. Change them only when asked; a project's own
exception goes at the end of a list, with its reason.

## The project's own notes

<!-- What an agent has to know of this project beyond the template: its design, its terms, what
     not to touch. The template wrote this section once; an update keeps what is added here. -->

### Layout

- `src/` — the library, in the Arduino library format 1.5, one part per folder. Everything
  lies in the namespace `max_matrix`, the parts below the facade in `max_matrix::internal`;
  the names below are as the code in the namespace writes them.
  - `MaxMatrix.h` / `MaxMatrix.cpp` — the class `Matrix`, the facade a sketch includes:
    the public API, the state of the library and `Init()`; the rest it passes on.
  - `MaxMatrixConfig.h` — the configuration macros and the size of a module; the length of the
    chain is the sketch's, `MatrixOf<N>` or a buffer handed to `Matrix`.
  - `Max7219/` — `internal::Chain`, the transfer to the chain, and the register macros.
  - `Matrix/` — `internal::Wired`, the buffer as wired, and `internal::Panel` on top of it, the
    coordinates as the viewer sees them.
  - `Font/` — `Font`, the font format of MD_MAX72XX, `kDefaultFont`, the
    lookup of a char's glyph and `DecodeChar()`.
  - `Text/` — `internal::Text`, text that stands or shifts in.
  - `Command/` — `Command`, the lines of text a control page or program sends
    (`text=`, `setDot=`, …), carried out on a `Matrix`.
  - `MaxMatrixError.h` / `.cpp` — `Error`, and `Status` / `Result<T>`,
    what a function that can fail returns, with `GetErrorText()`; `MaxMatrixErrorReport.cpp`, the
    report of `MAXMATRIX_DEV_ERROR_DETECT`, in a file of its own (see "Return values").
  - `MaxMatrixStandardTypes.h` — `STD_ON`/`STD_OFF` and `internal::kNullCharacter`.
  - `MaxMatrixAttributes.h` — `MAXMATRIX_NODISCARD` and `Discard()` of FN-10, which the host
    code uses as well.
- `examples/` — one sketch per folder, compiled by `arduino-cli` and checked by `arduino-lint`.
- `docu/` — the coordinates a sketch uses (`Coordinates.svg`, shown in the README) and how the
  bytes of `matrix_buffer_` lie on a module of each module type (`Wiring_*.svg`). None is
  drawn by hand: `DocuPictures.cpp` draws them from the library and the model, and the test
  `docu/pictures` fails until `build/DocuPictures --write docu` has drawn them again after a
  change that moves a dot. `ErrorHandling.md` records what was weighed, and measured, for the
  errors of OIL item 27. `HardwareCheck.md`
  is what to run on a real matrix before a release or after a change to the transfer code.
- `simulator/` — runs an example sketch unchanged on the host, against the mock and the model,
  and draws the panel as a module of the chosen wiring would light it: in the terminal, or with
  `--web` on `localhost:8080`. One class per job, as in `src/`, and a folder per CMake target
  where the folder's classes are one:
  - `main.cpp` — `setup()` once, `loop()` over and over, and `Tick()`, which `mock::tick` calls.
  - `SimulatorOptions` — the command line.
  - `SimulatorFrontEnd` — the ends both simulators share: the terminal or the page, and the
    serial host; it takes what the sketch printed from the mock once per tick, for all of them.
  - `SimulatorPanel` — the model's LEDs for a wiring and mounting, and whether to draw again.
  - `SimulatorTerminal` — the panel redrawn in place, or printed once at the end of `--for`.
  - `Web/` — `SimulatorWeb`, the JSON for `index.html` beside it (read at run time) and its
    commands; `serial LINE` from the page reaches the sketch's `Serial`, and what the sketch
    prints goes back in the JSON.
  - `Host/` — the library `SimulatorHosts`, the network alone, which the control program's tests
    use as well:
    - `SimulatorWebHost` / `SimulatorWebSocket` — HTTP on loopback and the WebSocket protocol,
      ported from Wordclock; the protocol works on strings only, so it is tested without a
      network.
    - `SimulatorSerialHost` — `--tcp PORT`: the sketch's `Serial` as a plain TCP port on
      loopback, one client at a time, for the control program, which has no WebSocket.
    - `SimulatorSocket` — what both hosts do with a socket: listen on loopback, never wait, send
      all.
  - `Avr/` — `mainAvr.cpp` and `SimulatorAvr`, the executable `SimulatorAvr`: the same simulator
    for the `.elf` `avr-gcc` makes of an example (`--elf`). simavr runs it on an ATmega328P, its
    pins 10, 11 and 13 and its SPI output drive the model, its UART is `Serial`. It writes the
    `mock::` variables the Arduino stand-in writes, so panel, terminal and page work unchanged.
  - `Expected/` — `EXAMPLE.txt`, the panel a test expects of an example at a time CMake names,
    `Basics.txt` one second into `Basics` on an FC-16 board.
  - `Test/` — `WebTest.sh` checks what a browser would get from `--web`, `ControlTest.sh` the
    commands of the page with `SerialControl` (both with the WebSocket of `WebTestLib.sh`),
    `TcpTest.sh` the same commands over `--tcp`, and `PageTest.js` the page's own script, against
    stand-ins for the DOM, the WebSocket and a board on Web Serial.
- `control/` — the control program of OIL item 26, `MaxMatrixControl`: SerialControl on a board
  over its serial port, or in the simulator over `--tcp`, from a wxWidgets window laid out as the
  old Windows tool of `AndreasBur/Led-Matrix`. A CMake project of its own as well, for Windows.
  Classes start with `Control`; everything but the window is the library `Control`, tested
  without wxWidgets:
  - `ControlPort` — one end: `ControlSerialPort` (`termios` / Win32) or `ControlTcpPort`.
  - `ControlLink` — the lines over a port, waiting for `ready` on a board, and asking for the
    `picture` the window shows.
  - `ControlCommand` — the lines `Command` reads, whether one fits its buffer, and the
    answer to `picture`.
  - `ControlWindow`, `ControlDotPanel` — the window and the matrix as SerialControl answers
    `picture`, to click and paint on, checked by hand;
    `.clang-tidy` there switches off the check that argues with wxWidgets' flag enums.
- `diagram.json`, `wokwi.toml` — a Uno with four FC-16 modules for Wokwi, which runs the real
  AVR binary that `tools/wokwi.py EXAMPLE` builds; to look at, not tested in CI.
- `OIL.md` — the open issue list: what is planned, in which order, and why. A PR that works
  on an item updates its row.
- `test/` — host-side tests on GoogleTest against a mock of the Arduino core in `test/mock/`
  and a model of the MAX7219 chain in `test/model/`, built by `CMakeLists.txt` in the root.
  CMake includes the mock as a system header, as the board includes the core, so that nothing
  in it is reported; the model is our own code and is checked as the tests are. The tests of
  the library are a file per header, in folders as in `src/` (`test/Command/CommandTest.cpp`),
  so that clang-tidy reads them side by side; what they share, the fixtures `MaxMatrixTest`
  and `MaxMatrixMappingTest`, `Display()` and the reports, is in `MaxMatrixTestSupport.h`.
  `test/compile_warn/` holds what has to make the compiler warn, a dropped result of a getter,
  compiled under `compile_warn/` in gnu++11 and C++17 and left out by clang-tidy.
- `.github/workflows/` — the template's `ci.yml`, `static-analysis.yml`, `docs.yml` and
  `release.yml`, which run the targets of the `Makefile`, and `avr.yml` of MaxMatrix's own, which
  runs `make avr`: the examples in simavr, as the template's SelfCheck in simavr does for a
  project that has one. The reference is published on GitHub Pages from `main`.
- `.devcontainer/` — the template's; what MaxMatrix needs beyond it, simavr's library for
  `SimulatorAvr` and wxWidgets for the control program, is in `apt-packages.txt`, which CI
  installs as well.

### How the library is built

#### The matrix buffer

`matrix_buffer_` mirrors what is on the LEDs: **one byte per column, one bit per row**
(bit 0 is row 0), columns of all modules in a row, module 0 first — as wired, which for
the module type `kDr0Cr0Rr1` is also how the viewer sees it; `docu/Wiring_*.svg` show all eight.
The buffer is what `get…` functions read — the MAX7219 cannot be read back.

The buffer is the sketch's, and so is the length of the chain: `MatrixOf<N>` brings a buffer
of `BufferSize(N)` bytes in a base class, which is made before the `Matrix` it is
handed to, and `Matrix` takes any buffer and length at run time, as the Arduino IDE passes a
sketch's defines to no library. So nothing in the library may size an array by the chain: what
needs the whole panel works in place, a column at a time, or on a pair of modules, as
`internal::Panel::ChangeOrientation()` does, and `Send()` hands the chain a column of every
module straight from the buffer, a module's columns apart.

Drawing only changes the buffer. `internal::Wired` keeps which columns of a module changed, one
bit each in `changed_columns_`, and `Send()` sends each of them in one frame for all modules:
eight frames at most for a whole picture. The facade decides when: with `auto_update_`, the
default, every public function that draws calls `UpdateIfAuto()` before it returns, so that each
call shows its result; with it off, only `Update()` sends. A new public function that draws
ends with `UpdateIfAuto()` as well, and nothing below the facade sends. `Clear()` marks every
column, even one that was empty already, because after power-up the registers hold anything.
`Batch` turns `auto_update_` off for the life of a block and puts back what it found, so
that a sketch cannot forget to turn it on again; the sketch-facing docs lead with it.

The facade also keeps the pace of a shifting text: `ShiftTask()` returns at once until
`shift_interval_` has passed since `last_shift_`, so a sketch calls it on every `loop()` and times
nothing itself.

#### The parts

Dependencies point one way: `internal::Text` → `Font` and `internal::Panel`,
`internal::Panel` → `internal::Wired` → `internal::Chain` → `Arduino.h` / `SPI.h`. Each part is a
member of the one above it, by value and without virtual functions, so that the parts cost no
RAM of their own; `internal::Text` is handed the panel by every function that draws, rather
than keeping a reference, which cost 86 bytes of flash in `ScrollText` (see its header).

`Command` sits on top of the facade rather than in it: the sketch owns it beside its
`Matrix` and hands it the matrix with every char, as the panel is handed to the text. A sketch
that does not use it pays nothing, because the linker drops what no one calls; `Basics` and
`ScrollText` are the same size with it as without.

#### Two coordinate systems

- **`internal::Panel`** (`SetColumn`, `SetRow`, `SetDot`, `ShiftLeft`, `DrawLine`, …) takes coordinates as
  the viewer sees them.
- **`internal::Wired`** works on the buffer and the hardware as wired and knows nothing about
  orientation. Its functions have the same names; the object says which system is meant.

Between them, `ConvertDotToBuffer()` is the only function that knows the matrix orientation
and the module type: the matrix orientation (0, 180) turns the whole panel, the module type then
says which digit register and bit drive a dot within its module, by the three properties
MD_MAX72XX names its hardware types by (DR, CR, RR), one bit each of `ModuleType`. It
maps one dot as the viewer sees it to a buffer column and bit. Lines go through
`GetLine()`/`SetLine()`; shifting and changing the mapping work on the picture in viewer
coordinates, a column of it at a time.

The shapes, `DrawLine()`, `DrawRect()`, `FillRect()` and `DrawCircle()`, are drawn dot by dot
through `SetDot()`; a column of a bitmap, a glyph or a sprite goes through `DrawColumn()`, which
the text uses as well. Their sums are kept from overflowing an `int`, which is 16 bits on the
Uno: the shapes add up in `int32_t`, which only a sketch that draws them pays for, and a column
returns early where the sum could overflow, as the text should not pay for the shapes.

A new feature that works on coordinates builds on these and never branches on an orientation
or a module type itself. When every function handled the orientations on its own, rows, shifts and dots
disagreed in all but orientation 0.

#### Hardware access

Only `internal::Chain` touches the pins. A transfer to the MAX7219 chain is framed by
`StartTransmission()` and `StopTransmission()`, which drive chip select and, in SPI mode, begin
and end the SPI transaction. Every module in the chain gets one address/value pair per frame.
`WriteRegisterAll()` (one value for every module) and `WriteRegisterEach()` (one value per
module) are the two kinds of frame. Bit banging and SPI are selected with `#if` on
`MAXMATRIX_USE_SPI` in `Transfer()`, and in `Init()` and the members for the two pins only bit
banging uses; both branches have to keep working.

The pins are written through their port registers, not with `digitalWrite()`: `Init()` looks up
each pin's port and bit once (`digitalPinToPort()`, `digitalPinToBitMask()`), and a write takes
`portOutputRegister()` of the port, with the interrupts held off as `digitalWrite()` holds them
off. That made a bit-banged frame four times as fast (measured in simavr). The mock's port
registers are objects that pass every bit on to the pins, so the host tests run the same code.

### Code conventions

[docs/CodingStyle.md](docs/CodingStyle.md) is the standard, as in every project made from the
template; this section says how MaxMatrix's code is written today where it says more or else. Since
version 2.0.0 the library lies in the namespace `max_matrix` (F-9) and its constants are
`constexpr`, as OIL.md records. They hold for `simulator/` and `control/` as much as for `src/`: host code that runs beside
the library is read beside it. Only the tests in `test/` keep a lighter style, a comment at the top
and one above each test.

#### Naming

Google's names, as in Utl and Wordclock, so that every project names alike; clang-tidy's
`readability-identifier-naming` checks them, with the options in `.clang-tidy`:

- Types, functions and methods `PascalCase`: `internal::Panel`, `SetText()`, `GetState()`;
  `Get`/`Set` for accessors, `Convert…`/`Decode…` for conversions.
- Variables and parameters `snake_case`; private and protected members end in `_`
  (`module_type_`), so that a setter's parameter takes the plain name
  (`SetModuleType(ModuleType module_type)`). Members of a
  struct whose members are all public have no `_`.
- Constants, `constexpr` and `const` with static storage, start with `k`: `kErrorTexts`,
  `kDataInPin` in a sketch.
- Enums are `enum class` with values in the same form: `State::kReady`,
  `Error::kOutOfRange`. The indices of a sprite, `kSpriteWidth` and on, are a plain
  enum, as they index an array.
- Macros upper case and prefixed with the library: `MAXMATRIX_USE_SPI`. An include guard
  names the path:
  `MAXMATRIX_SRC_MATRIX_PANEL_H_`.
- The library lies in the namespace `max_matrix` (F-9 of `docs/CodingStyle.md`), so its names
  carry no prefix: `max_matrix::Matrix`, `max_matrix::Font`. What a sketch does not use, the
  parts below the facade, lies in `max_matrix::internal`. A test or a program on the host
  names what it uses with a using-declaration (`using max_matrix::Error;`), never with
  `using namespace`. The simulator is linked with the sketch and has no namespace yet, so its
  classes start with `Simulator`.
- What Arduino names keeps its name: `setup()` and `loop()` in a sketch, and the stand-ins of
  the Arduino core in `test/mock/` (`digitalWrite()`, `Serial`, `SPI`), which carry a NOLINT.
- Files are named exactly as the class they hold (F-3), in `PascalCase`, as Arduino names its own
  (`Arduino.h`, `HardwareSerial.h`, `Servo.h`): `Matrix/Panel.h` for `internal::Panel`,
  `simulator/Host/SimulatorWebHost.cpp`. Google's `snake_case` for files was tried and dropped
  (#45); Utl keeps it, as it stands in for the STL. The files at the root of `src/` keep the
  prefix, against F-3: `src/MaxMatrix.h`, as a sketch includes it by the library's name, and
  `MaxMatrixAttributes.h`, `MaxMatrixConfig.h`, `MaxMatrixError.h`, `MaxMatrixErrorReport.cpp` and
  `MaxMatrixStandardTypes.h`, as Arduino puts `src/` on the include path of every sketch and
  library it builds with MaxMatrix, where an `Error.h` or `Config.h` would meet another's. In a
  folder, the folder's name in the path keeps them apart.

#### Parameters

FN-3 allows four parameters, and the library departs from it where a sketch writer knows the
function by five: the shapes take their coordinates as Adafruit GFX and MD_MAX72XX do,
`DrawLine(x0, y0, x1, y1, value)`, `DrawRect(x, y, width, height, value)` and `DrawBitmap`, and
the constructors of `Matrix`, `Panel` and `Wired` take the three pins, the buffer and its length.
A struct for a point or the pins was weighed and left for 2.0.0: every sketch that draws a shape
would change, for a rule of review. Each carries a `NOLINT(readability-function-size)`; nothing
else in `src/` does.

#### Integer types

Every integer has a fixed width, in `src/`, `simulator/` and `test/` alike: `byte` for an
unsigned 8-bit value, as the Arduino API names it, and `int8_t`, `int16_t`, `uint16_t`,
`uint32_t` and so on for the rest — never `int`, `unsigned`, `short` or `long`. An `int` is 16
bits on the Uno and 32 on the host, so the same code would behave differently in the tests
than on the board. A coordinate that can lie off the panel to the left or above is signed
(`int16_t`); a `uint8_t` or `byte` that goes into a `std::ostream` is cast to `uint16_t` first,
or it prints as a char.

The type stays where an outside interface fixes it: `main(int argc, char** argv)` and an index
into `argv`; `millis()`, `micros()` and `delay()` in the mock, declared with `unsigned long` as
the Arduino core declares them (a value from `millis()` is kept in a `uint32_t`, which is what
it is on the Uno); POSIX file descriptors and `setsockopt()` options (`int`); and `size_t` for
sizes of containers. What `strtoul()` returns is kept in a `uint64_t`, which holds it on every
host, until its range is checked.

#### const

`const` stands right of what it qualifies, in every file alike: `char const* Text`,
`Batch const&`, `std::string const Name`, `static char const kErrorTextOk[] PROGMEM`,
`struct { … } const Names[]`. Read from right to left, the declaration then says what it is,
and `char const* const` needs no second rule. The Arduino core writes `const` on the left; the
mock, which stands in for it, and the examples follow this repository all the same.

clang-format puts it there, `QualifierAlignment: Right` of `.clang-format`, and `make lint`
fails on one left of it.

What a function only reads through a pointer points to `const` (CodingStyle T-5), also where
it keeps the pointer: `SetTextWithShift()` keeps it in a member, and took a `char*` while it
read the text only, so that a sketch could not make its text `const`. No check finds that: the
const checks of clang-tidy take a pointer stored in a member that is not `const` for a write,
so review does.

A text itself is an array, `char const kText[] = "...";` (T-20): the object is the chars, and
only an array takes `PROGMEM`. `char const*` is a pointer that picks a text that exists
already, as `text` of `Message` in `Messages`. Only as a parameter are the two the same, and
there it is written `char const*`, as the function gets a pointer and no array.

#### Constants and configuration

Constants are `constexpr` in the namespace, grouped under a comment; one that only a `.cpp`
uses lies in an anonymous namespace there. The host code, which has no namespace yet, does the
same, but a constant of a header carries its module's name, as a class does:
`kSimulatorWebSocketOpcodeText`. A macro is left for what a sketch or the build sets
with `-D` and what `#if` reads: the configuration parameters; and in the host code for what is
no constant expression, a `wxColour`. Configuration switches are
`STD_ON`/`STD_OFF` and are tested as `#if(MAXMATRIX_USE_SPI == STD_ON)`. Register addresses
and values of the MAX7219 get a named constant in `max7219` rather than a literal, as
`Matrix::RegisterWrite()` takes them: `max7219::kIntensityAddress`.

Every configuration parameter is wrapped in `#ifndef`, because that is how the tests build the
SPI variant (`-DMAXMATRIX_USE_SPI=STD_ON`). A new parameter gets the same guard.

#### Return values

A function of the library that can fail says why, in `Error`
(`src/MaxMatrixError.h`): it returns a `Status`, or a getter of a single value a
`Result<T>` with the value and the error side by side. Neither converts to `bool`, as
`E_OK` was 0 and `if(matrix.SetText(…))` was true on an error; code asks `IsOk()` or compares
with an error. Only an array still goes through a parameter (`GetRow()` of the whole panel by
reference). A new reason gets its own value in the enum, a text in `kErrorTexts`
(`src/MaxMatrixError.cpp`) and a row in the test `EveryErrorSaysWhy`, which checks every way a
call can fail and that it changes nothing. `Command::Feed()` answers a failed line with
`GetErrorText()` on the `Print` it is given, and returns the reason as well.

A result whose drop is a mistake is `MAXMATRIX_NODISCARD` (FN-10): a getter's, whose value would
be lost, and the `Status` a part below the facade passes on. A setter's `Status`, as of
`SetText()`, is not, nor what `Command::Feed()` returns: a sketch drops them when it has no one
to tell, and the examples do. Where the library drops a marked result on purpose, it hands it to
`Discard()` with the reason beside it.

The texts lie in a table in `PROGMEM`, not behind a `switch`: avr-gcc turns a `switch` that
returns a constant per case into a table in `.rodata`, which the Uno copies into RAM, as it does
with every constant not marked `PROGMEM`.

Coordinates and indices are range checked before the buffer is touched. A function that draws
leaves out what lies off the panel rather than failing, because drawing partly off the panel is
what a moving sprite or text does all the time (see `internal::Panel::SetDot()`); `SetDot`,
`SetColumn` and `SetRow` cannot fail at all and return `void`, while `SetChar` and `SetText`
still fail on a char the font lacks. A getter or a setting out of range returns
`Error::kOutOfRange` and changes nothing. `Command` checks the coordinates it is
sent itself, so that a typed line that would do nothing says so.

With `MAXMATRIX_DEV_ERROR_DETECT`, every function of the facade that fails reports its name and
the error to `ReportError()`, as the DET of AUTOSAR Classic does. A new function of the
facade that can fail wraps the call of the part below in `MAXMATRIX_REPORT_ERROR("name", …)`,
which is the bare call when the switch is off; the parts below the facade and `Command`
report nothing. The weak default, which prints on `Serial`, lies in `MaxMatrixErrorReport.cpp`,
and `library.properties` links the library as an archive (`dot_a_linkage`): linked as loose
objects, the default pulled `Serial` into a sketch that defines its own report, 175 bytes of RAM
on the Uno.

Host code whose failures no sketch sees, the simulator and `DocuPictures`, returns `bool`, `true`
for success: each of its functions fails for one reason, which its `\return` names beside `false`.
`Error` has no words for a port that is taken, and `std::optional` is C++17, while the
simulator is built as `gnu++11` with the library.

#### Layout of a source file

Every file of `src/`, `simulator/`, `control/` and `docu/` starts with the copyright, the SPDX line and a
`\file` block, as the files of Utl do; then come the includes and the code, with no banners
between them and no name after a closing brace. The format is clang-format's: `make fmt`.

How the code is documented is in [docs/Documentation.md](docs/Documentation.md), the style of
Utl: a Doxygen block at each declaration, in the header, for the one who uses it; a comment on the
implementation, `//` for one or two lines and `/* ... */` from three, where the code it explains
is; what a sketch reads documented whole, and how `make docs` and clang's `-Wdocumentation`
check it.

An include in `src/` names a header by its path from `src/` (`"Font/Font.h"`), or a
header in the same folder by its name alone, never with `../`. Both builds put `src/` on the
include path: `arduino-cli` for a library in format 1.5, and `CMakeLists.txt`. The simulator
does the same from `simulator/` (`"Host/SimulatorSerialHost.h"`), in its own files and in the
tests. A header includes what it uses itself, rather than relying on a header included before
it.

#### Comments

Comments say *why*, not what — the reasoning that is not recoverable from the code, such as
which alternative was rejected and what went wrong with it. Keep a comment where the code it
explains is, as `docs/Documentation.md` says: in the body, at the private member, or at the
definition in the `.cpp`, never between a Doxygen block and its declaration.

### Encoding and umlauts

All files are **UTF-8**, as `.editorconfig` says. The Arduino IDE saves sketches as UTF-8 as
well, so an `ä` in a user's text arrives as the two bytes `0xC3 0xA4`.

- **Never compare against a character literal such as `'ä'`.** In a UTF-8 source file it is a
  two-byte multi-character constant that never equals a `char`. Compare against the latin-1
  code (`0xE4`) and put the character in a comment beside it, as `kDefaultFont` does.
- Text goes through `DecodeChar()`, which turns a two-byte UTF-8 sequence into one latin-1
  char before the glyph lookup. A new function that walks a string uses it too.
- A font holds its chars at their latin-1 codes, a char it lacks as a width of 0: a char added
  to `kDefaultFont` takes the place of the 0 at its code.

### Build and check

`make` lists every target; `make check` is what CI runs, all of it, `make avr` among it. Beyond
the template's targets:

```bash
make avr                      # every example for the Uno, in simavr, against simulator/Expected/
build/host/DocuPictures --write docu   # draws docu/ again after a change that moves a dot
tools/wokwi.py EXAMPLE        # an example for Wokwi, see wokwi.toml
```

Compile every example after a change to the library, not just the one that uses the change:
the examples are the only code that instantiates the class with the real AVR toolchain.
`make examples` compiles each in the three variants of `tools/project.sh`, bit banging, SPI and
with `MAXMATRIX_DEV_ERROR_DETECT`, and fails on a warning in this repository; warnings from the
Arduino core are ignored.
#### Tests

`make tests` configures into `build/host`, builds the library three times, with bit banging,
with SPI and with `MAXMATRIX_DEV_ERROR_DETECT` on, and runs the same tests on each through
`ctest`, each test under a `bitbang/`, `spi/` or `det/` prefix; the simulators are built for the
first two only. In `det/`, `MaxMatrixTestSupport.cpp` defines `ReportError()` itself and records
into `reports`, which stays empty in the other two, so that a test expects reports in every variant. The library and the mock are built as `gnu++11`, like `avr-gcc` builds a sketch;
only the tests are C++17, which GoogleTest needs. A test that loops over the orientations and
module types is a `TEST_P` instead. The mock follows the pins as the chain sees them, given by `mock::Reset()`:
a falling chip select starts a frame, a rising clock takes the level of data in as a bit, and a
rising chip select latches the frame, as SimulatorAvr does for the real binary. It records every
frame in `mock::last_frame` and counts in `mock::errors` a bit or an SPI byte outside a frame or
an SPI transaction, a frame that ends within a byte, and a port written with the interrupts on;
every library test ends by checking that there are none and that the interrupts are on again.

Every bit and every rising chip select also drive `mock::chain`, a model of the MAX7219 chain
(`test/model/Max7219.h`): it shifts like the hardware and each chip latches its register on the
rising edge, so a frame that is too short or a wrong address shows on the model's LEDs.
`Display()` in the tests reads them back laid out like `matrix_buffer_`, and every library test
ends by checking that the two agree. A test of what is lit asks the model, not the buffer. The
model has its own tests against the datasheet, run once under the `model/` prefix.

`mock::chain.IsLit()` goes one step further, to the LEDs as the viewer sees them: given one of
the eight module wirings MD_MAX72XX names (`DR0CR0RR0` to `DR1CR1RR1`, `FC16`, `GENERIC`, …)
and whether the board is mounted upside down. `MaxMatrixModuleTest` pins that every module
type shows the picture upright on the wiring of its name, and the named four on the wirings
MD_MAX72XX gives those names; the README's table says the same and changes with it.

The mock has a `Serial` that reads `mock::serial_input` and appends what the sketch prints to
`mock::serial_output`, so that `SerialControl` builds into a simulator as well; `F()`, `PSTR()`,
`strcmp_P()` and `pgm_read_ptr()` take strings in RAM there, as the host has no separate flash, but
`F()` keeps the type `__FlashStringHelper const*` of the core.
The mock also has `millis()`, `micros()` and `delay()` on the host clock, for the simulator;
`delay()` really waits. A test that times something holds the clock with `mock::SetMillis()`
instead, until the next `mock::Reset()`; `delay()` then moves it on without waiting. The library
tests set the shift interval to 0 in `SetUp()`, so that every `ShiftTask()` is a step, and test
the timing on its own. `mock::tick`, called after every frame and every millisecond of
`delay()`, is how the simulator draws while a sketch blocks. The tests leave it unset. The tests
`simulator/run/<example>` run every example for two seconds, which fails if a frame reaches the
chain broken; what they draw depends on the host's clock, so it is not compared. The test
`simulator/Basics` runs the `Basics` example for a second and compares the panel with
`simulator/Expected/Basics.txt`: the whole way from a sketch to the LEDs of an FC-16 board. The test
`simulator/web` runs it with `--web` on port 18080 and stands in for the browser with `curl` and
a WebSocket over bash's `/dev/tcp`: the page, the handshake, the panel after `hello`, and a
change of wiring. It connects while `Basics` shows "Hi!" and sends nothing, so that only an answer
to the page can bring a panel; a new example in its place needs such a pause as well. The test
`simulator/control` runs `SerialControl` on port 18082 and sends it `serial` lines as the page
does, expecting `ok` and `error` back in the JSON; `simulator/tcp` sends the same lines over
`--tcp` on port 18084, as the control program does, and reads the answers as lines. `simulator/page` runs `PageTest.js` under
Node, once; CMake leaves it out where there is no `node`, as the dev container's `Dockerfile`
installs none. A fake port there refuses to close while a stream is locked, as Chrome does.
`SimulatorWebSocketTest` checks the rest of the protocol a browser may use, against the examples
of the RFCs, `SimulatorWebHostTest` that a client which connects and says nothing cannot stop
the host (on port 18081, so it needs no lock with `simulator/web`), and `SimulatorTest` the
command line and the status line against the model, all once under the `simulator/` prefix: 
all of the simulator but `main.cpp` is one library, built once, because it reads the model and 
never the library.
`SimulatorSerialHostTest` checks what a client of `--tcp` may do that the sketch never sees,
connect a second time and come back, on port 18087.

The control program's classes without a window are tested once under the `control/` prefix:
`ControlCommandTest` the lines, `ControlLinkTest` the waiting for `ready` and the cutting into
lines against a port that is a string each way, and `ControlPortTest` the serial port against a
pseudo terminal from `openpty()` and the TCP port against `SimulatorSerialHost`, on port 18086.
Each test binary that opens a port holds a `RESOURCE_LOCK` of its own name, as ctest runs every
test of it as a process of its own. The window is checked by hand: under Xvfb with
`GDK_BACKEND=x11`, as GTK otherwise opens it on WSLg's Wayland desktop.

The tests under `avr/` run what the Uno runs: CMake builds every example with `arduino-cli`, with
bit banging and with SPI, into `build/avr/`, and `SimulatorAvr` runs each `.elf` for a simulated
time and compares the panel with `simulator/Expected/EXAMPLE.txt`, the same file for both transfer paths.
The time is simulated, so a moving example ends with the same panel every run, which the
simulator on the host clock cannot promise. `avr/*/control` is `simulator/control` against the
`.elf` of `SerialControl`, on port 18083, and `avr/*/tcp` is `simulator/tcp`, on port 18085; with
`--web` or `--tcp`, `SimulatorAvr` keeps to the host's clock, as the page and the program do. CMake builds them only with `MAXMATRIX_AVR_TESTS`, which `make avr` sets in
`build/avr-tests/`, and `avr.yml` runs them in CI. A new example gets a line in
`AvrRuns` in `CMakeLists.txt` and its `.txt`, written from a run of `SimulatorAvr` whose panel
has been looked at.

**A bug fix comes with a test that fails without the fix.** Put the bug back and run the
tests before committing; a test that stays green then does not test the bug. The comment above
the test names the bug it guards against.

The tests reach private members through `#define private public`, because some of what they
check has no public path — which frame a change to one buffer column turns into, for instance,
is checked by storing the bit with `internal::Wired::StoreBit()`, so that the test names the
buffer column itself rather than reaching it through an orientation. The parts are private
members of `Matrix`, so a test reaches the buffer as `matrix_.panel_.wired_.matrix_buffer_`. 
The define comes after the GoogleTest and standard headers, which it would rewrite as well.

#### Hardware

Nothing here reaches real hardware. The tests under `avr/` run the transfer code as the Uno
runs it, but against the model, not against a MAX7219 and its timing on a real wire. A change
to the transfer code (chip select, SPI, bit banging) is untested until it has run on a matrix,
and the pull request says so; `docu/HardwareCheck.md` lists what to run there.


### Delivering a change

that way rather than the obvious alternative, and what was measured. A body that repeats the
diff is worth nothing to a reviewer; the numbers — flash and RAM of the examples on the Uno,
which examples were compiled, whether it ran on hardware — are the part nobody can
reconstruct from the code. `make examples` prints flash and RAM of every
example, and CI writes them into its job summary.
