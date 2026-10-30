# MaxMatrix

An Arduino library for chained 8x8 LED dot matrix modules driven by the MAX7219.

## Layout

- `src/` — the library, in the Arduino library format 1.5, one part per folder:
  - `MaxMatrix.h` / `MaxMatrix.cpp` — the `MaxMatrix` class, the facade a sketch includes:
    the public API, the state of the library and `init()`; the rest it passes on.
  - `MaxMatrixConfig.h` — the configuration macros and the size of the chain.
  - `Max7219/` — `MaxMatrixChain`, the transfer to the chain, and the register macros.
  - `Matrix/` — `MaxMatrixWired`, the buffer as wired, and `MaxMatrixPanel` on top of it, the
    coordinates as the viewer sees them.
  - `Font/` — `MaxMatrixFont`, the sprite table, the latin-1 lookup and `decodeChar()`.
  - `Text/` — `MaxMatrixText`, text that stands or shifts in.
  - `MaxMatrixStandardTypes.h` — `stdReturnType`, `STD_ON`/`STD_OFF` and bit macros.
- `examples/` — one sketch per folder, compiled by `arduino-cli` and checked by `arduino-lint`.
- `docu/` — how rows, columns and bits map onto a module in each orientation.
- `simulator/` — runs an example sketch unchanged on the host, against the mock and the model,
  and draws the panel as a module of the chosen wiring would light it: in the terminal, or with
  `--web` on `localhost:8080`. One class per job, as in `src/`:
  - `main.cpp` — `setup()` once, `loop()` over and over, and `tick()`, which `Mock::Tick` calls.
  - `SimulatorOptions` — the command line.
  - `SimulatorPanel` — the model's LEDs for a wiring and mounting, and whether to draw again.
  - `SimulatorTerminal` — the panel redrawn in place, or printed once at the end of `--for`.
  - `SimulatorWeb` — the JSON for `index.html` (read at run time) and its commands.
  - `SimulatorWebHost` / `SimulatorWebSocket` — HTTP on loopback and the WebSocket protocol,
    ported from Wordclock; the protocol works on strings only, so it is tested without a network.
  `Basics.txt` is the panel a test expects one second into `Basics` on an FC-16 board;
  `WebTest.sh` checks what a browser would get from `--web`.
- `OIL.md` — the open issue list: what is planned, in which order, and why. A PR that works
  on an item updates its row.
- `test/` — host-side tests on GoogleTest against a mock of the Arduino core and a model of
  the MAX7219 chain in `test/mock/`, built by `CMakeLists.txt` in the root.
- `.github/workflows/ci.yml` — host tests, the examples on `arduino:avr:uno`, `arduino-lint`.
- `.devcontainer/` — one `Dockerfile` plus a `devcontainer.json` per host (`linux`, `wsl2`);
  shared editor settings and volumes live in the local `shared/` feature.

## How the library is built

### The matrix buffer

`MatrixBuffer` mirrors what is on the LEDs: **one byte per column, one bit per row**
(bit 0 is row 0), columns of all modules in a row, module 0 first. See
`docu/LED_Matrix.png`. The buffer is what `get…` functions read — the MAX7219 cannot be read
back.

Drawing only changes the buffer. `MaxMatrixWired` keeps which columns of a module changed, one
bit each in `ChangedColumns`, and `send()` sends each of them in one frame for all modules:
eight frames at most for a whole picture. The facade decides when: with `AutoUpdate`, the
default, every public function that draws calls `updateIfAuto()` before it returns, so that each
call shows its result; with it off, only `update()` sends. A new public function that draws
ends with `updateIfAuto()` as well, and nothing below the facade sends. `clear()` marks every
column, even one that was empty already, because after power-up the registers hold anything.
`MaxMatrixBatch` turns `AutoUpdate` off for the life of a block and puts back what it found, so
that a sketch cannot forget to turn it on again; the sketch-facing docs lead with it.

The facade also keeps the pace of a shifting text: `shiftTask()` returns at once until
`ShiftInterval` has passed since `LastShift`, so a sketch calls it on every `loop()` and times
nothing itself.

### The parts

Dependencies point one way: `MaxMatrixText` → `MaxMatrixFont` and `MaxMatrixPanel`,
`MaxMatrixPanel` → `MaxMatrixWired` → `MaxMatrixChain` → `Arduino.h` / `SPI.h`. Each part is a
member of the one above it, by value and without virtual functions, so that the parts cost no
RAM of their own; `MaxMatrixText` is handed the panel by every function that draws, rather
than keeping a reference, which cost 86 bytes of flash in `ScrollText` (see its header).

