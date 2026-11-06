# MaxMatrix

An Arduino library for chained 8x8 LED dot matrix modules driven by the MAX7219.

## Layout

- `src/` — the library, in the Arduino library format 1.5, one part per folder:
  - `MaxMatrix.h` / `MaxMatrix.cpp` — the `MaxMatrix` class, the facade a sketch includes:
    the public API, the state of the library and `Init()`; the rest it passes on.
  - `MaxMatrixConfig.h` — the configuration macros and the size of the chain.
  - `Max7219/` — `MaxMatrixChain`, the transfer to the chain, and the register macros.
  - `Matrix/` — `MaxMatrixWired`, the buffer as wired, and `MaxMatrixPanel` on top of it, the
    coordinates as the viewer sees them.
  - `Font/` — `MaxMatrixFont`, the sprite table, the latin-1 lookup and `DecodeChar()`.
  - `Text/` — `MaxMatrixText`, text that stands or shifts in.
  - `Command/` — `MaxMatrixCommand`, the lines of text a control page or program sends
    (`text=`, `setDot=`, …), carried out on a `MaxMatrix`.
  - `MaxMatrixError.h` / `.cpp` — `MaxMatrixError`, and `MaxMatrixStatus` / `MaxMatrixResult<T>`,
    what a function that can fail returns, with `GetErrorText()`; `MaxMatrixErrorReport.cpp`, the
    report of `MAXMATRIX_DEV_ERROR_DETECT`, in a file of its own (see "Return values").
  - `MaxMatrixStandardTypes.h` — `STD_ON`/`STD_OFF` and `STD_NULL_CHARACTER`.
- `examples/` — one sketch per folder, compiled by `arduino-cli` and checked by `arduino-lint`.
- `docu/` — the coordinates a sketch uses (`Coordinates.svg`, shown in the README) and how the
  bytes of `matrix_buffer_` lie on a module in each module orientation (`Wiring_*.svg`). None is
  drawn by hand: `DocuPictures.cpp` draws them from the library and the model, and the test
  `docu/pictures` fails until `build/DocuPictures --write docu` has drawn them again after a
  change that moves a dot. `ErrorHandling.md` records what was weighed, and measured, for the
  errors of OIL item 27. `Documentation.md` says how the code is documented.
- `simulator/` — runs an example sketch unchanged on the host, against the mock and the model,
  and draws the panel as a module of the chosen wiring would light it: in the terminal, or with
  `--web` on `localhost:8080`. One class per job, as in `src/`:
  - `main.cpp` — `setup()` once, `loop()` over and over, and `Tick()`, which `mock::tick` calls.
  - `SimulatorOptions` — the command line.
  - `SimulatorFrontEnd` — the ends both simulators share: the terminal or the page, and the
    serial host; it takes what the sketch printed from the mock once per tick, for all of them.
  - `SimulatorPanel` — the model's LEDs for a wiring and mounting, and whether to draw again.
  - `SimulatorTerminal` — the panel redrawn in place, or printed once at the end of `--for`.
  - `SimulatorWeb` — the JSON for `index.html` (read at run time) and its commands; `serial LINE`
    from the page reaches the sketch's `Serial`, and what the sketch prints goes back in the JSON.
  - `SimulatorWebHost` / `SimulatorWebSocket` — HTTP on loopback and the WebSocket protocol,
    ported from Wordclock; the protocol works on strings only, so it is tested without a network.
  - `SimulatorSerialHost` — `--tcp PORT`: the sketch's `Serial` as a plain TCP port on loopback,
    one client at a time, for the control program, which has no WebSocket.
  - `SimulatorSocket` — what both hosts do with a socket: listen on loopback, never wait, send
    all.
  - `mainAvr.cpp` / `SimulatorAvr` — the same simulator for the `.elf` `avr-gcc` makes of an
    example (`--elf`): simavr runs it on an ATmega328P, its pins 10, 11 and 13 and its SPI output
    drive the model, its UART is `Serial`. It writes the `mock::` variables the Arduino stand-in
    writes, so panel, terminal and page work unchanged.
  `EXAMPLE.txt` is the panel a test expects of an example at a time CMake names, `Basics.txt`
  one second into `Basics` on an FC-16 board;
  `WebTest.sh` checks what a browser would get from `--web`, `ControlTest.sh` the commands of
  the page with `SerialControl` (both with the WebSocket of `WebTestLib.sh`), `TcpTest.sh` the
  same commands over `--tcp`, and `PageTest.js` the page's own script, against stand-ins for the
  DOM, the WebSocket and a board on Web Serial.
