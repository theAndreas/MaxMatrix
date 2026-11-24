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
    the public API, the state of the library and `Init()`; the rest it passes on. `MatrixOf`
    and its `internal::Buffer` stay beside it, a Matrix with its buffer, as `MatrixOf` derives
    from it and a header of its own would make an include cycle with the one that exports it.
  - `MaxMatrixBatch.h` / `.cpp` — `Batch`, which holds the sending back for a block.
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
  - `Interval/` — `Interval`, when the next step of something that moves is due, from the clock
    a sketch hands it; a shifting text takes one step per `Matrix::ShiftStep()`.
  - `MaxMatrixError.h` / `.cpp` — `Error`, and `Status` / `Result<T>`,
    what a function that can fail returns, with `GetErrorText()`; `MaxMatrixErrorReport.cpp`, the
    report of `MAXMATRIX_DEV_ERROR_DETECT`, in a file of its own (see "Return values").
  - `MaxMatrixStandardTypes.h` — `STD_ON`/`STD_OFF` and `internal::kNullCharacter`.
  - `MaxMatrixAttributes.h` — `MAXMATRIX_NODISCARD` and `Discard()` of FN-10, which the host
    code uses as well.
  - `Utility/` — what neither the core nor a library gives (M-3): `Byte`, a byte of raw data
    as `std::byte`; `array` and `make_array()`, an array that knows its size as T-20 asks, and
    its size taken from its values; `internal::reference_wrapper`, a member that refers to
    an object another owns, as FN-15 asks; and `Bit`, the one class whose functions use the
    bitwise operators, as H-9 asks, in `Utility/Bit.h` rather than the `max_matrix/Bit.h` the
    rule names, beside the others. avr-gcc lacks all four.
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
  - `SimulatorRun` — how a run of either simulator starts, from the command line with the chain
    every example has, and ends, with its exit code; `main.cpp` and `mainAvr.cpp` share it.
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
      all, and tell a connection that is gone.
    - `SimulatorFile` — a whole file read into a string, the page the web host serves, the
      panel `--expect` names and the pictures `DocuPictures --check` compares.
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
  - `ControlPort` — one end: `ControlSerialPort` (`termios` / Win32) or `ControlTcpPort`;
    `ControlStream` holds the loops that send all and read what has arrived, which the serial port
    on Linux and the TCP port share.
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
  The tests of what is not the library lie in folders of their own (F-13): the simulator's in
  `test/Simulator/`, the control program's in `test/Control/`, and the model's beside it in
  `test/model/`.
  `test/compile_warn/` holds what has to make the compiler warn, a dropped result of a getter,
  compiled under `compile_warn/` in gnu++11 and C++17 and left out by clang-tidy.
  `test/fuzz/` holds `CommandFuzz`, the fuzzer of `Command`, the one parser of what comes from
  outside (TS-28): every input fed to a matrix of its own, and what a program at the other end
  relies on checked, every frame whole and every answer one it knows; beside it the corpus it
  starts from, the words of its dictionary and its options. `make fuzz` builds and runs it.
- `.github/workflows/` — the template's `ci.yml`, `static-analysis.yml`, `docs.yml` and
  `release.yml`, which run the targets of the `Makefile`, and two of MaxMatrix's own: `avr.yml`,
  which runs `make avr`, the examples in simavr, as the template's SelfCheck in simavr does for a
  project that has one; and `boards.yml`, which runs `make boards` for the Uno R4, the Pico and
  the ESP32, a job and a cache each, as their cores take 8 GB. The reference is published on GitHub Pages from `main`.
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
handed to, and `Matrix` takes any buffer at run time, as a span whose length says how many
modules the chain has, as the Arduino IDE passes a sketch's defines to no library. So nothing
in the library may size an array by the chain: what needs the whole panel works in place, a
column at a time, or on a pair of modules, as `internal::Panel::ChangeOrientation()` does,
and `Send()` hands the chain the buffer and a column, whose values lie a module's columns
apart.