### Two coordinate systems

- **`MaxMatrixPanel`** (`setColumn`, `setRow`, `setDot`, `shiftLeft`, …) takes coordinates as
  the viewer sees them.
- **`MaxMatrixWired`** works on the buffer and the hardware as wired and knows nothing about
  orientation. Its functions have the same names; the object says which system is meant.

Between them, `convertDotToBuffer()` is the only function that knows the two orientations: the
matrix orientation (0, 180) turns the whole panel, the module orientation (0, 90, 180, 270) then
every module in its place. It maps one dot as the viewer sees it to a buffer column and bit.
Lines go through `getLine()`/`setLine()`; shifting and changing an orientation work on the whole
picture in viewer coordinates (`getPicture()`/`setPicture()`).

A new feature that works on coordinates builds on these and never branches on an orientation
itself. When every function handled the orientations on its own, rows, shifts and dots
disagreed in all but orientation 0.

### Hardware access

Only `MaxMatrixChain` touches the pins. A transfer to the MAX7219 chain is framed by
`startTransmission()` and `stopTransmission()`, which drive chip select and, in SPI mode, begin
and end the SPI transaction. Every module in the chain gets one address/value pair per frame.
`writeRegisterAll()` (one value for every module) and `writeRegisterEach()` (one value per
module) are the two kinds of frame. Bit banging and SPI
are selected with `#if` on `MAXMATRIX_USE_SPI` in `transfer()` and nowhere else, and both
branches have to keep working.

## Code conventions

These are observed throughout the existing code — follow them rather than reformatting to a
different taste. They hold for `simulator/` as much as for `src/`: host code that runs beside
the library is read beside it. Only the tests in `test/` keep a lighter style, a comment at the
top and one above each test.

### Naming

- Functions `lowerCamelCase`; `get`/`set` for accessors, `convert…`/`decode…` for
  conversions.
- Classes start with `MaxMatrix`. A sketch links every library it includes into one namespace,
  and a class called `Font` or `Text` would collide with the next library that has one; the
  mock in `test/mock/` already has a `Max7219Chain`. The simulator is linked with the sketch
  too, so its classes start with `Simulator`.
- Variables, parameters and members `PascalCase`.
- A setter parameter that collides with its member is prefixed `s`
  (`setModuleOrientation(MaxMatrixModuleOrientationType sModuleOrientation)` assigning
  `ModuleOrientation`).
- Types end in `Type` (`MaxMatrixStateType`); enum values and macros are upper case and
  prefixed with the module (`MAXMATRIX_STATE_READY`, `MAX7219_REG_INTENSITY_ADDRESS`).

### Constants and configuration

Constants are `#define`s in the header, grouped under a comment. Configuration switches are
`STD_ON`/`STD_OFF` and are tested as `#if(MAXMATRIX_USE_SPI == STD_ON)`. Register addresses
and values of the MAX7219 get a named macro rather than a literal.

Every configuration parameter is wrapped in `#ifndef`, because that is how the tests build the
SPI variant (`-DMAXMATRIX_USE_SPI=STD_ON`). A new parameter gets the same guard.

### Return values

A function that can fail returns `stdReturnType` (`E_OK` / `E_NOT_OK`) and passes results
through a pointer parameter. Coordinates and indices are range checked before the buffer is
touched; an out-of-range call returns `E_NOT_OK` and changes nothing.

### Layout of a source file

Every file starts with the copyright and `\file` header. Sections are separated by the wide
banner comments (`INCLUDES`, `GLOBAL CONSTANT MACROS`, `P U B L I C   F U N C T I O N S`, …).
Every function has a banner with its name, a Doxygen block (`\brief`, `\details`,
`\param[in]` / `\param[out]`, `\return`, `\pre` where it applies) and ends with
`} /* functionName */`. Indentation is four spaces.

An include in `src/` names a header by its path from `src/` (`"Font/MaxMatrixFont.h"`), or a
header in the same folder by its name alone, never with `../`. Both builds put `src/` on the
include path: `arduino-cli` for a library in format 1.5, and `CMakeLists.txt`.

### Comments

Comments say *why*, not what — the reasoning that is not recoverable from the code, such as
which alternative was rejected and what went wrong with it. Keep a comment attached to the
function whose behaviour it explains.

## Encoding and umlauts

All files are **UTF-8**, as `.editorconfig` says. The Arduino IDE saves sketches as UTF-8 as
well, so an `ä` in a user's text arrives as the two bytes `0xC3 0xA4`.

- **Never compare against a character literal such as `'ä'`.** In a UTF-8 source file it is a
  two-byte multi-character constant that never equals a `char`. Compare against the latin-1
  code (`0xE4`) and put the character in a comment beside it, as `convertCharToSprite` does.