- `control/` — the control program of OIL item 26, `MaxMatrixControl`: SerialControl on a board
  over its serial port, or in the simulator over `--tcp`, from a wxWidgets window laid out as the
  old Windows tool of `AndreasBur/Led-Matrix`. A CMake project of its own as well, for Windows.
  Classes start with `Control`; everything but the window is the library `Control`, tested
  without wxWidgets:
  - `ControlPort` — one end: `ControlSerialPort` (`termios` / Win32) or `ControlTcpPort`.
  - `ControlLink` — the lines over a port, waiting for `ready` on a board, and asking for the
    `picture` the window shows.
  - `ControlCommand` — the lines `MaxMatrixCommand` reads, whether one fits its buffer, and the
    answer to `picture`.
  - `ControlWindow`, `ControlDotPanel` — the window and the matrix as SerialControl answers
    `picture`, to click on, checked by hand;
    `.clang-tidy` there switches off the check that argues with wxWidgets' flag enums.
- `diagram.json`, `wokwi.toml` — a Uno with four FC-16 modules for Wokwi, which runs the real
  AVR binary that `tools/wokwi.py EXAMPLE` builds; to look at, not tested in CI.
- `OIL.md` — the open issue list: what is planned, in which order, and why. A PR that works
  on an item updates its row.
- `test/` — host-side tests on GoogleTest against a mock of the Arduino core and a model of
  the MAX7219 chain in `test/mock/`, built by `CMakeLists.txt` in the root.
- `.github/workflows/ci.yml` — host tests, the examples on `arduino:avr:uno`, `arduino-lint`,
  `tools/east-const.py`, which finds a `const` left of what it qualifies, and
  `tools/doc-indent.py`, which finds a line of a Doxygen block not indented under its command.
- `.github/workflows/docs.yml` — the reference from the Doxygen blocks, checked on every pull
  request by `tools/docs.py` and published on GitHub Pages from `main`; `Doxyfile` says what
  Doxygen reads.
- `.github/workflows/static-analysis.yml` — clang-tidy over the host build and the Uno's, and
  cppcheck; `.clang-tidy` says which checks and why the rest are off, and
  `tools/cross-clang-tidy.py` lets clang read what `avr-gcc` was told.
- `.devcontainer/` — one `Dockerfile` plus a `devcontainer.json` per host (`linux`, `wsl2`);
  shared editor settings and volumes live in the local `shared/` feature.

## How the library is built

### The matrix buffer

`matrix_buffer_` mirrors what is on the LEDs: **one byte per column, one bit per row**
(bit 0 is row 0), columns of all modules in a row, module 0 first — as wired, which in
module orientation 0 is also how the viewer sees it; `docu/Wiring_*.svg` show the other three.
The buffer is what `get…` functions read — the MAX7219 cannot be read back.

Drawing only changes the buffer. `MaxMatrixWired` keeps which columns of a module changed, one
bit each in `changed_columns_`, and `Send()` sends each of them in one frame for all modules:
eight frames at most for a whole picture. The facade decides when: with `auto_update_`, the
default, every public function that draws calls `UpdateIfAuto()` before it returns, so that each
call shows its result; with it off, only `Update()` sends. A new public function that draws
ends with `UpdateIfAuto()` as well, and nothing below the facade sends. `Clear()` marks every
column, even one that was empty already, because after power-up the registers hold anything.
`MaxMatrixBatch` turns `auto_update_` off for the life of a block and puts back what it found, so
that a sketch cannot forget to turn it on again; the sketch-facing docs lead with it.

The facade also keeps the pace of a shifting text: `ShiftTask()` returns at once until
`shift_interval_` has passed since `last_shift_`, so a sketch calls it on every `loop()` and times
nothing itself.

### The parts