Drawing only changes the buffer. `internal::Wired` keeps which columns of a module changed, one
bit each in `changed_columns_`, and `Send()` sends each of them in one frame for all modules:
eight frames at most for a whole picture. The facade decides when: with `auto_update_`, the
default, every public function that draws calls `UpdateIfAuto()` before it returns, so that each
call shows its result; with it off, only `Update()` sends. A new public function that draws
ends with `UpdateIfAuto()` as well, and nothing below the facade sends. `Clear()` marks every
column, even one that was empty already, because after power-up the registers hold anything.
`Batch` turns `auto_update_` off for the life of a block and puts back what it found, so
that a sketch cannot forget to turn it on again; the sketch-facing docs lead with it.

The library reads no clock (H-2, AVR-4): `ShiftStep()` shifts a text one column on every call, and
the sketch decides when, as a rule with an `Interval` (`src/Interval/`), which takes the clock it is
given and says when the next step is due. `Command` sets that `Interval` for `speed=`, as the sketch
hands it to `Feed()` beside the matrix. Wordclock does the same: its platform reads the clock, its
firmware counts ticks.

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
Uno: the line and the circle add up in `int32_t`, which only a sketch that draws them pays for,
a rectangle walks only the part of it on the panel, and a column returns early where the sum
could overflow, as the text should not pay for the shapes.

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
banging uses; both branches have to keep working. With SPI the chain keeps the bus instead, the
global `SPI` or the one a sketch names, `MatrixOf<4>{SPI1, ChipSelectPin{13}}`, for a board with
two (OIL 47); DIN and CLK are then the bus's, and the pin constructor takes them only so that a
sketch builds both ways. The mock has `SPI` and `SPI1`, and `mock::chain_bus` says which the chain
is wired to: a call on the other counts as an error.

The pins are written through their port registers, not with `digitalWrite()`: `Init()` looks up
each pin's port and bit once (`digitalPinToPort()`, `digitalPinToBitMask()`), and a write takes
`portOutputRegister()` of the port, with the interrupts held off as `digitalWrite()` holds them
off. That made a bit-banged frame four times as fast (measured in simavr). The mock's port
registers are objects that pass every bit on to the pins, so the host tests run the same code.
The port registers are the AVR's alone, `MAXMATRIX_USE_PORT_REGISTERS`, on by default where
`ARDUINO_ARCH_AVR` is, as the mock defines it: every other board writes its pins through
`digitalWrite()`, which every core has, and the variant `portable` tests that way on the host and
compiles it for the Uno (OIL 46).

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
  `Error::kOutOfRange`.
- Macros upper case and prefixed with the library: `MAXMATRIX_USE_SPI`. An include guard
  names the path:
  `MAXMATRIX_SRC_MATRIX_PANEL_H_`.
- The library lies in the namespace `max_matrix` (F-9 of `docs/CodingStyle.md`), so its names
  carry no prefix: `max_matrix::Matrix`, `max_matrix::Font`. What a sketch does not use, the
  parts below the facade, lies in `max_matrix::internal`. A test of the library lies in the
  namespace `max_matrix` and an anonymous one inside it, as TS-6 of `docs/Testing.md` asks; a
  program on the host names what it uses with a using-declaration (`using max_matrix::Error;`),
  never with `using namespace`. The simulator is linked with the sketch and has no namespace
  yet, so its classes start with `Simulator`.
- `Byte` stands in for `std::byte`, and is named `Byte` against the names of
  `docs/CodingStyle.md`, which give such a type the standard's name: a `max_matrix::byte` would
  meet Arduino's `byte` in a sketch that is `using namespace max_matrix` (OIL 41).
- What Arduino names keeps its name: `setup()` and `loop()` in a sketch, and the stand-ins of
  the Arduino core in `test/mock/` (`digitalWrite()`, `Serial`, `SPI`), which carry a NOLINT.