- Text goes through `decodeChar()`, which turns a two-byte UTF-8 sequence into one latin-1
  char before the sprite lookup. A new function that walks a string uses it too.
- A sprite added to `SpriteTable` gets its latin-1 code in `convertCharToSprite`.

## Build and check

In the dev container, the same three things CI runs:

```bash
cmake --workflow --preset default
arduino-cli compile --fqbn arduino:avr:uno --library . --warnings all examples/ScrollText
arduino-lint --library-manager submit
```

Compile every example after a change to the library, not just the one that uses the change —
the examples are the only code that instantiates the class with the real AVR toolchain. CI
fails on any warning in this repository, from `g++` in the tests (`-Werror`) and from
`avr-gcc` in the examples; warnings from the Arduino core are ignored.

### Tests

The workflow preset configures into `build/`, builds the library twice, with bit banging and
with SPI, and runs the same tests on both through `ctest`, each test under a `bitbang/` or
`spi/` prefix. The library and the mock are built as `gnu++11`, like `avr-gcc` builds a sketch;
only the tests are C++17, which GoogleTest needs. A test that loops over the four orientations
is a `TEST_P` instead. The mock records every frame between a falling and a rising chip select in
`Mock::LastFrame` and counts a transfer outside a frame or outside an SPI transaction in
`Mock::Errors`, so a test can check what went to the chain, not just the buffer.

Every byte and every rising chip select also drive `Mock::Chain`, a model of the MAX7219 chain
(`test/mock/Max7219.h`): it shifts like the hardware and each chip latches its register on the
rising edge, so a frame that is too short or a wrong address shows on the model's LEDs.
`display()` in the tests reads them back laid out like `MatrixBuffer`, and every library test
ends by checking that the two agree. A test of what is lit asks the model, not the buffer. The
model has its own tests against the datasheet, run once under the `model/` prefix.

`Mock::Chain.isLit()` goes one step further, to the LEDs as the viewer sees them: given one of
the eight module wirings MD_MAX72XX names (`DR0CR0RR0` to `DR1CR1RR1`, `FC16`, `GENERIC`, …)
and whether the board is mounted upside down. `MaxMatrixModuleTest` pins which orientations
fit which module; the README's table says the same and changes with it.

The mock also has `millis()`, `micros()` and `delay()` on the host clock, for the simulator;
`delay()` really waits. A test that times something holds the clock with `Mock::setMillis()`
instead, until the next `Mock::reset()`; `delay()` then moves it on without waiting. The library
tests set the shift interval to 0 in `SetUp()`, so that every `shiftTask()` is a step, and test
the timing on its own. `Mock::Tick`, called after every frame and every millisecond of
`delay()`, is how the simulator draws while a sketch blocks. The tests leave it unset. The test
`simulator/Basics` runs the `Basics` example for a second and compares the panel with
`simulator/Basics.txt`: the whole way from a sketch to the LEDs of an FC-16 board. The test
`simulator/web` runs it with `--web` on port 18080 and stands in for the browser with `curl` and
a WebSocket over bash's `/dev/tcp`: the page, the handshake, the panel after `hello`, and a
change of wiring. It connects while `Basics` shows "Hi!" and sends nothing, so that only an answer
to the page can bring a panel; a new example in its place needs such a pause as well.
`SimulatorWebSocketTest` checks the rest of the protocol a browser may use, against the examples
of the RFCs, `SimulatorWebHostTest` that a client which connects and says nothing cannot stop
the host (on port 18081, so it needs no lock with `simulator/web`), and `SimulatorTest` the
command line and the status line against the model, all once under the `simulator/` prefix: all of the simulator but `main.cpp` is one library, built
once, because it reads the model and never the library.

**A bug fix comes with a test that fails without the fix.** Put the bug back and run the
tests before committing; a test that stays green then does not test the bug. The comment above
the test names the bug it guards against.

The tests reach private members through `#define private public`, because some of what they
check has no public path — which frame a change to one buffer column turns into, for instance,
is checked by storing the bit with `MaxMatrixWired::storeBit()`, so that the test names the
buffer column itself rather than reaching it through an orientation. The parts are private
members of `MaxMatrix`, so a test reaches the buffer as `Matrix.Panel.Wired.MatrixBuffer`. The define comes after the GoogleTest and
standard headers, which it would rewrite as well.

### Hardware

Nothing here reaches real hardware. A change to the transfer code (chip select, SPI, bit
banging) is untested until it has run on a matrix, and the pull request says so.

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