Dependencies point one way: `MaxMatrixText` → `MaxMatrixFont` and `MaxMatrixPanel`,
`MaxMatrixPanel` → `MaxMatrixWired` → `MaxMatrixChain` → `Arduino.h` / `SPI.h`. Each part is a
member of the one above it, by value and without virtual functions, so that the parts cost no
RAM of their own; `MaxMatrixText` is handed the panel by every function that draws, rather
than keeping a reference, which cost 86 bytes of flash in `ScrollText` (see its header).

`MaxMatrixCommand` sits on top of the facade rather than in it: the sketch owns it beside its
`MaxMatrix` and hands it the matrix with every char, as the panel is handed to the text. A sketch
that does not use it pays nothing, because the linker drops what no one calls; `Basics` and
`ScrollText` are the same size with it as without.

### Two coordinate systems

- **`MaxMatrixPanel`** (`SetColumn`, `SetRow`, `SetDot`, `ShiftLeft`, …) takes coordinates as
  the viewer sees them.
- **`MaxMatrixWired`** works on the buffer and the hardware as wired and knows nothing about
  orientation. Its functions have the same names; the object says which system is meant.

Between them, `ConvertDotToBuffer()` is the only function that knows the two orientations: the
matrix orientation (0, 180) turns the whole panel, the module orientation (0, 90, 180, 270) then
every module in its place. It maps one dot as the viewer sees it to a buffer column and bit.
Lines go through `GetLine()`/`SetLine()`; shifting and changing an orientation work on the whole
picture in viewer coordinates (`GetPicture()`/`SetPicture()`).

A new feature that works on coordinates builds on these and never branches on an orientation
itself. When every function handled the orientations on its own, rows, shifts and dots
disagreed in all but orientation 0.

### Hardware access

Only `MaxMatrixChain` touches the pins. A transfer to the MAX7219 chain is framed by
`StartTransmission()` and `StopTransmission()`, which drive chip select and, in SPI mode, begin
and end the SPI transaction. Every module in the chain gets one address/value pair per frame.
`WriteRegisterAll()` (one value for every module) and `WriteRegisterEach()` (one value per
module) are the two kinds of frame. Bit banging and SPI
are selected with `#if` on `MAXMATRIX_USE_SPI` in `Transfer()` and nowhere else, and both
branches have to keep working.

## Code conventions

These are observed throughout the existing code — follow them rather than reformatting to a
different taste. They hold for `simulator/` and `control/` as much as for `src/`: host code that
runs beside the library is read beside it. Only the tests in `test/` keep a lighter style, a comment at the
top and one above each test.

### Naming

Google's names, as in Utl and Wordclock, so that every project names alike; clang-tidy's
`readability-identifier-naming` checks them, with the options in `.clang-tidy`:

- Types, functions and methods `PascalCase`: `MaxMatrixPanel`, `SetText()`, `GetState()`;
  `Get`/`Set` for accessors, `Convert…`/`Decode…` for conversions.
- Variables and parameters `snake_case`; private and protected members end in `_`
  (`module_orientation_`), so that a setter's parameter takes the plain name
  (`SetModuleOrientation(MaxMatrixModuleOrientation module_orientation)`). Members of a
  struct whose members are all public have no `_`.
- Constants, `constexpr` and `const` with static storage, start with `k`: `kErrorTexts`,
  `kDataInPin` in a sketch.
- Enums are `enum class` with values in the same form: `MaxMatrixState::kReady`,
  `MaxMatrixError::kOutOfRange`. The indices of a sprite, `kSpriteWidth` and on, are a plain
  enum, as they index an array.
- Macros upper case and prefixed with the module: `MAXMATRIX_NUMBER_OF_MODULES`,
  `MAX7219_REG_INTENSITY_ADDRESS`. An include guard names the path:
  `MAXMATRIX_SRC_MATRIX_MAXMATRIXPANEL_H_`.
- Classes start with `MaxMatrix`. A sketch links every library it includes into one namespace,
  and a class called `Font` or `Text` would collide with the next library that has one; the
  mock in `test/mock/` already has a `Max7219Chain`. The simulator is linked with the sketch
  too, so its classes start with `Simulator`. A type of the library does as well:
  `MaxMatrixSprite`, `MaxMatrixRow`.