- Files are named exactly as the class they hold (F-3), in `PascalCase`, as Arduino names its own
  (`Arduino.h`, `HardwareSerial.h`, `Servo.h`): `Matrix/Panel.h` for `internal::Panel`,
  `simulator/Host/SimulatorWebHost.cpp`. Google's `snake_case` for files was tried and dropped
  (#45); Utl keeps it, as it stands in for the STL. The files at the root of `src/` keep the
  prefix, against F-3: `src/MaxMatrix.h`, as a sketch includes it by the library's name, and
  `MaxMatrixAttributes.h`, `MaxMatrixBatch.h`, `MaxMatrixConfig.h`, `MaxMatrixError.h`,
  `MaxMatrixErrorReport.cpp` and `MaxMatrixStandardTypes.h`, as Arduino puts `src/` on the
  include path of every sketch and library it builds with MaxMatrix, where an `Error.h` or
  `Config.h` would meet another's. In a
  folder, the folder's name in the path keeps them apart.

#### Parameters

FN-3 allows four parameters, and nothing in `src/` takes more, as a dot is one `Point`: the
shapes take two points or a point and a radius, `DrawLine(from, to, value)`,
`DrawRect(corner, opposite_corner, value)`, `DrawCircle(centre, radius, value)`;
`DrawBitmap(top_left, height, bitmap)` takes three, and the constructors, three pins and the
buffer, four, as a span brings its length.

Parameters that a caller could swap unnoticed take a type each (FN-16): the three pins are
`DataInPin`, `ChipSelectPin` and `ClockPin`, with an `explicit` constructor, as a bare number
would be any of them again; a module beside its column or row is a `Module`, as in
`SetRow(Module{2}, 3, value)`; a register is a `max7219::Register`; and a dot is a `Point` of an
`X` and a `Y`, `Point{X{3}, Y{2}}`, so that a point with its column and row swapped does not
compile either. A column or a row of the whole panel takes its `X` or `Y` as well,
`SetColumn(X{5}, value)`, so that every coordinate a sketch writes names its axis. A rectangle is
given by two opposite corners rather than a corner and a size, which would want a type each for
its width and height; swapped, the two corners draw the same rectangle, as the two ends of a line
draw the same line. Below the facade, the parts take a `Point` where a sketch's point arrives,
and unpack it into `int16_t` at the start of a function, and pass on a new point rather than a
part of one; `internal::Panel::SetDot()` unpacks it inline, before the call. avr-gcc 7.3 folds
no constant through a part of a point, and passes a point to a function it does not inline as
one value of four bytes: both cost the examples up to 120 bytes of flash (OIL 41).

clang-tidy's `bugprone-easily-swappable-parameters` checks FN-16, though not a function that
passes its parameters on in one call, as the facade's do, which it takes for parameters used
together; there review checks it. Where two such parameters stay, a `NOLINT` names FN-16 and
why: a private function whose few callers pass named values, an order a standard fixes, as
RFC 3174's for SHA-1, or a test's helper whose test fails on a swap.

#### Integer types

Every integer has a fixed width, in `src/`, `simulator/` and `test/` alike: `uint8_t`,
`int8_t`, `int16_t`, `uint16_t`, `uint32_t` and so on — never `int`, `unsigned`, `short` or
`long`, and never Arduino's `byte`, which says nothing of what it holds (T-17). An `int` is 16
bits on the Uno and 32 on the host, so the same code would behave differently in the tests
than on the board. A coordinate that can lie off the panel to the left or above is signed
(`int16_t`); a `uint8_t` that goes into a `std::ostream` is cast to `uint16_t` first, or it
prints as a char. Only the mock keeps `byte`, as the core it stands in for declares it.

A byte of dots is raw data, no number (T-17): a column or a row, the buffer, what a shift takes
in and gives out is a `Byte`, which takes only the bitwise operators and is a number only
through `ToInteger()`. A value of a configuration register of the MAX7219, as the intensity
or the scan limit, is a number and stays a `uint8_t`, `RegisterWrite()` among them; so does a
table in flash, a font or a bitmap, which mixes numbers with columns or is read by
`pgm_read_byte()`, and so do the columns of a `Sprite`, which a sketch writes beside its width
and height, as a font's; each is made a `Byte` column by column as it is read. Two things of `Byte`
are measured choices, each with its reason at the code: its default constructor leaves it as
`std::byte`'s does, against T-13, and `internal::Panel::ChangeMapping()` keeps its private
array as numbers, as avr-gcc walks an array of a class by an index (OIL 41, 42).

Bits of a number are read and written through `Bit` (H-9), in the host code as well: the
simulator's SHA-1 and Base64, the frames of the WebSocket, the model and the tests. `Byte`
keeps the built-in operators in the bodies of its own `&`, `|`, `~`, `<<` and `>>`, as H-9
allows a type of raw data. `Bit::SetBits()` and `ClearBits()` take the target as a template, so
that the mock's port registers, objects with `|=` and `&=` of their own, are written as the
board's are. A combination of flags of an outside interface that takes them as an `int`, as
wxWidgets' and POSIX's, is named once, in a `constexpr`, which H-9 allows.

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

What a function only reads through a pointer or a span is `const` (CodingStyle T-5), also
where it keeps it: `SetTextWithShift()` keeps its `span<char const>` in a member, and once took
a `char*` while it read the text only, so that a sketch could not make its text `const`. No
check finds that: the const checks of clang-tidy take a pointer stored in a member that is not
`const` for a write, so review does.

A text itself is an array, `char const kText[] = "...";` (T-20): the object is the chars, and
only an array takes `PROGMEM`. A `span<char const>` picks a text that exists already, with its
length, as `text` of `Message` in `Messages`, and is what a function of the library takes for a
text: the text ends at its null character or at the end of the span, whichever comes first,
so that a buffer without one is not read past its end. A text in flash, as `F()` gives it or
`FromFlash()` a text of a table with `PROGMEM`, is a `__FlashStringHelper const*`, as the core's
`Print` takes it, and ends at its null character alone; `internal::Text` reads either through
`ReadChars()`, which copies a char from flash a byte or two at a time for `DecodeChar()` (OIL 44).
The mock reads flash where RAM is, so only the examples in simavr tell the two apart: `Messages`
and `ScrollText` keep their texts in flash. `Command` alone walks its own line by
byte positions, as that line always ends with a null character and a span cost SerialControl
flash (OIL 41).

Every other table is a `max_matrix::array`, a font, a bitmap or a row of a sketch and the
library's own, made by `make_array<T>()` where its values are known when compiling, so that
nobody counts them: `constexpr auto kHeart PROGMEM{max_matrix::make_array<uint8_t>(0x0C, ...)}`.
T-20 leaves the C array to a text in quotes alone, and no folder turns its checks off.

#### Constants and configuration

Constants are `constexpr` in the namespace, grouped under a comment; one that only a `.cpp`
uses lies in an anonymous namespace there. The host code, which has no namespace yet, does the
same, but a constant of a header carries its module's name, as a class does:
`kSimulatorWebSocketOpcodeText`. A macro is left for what a sketch or the build sets
with `-D` and what `#if` reads: the configuration parameters; and in the host code for what is
no constant expression, a `wxColour`. Configuration switches are
`STD_ON`/`STD_OFF` and are tested as `#if(MAXMATRIX_USE_SPI == STD_ON)`. The registers of the
MAX7219 are the `enum class max7219::Register`, so that a value cannot take the place of an
address (FN-16), and their values get a named constant in `max7219` rather than a literal, as
`Matrix::RegisterWrite()` takes them:
`RegisterWrite(max7219::Register::kIntensity, max7219::kIntensityMaxValue)`.

Every configuration parameter is wrapped in `#ifndef`, because that is how the tests build the
SPI variant (`-DMAXMATRIX_USE_SPI=STD_ON`). A new parameter gets the same guard.

#### Return values

A function of the library that can fail says why, in `Error`
(`src/MaxMatrixError.h`): it returns a `Status`, or a getter of a single value a
`Result<T>` with the value and the error side by side. Neither converts to `bool`, as
`E_OK` was 0 and `if(matrix.SetText(…))` was true on an error; code asks `IsOk()` or compares
with an error. Only an array still goes through a parameter (`GetRow()` of the whole panel by
reference). A new reason gets its own value in the enum, a text in `kErrorTexts`
(`src/MaxMatrixError.cpp`), a case in `kTextCases` of `test/MaxMatrixErrorTest.cpp`, and each
way to fail a case of `ErrorCaseTest` there, which checks the reason and that the call changes
nothing. `Command::Feed()` answers a failed line with
`GetErrorText()` on the `Print` it is given, and returns the reason as well.

A result whose drop is a mistake is `MAXMATRIX_NODISCARD` (FN-10): a getter's, whose value would
be lost, and the `Status` a part below the facade passes on. A setter's `Status`, as of
`SetText()`, is not, nor what `Command::Feed()` returns: a sketch drops them when it has no one
to tell, and the examples do. Where the library drops a marked result on purpose, it hands it to
`Discard()` with the reason beside it.

An example checks the results it gets (F-15), but drops a setter's: `Init()` fails on a pin the
board lacks, which a user who copies an example and changes its pins can hit, so every example
lights the board's LED then and draws nothing, at 166 to 188 bytes of flash with bit banging for
`pinMode()` and `digitalWrite()` of the core, which the SPI library links already, 12 to 30
there; a setter fails only on a value the example itself wrote, which a run of every example in
simavr would show, as its panel would differ, and a check would cost flash for an error no run
can meet.

The host code marks alike what it computes or finds, as `ControlCommand::MakeText()` or
`SimulatorWebSocket::EncodeFrame()`, and leaves unmarked what only says whether an action
worked, as `Send()` or `Begin()`. It drops no result without `Discard()` and its reason, marked
or not, a system call's as well (`close()`, `setsockopt()`, `avr_ioctl()`), and handles one
whose failure matters instead: a socket that still blocks is closed, not polled. A result is a
value that tells the caller something, success, a count, a place or whether something changed;
a reference to the object itself, as `std::string::append()` and `std::ostream::flush()` return,
or a handle to what was just added, as `wxSizer::Add()` returns, is none, as the call returns
it only so that calls can be chained.

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
for success, against E-1 of CodingStyle.md, which asks for an error type: each of its functions
fails for one reason, which its `\return` names beside `false`.
`Error` has no words for a port that is taken, and `std::optional` is C++17, while the
simulator is built as `gnu++11` with the library. A function that finds or reads something
returns it, and whether it found it, in a small struct of its own, as `SimulatorFoundText` or
`ControlPicture`, never through a pointer (FN-8); a buffer that grows over many calls, as what a
port has read, is a reference, as is an object a function draws on (FN-15).

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

Every file includes what it uses (F-5, `misc-include-cleaner`), the C library's headers as
`<stdint.h>`, as avr-gcc has no `<cstdint>`. `MaxMatrix.h` exports the headers whose names a
sketch uses, with `// IWYU pragma: export`, so that a sketch includes it alone; a new public
header is exported there as well. `PROGMEM` and `pgm_read_byte()` come from `MaxMatrixPgmspace.h`,
which finds them where the board's core has them, `<avr/pgmspace.h>` or the ESP32's `<pgmspace.h>`,
so that neither the library nor a sketch names avr-libc's header itself.
`MaxMatrixTestSupport.h` exports `MaxMatrix.h` to the tests of the facade.

#### Comments

Comments say *why*, not what — the reasoning that is not recoverable from the code, such as
which alternative was rejected and what went wrong with it. Keep a comment where the code it
explains is, as `docs/Documentation.md` says: in the body, at the private member, or at the
definition in the `.cpp`, never between a Doxygen block and its declaration.

### Encoding and umlauts

A source file of this repository is **ASCII**, an example and a test as well (F-10 of
CodingStyle.md). A user's sketch is not: the Arduino IDE saves it as UTF-8, so an a umlaut in a
user's text arrives as the two bytes `0xC3 0xA4`.

- **A char beyond ASCII is written as its bytes:** in a string as escapes, in a comment by its
  name (`A umlaut`, `sharp s`), as `kDefaultFont` does. An escape takes every hex digit after
  it, so a string that goes on with one starts a literal of its own: `"\xC3\x96" "fen"`.
- **Compare against the latin-1 code** (`0xE4`), never a character literal: an umlaut in
  quotes would be two bytes of UTF-8 that never equal a `char`.
- Text goes through `DecodeChar()`, which turns a two-byte UTF-8 sequence into one latin-1
  char before the glyph lookup. A new function that walks a string uses it too.
- A font holds its chars at their latin-1 codes, a char it lacks as a width of 0: a char added
  to `kDefaultFont` takes the place of the 0 at its code.

### Build and check

`make` lists every target; `make check` is what CI runs, all of it, `make avr` among it. Beyond
the template's targets:

```bash
make avr                      # every example for the Uno, in simavr, against simulator/Expected/
make boards                   # every example for the Uno R4, the Pico and the ESP32; BOARDS="esp32"
build/host/DocuPictures --write docu   # draws docu/ again after a change that moves a dot
tools/wokwi.py EXAMPLE        # an example for Wokwi, see wokwi.toml
```

Compile every example after a change to the library, not just the one that uses the change:
the examples are the only code that instantiates the class with the real AVR toolchain.
`make examples` compiles each in the four variants of `tools/project.sh`, bit banging, SPI,
with `MAXMATRIX_DEV_ERROR_DETECT` and `portable`, through `digitalWrite()`, and fails on a warning in this repository; warnings from the
Arduino core are ignored.
#### Tests

`make tests` configures into `build/host`, builds the library four times, with bit banging,
with SPI, with `MAXMATRIX_DEV_ERROR_DETECT` on and `portable`, bit banging through
`digitalWrite()`, and runs the same tests on each through `ctest`, each test under a `bitbang/`,
`spi/`, `det/` or `portable/` prefix; the simulators are built for the first two only. The tests of a variant are a binary per folder of `test/`: ctest runs every test
as a process of its own, which registers every test of its binary first, and under the sanitizers
that took 121 ms a process with all of them in one binary; a new folder gets its line in
`MaxMatrixTestFolders` of `CMakeLists.txt`. In `det/`, `MaxMatrixTestSupport.cpp` defines `ReportError()` itself and records
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
ends by checking that they agree with the buffer, which the fixture owns and hands to its
`Matrix`, as a sketch may. A test of what is lit asks the model, not the buffer. The
model has its own tests against the datasheet, run once under the `model/` prefix.

`mock::chain.IsLit()` goes one step further, to the LEDs as the viewer sees them: given one of
the eight module wirings MD_MAX72XX names (`DR0CR0RR0` to `DR1CR1RR1`, `FC16`, `GENERIC`, …)
and whether the board is mounted upside down. `PanelModuleTest` pins that every module
type shows the picture upright on the wiring of its name, and the named four on the wirings
MD_MAX72XX gives those names; the README's table says the same and changes with it.

The mock has a `Serial` that reads `mock::serial_input` and appends what the sketch prints to
`mock::serial_output`, so that `SerialControl` builds into a simulator as well; `F()`, `PSTR()`,
`strcmp_P()` and `pgm_read_ptr()` take strings in RAM there, as the host has no separate flash, but
`F()` keeps the type `__FlashStringHelper const*` of the core. What avr-libc declares lies in
`test/mock/avr/` and `test/mock/util/`, as on the board, so that a file includes the same header for both.
The mock also has `millis()`, `micros()` and `delay()` on the host clock, for the simulator;
`delay()` really waits. A test that times something holds the clock with `mock::SetMillis()`
instead, until the next `mock::Reset()`; `delay()` then moves it on without waiting. Every
`ShiftStep()` is a step, so the library tests need no clock; `IntervalTest` tests the timing on its
own. `mock::tick`, called after every frame and every millisecond of `delay()`, is how the simulator draws while a sketch blocks. The tests leave it unset. The tests
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
`SimulatorAvrTest` checks the simulated time `SimulatorAvr` runs the AVR to, past the 71.6
minutes where simavr's own conversion wrapped; it is built wherever simavr is, as `SimulatorAvr`.

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

A test reaches the library through its API only, as TS-8 of `docs/Testing.md` asks, never
through a private member: the fixture's buffer is its own, as a sketch's may be, and a step of a
shift shows on the panel. What only a part below the facade can show is tested on that part
itself, made in the test, as `WiredTest` stores a bit with `internal::Wired::StoreBit()`, so that
it names the buffer column rather than reaching it through an orientation.

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