- What Arduino names keeps its name: `setup()` and `loop()` in a sketch, and the stand-ins of
  the Arduino core in `test/mock/` (`digitalWrite()`, `Serial`, `SPI`), which carry a NOLINT.
- Files are named after their class, in `PascalCase`, as Arduino names its own (`Arduino.h`,
  `HardwareSerial.h`, `Servo.h`): `Matrix/MaxMatrixPanel.h`, `simulator/SimulatorWebHost.cpp`.
  `src/MaxMatrix.h` has to keep that form, as a sketch includes it by the library's name. Google's
  `snake_case` for files was tried and dropped (#45); Utl keeps it, as it stands in for the STL.

### Integer types

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
it is on the Uno); the result of `strtoul()` until its range is checked; POSIX file descriptors
and `setsockopt()` options (`int`); and `size_t` for sizes of containers.

### const

`const` stands right of what it qualifies, in every file alike: `char const* Text`,
`MaxMatrixBatch const&`, `std::string const Name`, `static char const kErrorTextOk[] PROGMEM`,
`struct { … } const Names[]`. Read from right to left, the declaration then says what it is,
and `char const* const` needs no second rule. The Arduino core writes `const` on the left; the
mock, which stands in for it, and the examples follow this repository all the same.

No compiler and no check of `.clang-tidy` notices a `const` on the left, so
`tools/east-const.py` does, as a job of `ci.yml`. clang-format could move it with
`QualifierAlignment: Right`, but only while formatting the whole file.

### Constants and configuration

Constants are `#define`s in the header, grouped under a comment. Configuration switches are
`STD_ON`/`STD_OFF` and are tested as `#if(MAXMATRIX_USE_SPI == STD_ON)`. Register addresses
and values of the MAX7219 get a named macro rather than a literal.

Every configuration parameter is wrapped in `#ifndef`, because that is how the tests build the
SPI variant (`-DMAXMATRIX_USE_SPI=STD_ON`). A new parameter gets the same guard.

### Return values

A function of the library that can fail says why, in `MaxMatrixError`
(`src/MaxMatrixError.h`): it returns a `MaxMatrixStatus`, or a getter of a single value a
`MaxMatrixResult<T>` with the value and the error side by side. Neither converts to `bool`, as
`E_OK` was 0 and `if(matrix.SetText(…))` was true on an error; code asks `IsOk()` or compares
with an error. Only an array still goes through a parameter (`GetRow()` of the whole panel by
reference, `GetSprite()`). A new reason gets its own value in the enum, a text in `kErrorTexts`
(`src/MaxMatrixError.cpp`) and a row in the test `EveryErrorSaysWhy`, which checks every way a
call can fail and that it changes nothing. `MaxMatrixCommand::Feed()` answers a failed line with
`GetErrorText()` on the `Print` it is given, and returns the reason as well.

The texts lie in a table in `PROGMEM`, not behind a `switch`: avr-gcc turns a `switch` that
returns a constant per case into a table in `.rodata`, which the Uno copies into RAM, as it does
with every constant not marked `PROGMEM`.

Coordinates and indices are range checked before the buffer is touched. A function that draws
leaves out what lies off the panel rather than failing, because drawing partly off the panel is
what a moving sprite or text does all the time (see `MaxMatrixPanel::SetDot()`); `SetDot`,
`SetColumn` and `SetRow` cannot fail at all and return `void`, while `SetChar` and `SetText`
still fail on a char the font lacks. A getter or a setting out of range returns
`MaxMatrixError::kOutOfRange` and changes nothing. `MaxMatrixCommand` checks the coordinates it is
sent itself, so that a typed line that would do nothing says so.

With `MAXMATRIX_DEV_ERROR_DETECT`, every function of the facade that fails reports its name and
the error to `MaxMatrixReportError()`, as the DET of AUTOSAR Classic does. A new function of the
facade that can fail wraps the call of the part below in `MAXMATRIX_REPORT_ERROR("name", …)`,
which is the bare call when the switch is off; the parts below the facade and `MaxMatrixCommand`
report nothing. The weak default, which prints on `Serial`, lies in `MaxMatrixErrorReport.cpp`,
and `library.properties` links the library as an archive (`dot_a_linkage`): linked as loose
objects, the default pulled `Serial` into a sketch that defines its own report, 175 bytes of RAM
on the Uno.

Host code whose failures no sketch sees, the simulator and `DocuPictures`, returns `bool`, `true`
for success: each of its functions fails for one reason, which its `\return` names beside `false`.
`MaxMatrixError` has no words for a port that is taken, and `std::optional` is C++17, while the
simulator is built as `gnu++11` with the library.

### Layout of a source file

Every file of `src/`, `simulator/`, `control/` and `docu/` starts with the copyright, the SPDX line and a
`\file` block, as the files of Utl do; then come the includes and the code, with no banners
between them and no name after a closing brace. Indentation is four spaces.

How the code is documented is in [docu/Documentation.md](docu/Documentation.md), the style of
Utl: a Doxygen block at each declaration, in the header, for the one who uses it; a comment on the
implementation, `//` for one or two lines and `/* ... */` from three, where the code it explains
is; what a sketch reads documented whole, and how `tools/docs.py` and clang's `-Wdocumentation`
check it.

An include in `src/` names a header by its path from `src/` (`"Font/MaxMatrixFont.h"`), or a
header in the same folder by its name alone, never with `../`. Both builds put `src/` on the
include path: `arduino-cli` for a library in format 1.5, and `CMakeLists.txt`.

### Comments

Comments say *why*, not what — the reasoning that is not recoverable from the code, such as
which alternative was rejected and what went wrong with it. Keep a comment where the code it
explains is, as `docu/Documentation.md` says: in the body, at the private member, or at the
definition in the `.cpp`, never between a Doxygen block and its declaration.

## Encoding and umlauts

All files are **UTF-8**, as `.editorconfig` says. The Arduino IDE saves sketches as UTF-8 as
well, so an `ä` in a user's text arrives as the two bytes `0xC3 0xA4`.

- **Never compare against a character literal such as `'ä'`.** In a UTF-8 source file it is a
  two-byte multi-character constant that never equals a `char`. Compare against the latin-1
  code (`0xE4`) and put the character in a comment beside it, as `ConvertCharToSprite` does.
- Text goes through `DecodeChar()`, which turns a two-byte UTF-8 sequence into one latin-1
  char before the sprite lookup. A new function that walks a string uses it too.
- A sprite added to `SpriteTable` gets its latin-1 code in `ConvertCharToSprite`.

## Build and check

In the dev container, the same five things CI runs:

```bash
cmake --workflow --preset default
arduino-cli compile --fqbn arduino:avr:uno --library . --warnings all examples/ScrollText
arduino-lint --library-manager submit
tools/east-const.py
tools/doc-indent.py
```

the reference that `docs.yml` builds, into `build/docu/html`, failing on any warning that counts:

```bash
tools/docs.py
```

and what `static-analysis.yml` reads the code with, each of which fails on any finding:

```bash
run-clang-tidy -p build -quiet "$PWD/(src|simulator|control)/"
run-clang-tidy -p build -quiet -checks='-*,readability-identifier-naming' \
               -header-filter="$PWD/(test|docu|examples)/" "$PWD/(test|docu|examples)/"
arduino-cli compile --fqbn arduino:avr:uno --library . --only-compilation-database \
                    --build-path build/avr examples/SerialControl
python3 tools/cross-clang-tidy.py build/avr "$PWD/src/"
cppcheck --enable=warning,portability --check-level=exhaustive --std=c++17 --inline-suppr \
         --error-exitcode=1 --quiet $(find src -type d | sed 's/^/-I/') -Itest/mock -Isimulator \
         -Icontrol src simulator control
```

A finding is fixed rather than silenced where the fix is cheap. Where it is not, a NOLINT names
the check, and every name it is reported under, with the reason in a comment beside it; a check
that argues with a convention of this file is switched off in `.clang-tidy`, with the reason
there.

Compile every example after a change to the library, not just the one that uses the change —
the examples are the only code that instantiates the class with the real AVR toolchain. CI
fails on any warning in this repository, from `g++` in the tests (`-Werror`) and from
`avr-gcc` in the examples; warnings from the Arduino core are ignored.

### Tests

The workflow preset configures into `build/`, builds the library three times, with bit banging,
with SPI and with `MAXMATRIX_DEV_ERROR_DETECT` on, and runs the same tests on each through
`ctest`, each test under a `bitbang/`, `spi/` or `det/` prefix; the simulators are built for the
first two only. In `det/`, the test file defines `MaxMatrixReportError()` itself and records into
`Reports`, which stays empty in the other two, so that a test expects reports in every variant. The library and the mock are built as `gnu++11`, like `avr-gcc` builds a sketch;
only the tests are C++17, which GoogleTest needs. A test that loops over the four orientations
is a `TEST_P` instead. The mock records every frame between a falling and a rising chip select in
`mock::last_frame` and counts a transfer outside a frame or outside an SPI transaction in
`mock::errors`, so a test can check what went to the chain, not just the buffer.

Every byte and every rising chip select also drive `mock::chain`, a model of the MAX7219 chain
(`test/mock/Max7219.h`): it shifts like the hardware and each chip latches its register on the
rising edge, so a frame that is too short or a wrong address shows on the model's LEDs.
`Display()` in the tests reads them back laid out like `matrix_buffer_`, and every library test
ends by checking that the two agree. A test of what is lit asks the model, not the buffer. The
model has its own tests against the datasheet, run once under the `model/` prefix.

`mock::chain.IsLit()` goes one step further, to the LEDs as the viewer sees them: given one of
the eight module wirings MD_MAX72XX names (`DR0CR0RR0` to `DR1CR1RR1`, `FC16`, `GENERIC`, …)
and whether the board is mounted upside down. `MaxMatrixModuleTest` pins which orientations
fit which module; the README's table says the same and changes with it.

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
`simulator/Basics.txt`: the whole way from a sketch to the LEDs of an FC-16 board. The test
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
time and compares the panel with `simulator/EXAMPLE.txt`, the same file for both transfer paths.
The time is simulated, so a moving example ends with the same panel every run, which the
simulator on the host clock cannot promise. `avr/*/control` is `simulator/control` against the
`.elf` of `SerialControl`, on port 18083, and `avr/*/tcp` is `simulator/tcp`, on port 18085; with
`--web` or `--tcp`, `SimulatorAvr` keeps to the host's clock, as the page and the program do. CMake leaves the tests out where `arduino-cli` or simavr is missing, as in the
host-tests job of CI; the job "Examples in simavr" runs them. A new example gets a line in
`AvrRuns` in `CMakeLists.txt` and its `.txt`, written from a run of `SimulatorAvr` whose panel
has been looked at.

**A bug fix comes with a test that fails without the fix.** Put the bug back and run the
tests before committing; a test that stays green then does not test the bug. The comment above
the test names the bug it guards against.

The tests reach private members through `#define private public`, because some of what they
check has no public path — which frame a change to one buffer column turns into, for instance,
is checked by storing the bit with `MaxMatrixWired::StoreBit()`, so that the test names the
buffer column itself rather than reaching it through an orientation. The parts are private
members of `MaxMatrix`, so a test reaches the buffer as `matrix_.panel_.wired_.matrix_buffer_`. 
The define comes after the GoogleTest and standard headers, which it would rewrite as well.

### Hardware

Nothing here reaches real hardware. The tests under `avr/` run the transfer code as the Uno
runs it, but against the model, not against a MAX7219 and its timing on a real wire. A change
to the transfer code (chip select, SPI, bit banging) is untested until it has run on a matrix,
and the pull request says so.

## Delivering a change

Anything more than a trivial fix goes on a branch and arrives as a pull request, rather than
as a commit on `main`. `gh` is in the dev container for that:

```bash
git switch -c <branch>
# commit there, then
git push -u origin <branch>
gh pr create --fill
```

A stale line in a document, a typo, a comment that no longer matches its code — those may
still go straight to `main`. Anything that changes behaviour does not.

What the pull request body has to carry is what the commits already argue: what moved, why
that way rather than the obvious alternative, and what was measured. A body that repeats the
diff is worth nothing to a reviewer; the numbers — flash and RAM of the examples on the Uno,
which examples were compiled, whether it ran on hardware — are the part nobody can
reconstruct from the code. The CI job "Examples on arduino:avr:uno" writes flash and RAM of
every example into its job summary.
