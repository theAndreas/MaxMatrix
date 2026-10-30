# Open Issue List

What is planned for MaxMatrix, in the order it can be done. A pull request that works on an
item sets its status and links itself in the same PR; an item is `done` once that PR is
merged. New items go at the end with the next free number, so a number never changes meaning.

Status: `open`, `in progress`, `done`, `dropped` (with the reason in the item).

## Items

| #  | Item | Depends on | Status | PR |
|----|------|------------|--------|----|
| 1  | Build with CMake | – | done | #5 |
| 2  | Tests on GoogleTest | 1 | done | #5 |
| 3  | MAX7219 model, driven from the wire | 2 | done | #6 |
| 4  | Module wiring variants in the model | 3 | done | #7 |
| 5  | Simulator that runs the example sketches | 3 | done | #9 |
| 6  | Web front end for the simulator | 5 | done | #10 |
| 7  | Simulator in CI | 6 | done | #11 |
| 8  | Batched updates in the library | 13 | done | #20 |
| 9  | Mirrored modules in the library | 25 | open | |
| 10 | Adafruit GFX as an option | 8 | open | |
| 11 | Wokwi as a complement | – | open | |
| 12 | Later ideas | 8 | open | |
| 13 | Split the library into modules | 3 | done | #13 |
| 14 | Chain length at run time | 13 | open | |
| 15 | Font format | 13 | open | |
| 16 | Static analysis in CI | 1 | open | |
| 17 | Documentation from the Doxygen blocks | – | open | |
| 18 | Releases and the Library Manager | 13 | open | |
| 19 | More of the MAX7219 in the public API | 3 | open | |
| 20 | More examples | 5 | open | |
| 21 | Control page for simulator and board | 6 | open | |
| 22 | Fast pin access for bit banging | 3 | open | |
| 23 | The library keeps the shift timing | 13 | open | |
| 24 | No pause for a skipped char | 2 | done | #12 |
| 25 | Matrix orientation apart from module orientation | 4 | done | #8 |

### 1 Build with CMake

Replace `test/Makefile` with CMake, building the bit banging and the SPI variant as before.
`cmake` and `ninja-build` go into the `Dockerfile`; CI and the build section of `CLAUDE.md`
switch to `cmake` / `ctest`. Check whether `arduino-lint --library-manager submit` accepts a
`CMakeLists.txt` in the root; if not, keep it under `test/` and `simulator/`.

*Done when* CI runs the same twelve tests on both variants through `ctest`, and
`compile_commands.json` gives cpptools the mock's include path and the variant's defines.

### 2 Tests on GoogleTest

Port `test/MaxMatrixTest.cpp` to GoogleTest from `libgtest-dev`, found with
`find_package(GTest)` and registered with `gtest_discover_tests`. The loops over
`Orientations[]` become `TEST_P`, the repeated `Mock::reset` / `initMatrix` a fixture.

- The library stays a target of its own built as `gnu++11`, because that is what `avr-gcc`
  compiles; only the tests are C++17 (GoogleTest 1.17 needs it).
- `#define private public` moves below every GoogleTest and standard header: defined above
  them, it rewrites their classes too.

*Done when* every former check has a test, and putting back the bugs the tests were written
for (see their comments) turns them red.

May go in one PR with 1.

### 3 MAX7219 model, driven from the wire

A model of one MAX7219 per module in `test/mock/`, fed by the frames the mock records: the
chain shifts each frame through, every chip latches its own address/value pair on the rising
chip select. Registers: no-op, digit 0–7, decode mode, intensity, scan limit, shutdown,
display test. Tests then check what is lit rather than which bytes went out.

Driven from the wire and not from `MatrixBuffer`, because the buffer only says what the
library believes is on the LEDs; a wrong module address, register index or frame boundary
leaves the buffer right and the display wrong.

### 4 Module wiring variants in the model

How a module maps digit and segment onto rows and columns, in the eight variants
MD_MAX72XX names `DR0CR0RR0_HW` … `DR1CR1RR1_HW`: digit is a row or a column, columns
reversed, rows reversed. Covers the common FC-16, generic, Parola and ICStation modules.

May go in one PR with 3.

### 5 Simulator that runs the example sketches

A host program that compiles an example from `examples/` unchanged against the mock, calls
`setup()` once and `loop()` on a timer, with `millis()` from the host clock. Bit banging and
SPI, as in the tests; the wiring variant from 4 selectable.

### 6 Web front end for the simulator

An HTTP and WebSocket server on `localhost:8080`, ported from Wordclock's
`platform/simulator/src/WebHost.cpp` (loopback only, polled from the simulator's tick rather
than a thread), and one HTML page: a canvas with the matrix and its brightness, the wiring
variant, and a log of the frames.

A browser rather than a wxWidgets window: no GUI packages in the container, it runs in every
dev container variant through VS Code's port forwarding, and a headless browser can check it
in CI, which Wordclock's window cannot (see its README, "Gaps").

May go in one PR with 5.

### 7 Simulator in CI

Build the simulator, start it, check with `curl` that the page answers. A Playwright
screenshot may follow later.

### 8 Batched updates in the library

Separate drawing from sending: today every `setDot` / `setColumn` sends a frame to the whole
chain at once (see `MaxMatrixWired::setColumn`). A mode that only updates the buffer and a call that
flushes it, as `control(UPDATE, OFF)` / `update()` in MD_MAX72XX, `display()` in
LEDMatrixDriver and `write()` in Max72xxPanel do. A full refresh is eight frames however much
was drawn, and animations stop flickering.

*Done when* a test counts the frames of a drawing in both modes, and flash and RAM of every
example on the Uno are in the PR.

As built, sending immediately stays the default, as in MD_MAX72XX: a sketch that never heard of
`update()` keeps working. What changed underneath is that no call sends a frame per dot or
column any more. Drawing only changes the buffer, which keeps which columns of a module changed;
every public function that draws sends those before it returns, one frame per changed column
for all modules. So a call sends eight frames at most, and nothing when it changed nothing.
`setAutoUpdate(false)` stops the sending until `update()`, for a picture made of many calls.
`MaxMatrixBatch` does the same for one block, as `std::lock_guard` holds a mutex: it puts back
what it found when the block ends, so nothing has to be turned on again. A default parameter
`Update = true` on every function that draws was weighed and dropped: with it,
`setColumn(Module, Column, Value)` became ambiguous with `setColumn(Column, Value, Update)` and
no longer compiled.
`writeRegister()` and its no-op frames are gone: nothing sends to a single module any more.

### 9 Mirrored modules in the library

`MaxMatrixModuleOrientationType` covers rotations only. The model in 4 shows four wirings
that are mirror images of the rotations and that no orientation reaches: `PAROLA` and the
unnamed `DR0CR0RR0`, `DR0CR1RR1`, `DR1CR0RR1`. Decide between a mirror setting next to the
orientation and the three-bit wiring description from 4 in place of it.

After 25, which gives the rotations their full set. Needs a mirrored module to try it on;
the PR says whether it ran on hardware.

### 10 Adafruit GFX as an option

Lines, circles, bitmaps and fonts through Adafruit GFX, switched by a
`MAXMATRIX_USE_ADAFRUIT_GFX` `STD_ON`/`STD_OFF`, as LEDMatrixDriver does with
`USE_ADAFRUIT_GFX`. Decide after measuring what it costs in flash on the Uno.

### 11 Wokwi as a complement

A `diagram.json` with a Uno and a `wokwi-max7219-matrix` (`chain`, layout `parola` / `fc16`),
to see the real AVR binary run, including the bit banging timing the mock cannot show. Free
for open source in VS Code; clarify whether `wokwi-cli` runs in CI without the Pro plan,
whose pricing page is the only one listing CI minutes.

A complement to the simulator, not a replacement: it has two of the eight wirings, no view of
the frames and no test framework.

### 12 Later ideas

Not planned yet, collected so they are not lost:

- Callbacks for the column shifted in and out, as `setShiftDataInCallback` /
  `setShiftDataOutCallback` in MD_MAX72XX.
- More than one row of modules, as `setPosition` in Max72xxPanel.
- Other architectures: an `SPIClass&` instead of the global `SPI`, and more than `avr` in
  `library.properties`.
- Text effects and zones, as in MD_Parola.

### 13 Split the library into modules

`MaxMatrix` does five things in one class of about 1500 lines: the transfer to the chain,
the buffer as wired, the coordinates as the viewer sees them, the font, and the text that
shifts through. The transfer alone is written out five times - `RegisterWrite` and the four
`set…LL` / `set…OnAllModulesLL` functions each carry their own frame loop with its own
`#if` on `MAXMATRIX_USE_SPI`.

One module per subject under `src/`, in the Arduino library format 1.5, as Wordclock's
`firmware/` is laid out:

```
src/
├── MaxMatrix.h   the facade: today's public API, unchanged
├── Max7219/      the transfer: frame, no-op, SPI or bit banging, written once
├── Matrix/       the buffer and the orientation: today's LL and public layer
├── Font/         SpriteTable, the latin-1 lookup, decodeChar
└── Text/         the shifting text as a state machine, using Font and Matrix
```

Dependencies point one way: `Text` → `Font` and `Matrix`, `Matrix` → `Max7219` →
`Arduino.h` / `SPI.h`. The two layers `CLAUDE.md` describes become class boundaries instead
of a discipline.

Not taken from Wordclock: swapping the hardware by header name (there is one hardware here,
and the mock already stands in at the level of `Arduino.h` and `SPI.h`), singletons and a
scheduler (every instance stays on its own, the sketch keeps calling `shiftTask()`), and
virtual functions (a vtable costs RAM on the Uno; the parts are composed at compile time).

*Done when* the public API is unchanged - `#include <MaxMatrix.h>` and every example compile
as before - the tests from 3 pass without a change to what reaches the wire, and the PR
gives flash and RAM of every example on the Uno before and after. Breaking the API would be
a 2.0 and a decision of its own.

After 3, because only the model's tests check what reaches the wire and so tell whether the
split changed anything; before 8, which is simpler once the buffer is a class of its own.

The `…LL` suffix goes with it. "Low level" names a height, not what differs: `setColumn` and
`setColumnLL` work in two coordinate systems, the viewer's and the modules' as wired, and
only `CLAUDE.md` tells a reader so. Once each layer is a class, both may be called
`setColumn` and the object says which is meant - as Adafruit GFX's `drawPixel` applies the
rotation and then writes the buffer. Where a name still has to tell them apart, it names the
frame of reference ("wired", as graphics says "device space" against "user space"), and the
translation becomes one function in the facade instead of a part of every public one:

```
MaxMatrix      viewer's coordinates   setColumn, setDot, shiftLeft …
  └─ WiredMatrix   buffer as wired    setColumn, setRow, shiftLeft …
       └─ Max7219Chain  transfer      writeRegister(Module, Address, Value), writeRegisterAll(…)
```

The `…LL` functions are private, so no sketch notices. The tests reach
`setColumnOnAllModulesLL` through `#define private public` today; they would test
`WiredMatrix` directly. `CLAUDE.md`'s "Two layers" and its `LL` naming rule change with it.

As built, three things differ from the plan above:

- **Names.** Every class starts with `MaxMatrix` (`MaxMatrixChain`, `MaxMatrixWired`,
  `MaxMatrixPanel`, `MaxMatrixFont`, `MaxMatrixText`). A sketch shares one namespace with
  every library it includes, and the mock already has a `Max7219Chain`.
- **A panel class.** The viewer's coordinates are `MaxMatrixPanel`, not the facade itself,
  because `Text` draws in them and would otherwise call back into the class that holds it.
- **No reference.** `MaxMatrixText` is handed the panel by each function that draws: a
  reference member cost 2 bytes of RAM and 86 bytes of flash in `ScrollText`.

### 14 Chain length at run time

The number of modules is `MAXMATRIX_NUMBER_OF_MODULES` in the header, so a chain of another
length means editing the library. MD_MAX72XX and LEDMatrixDriver take it in the constructor.
Open: a buffer allocated at run time or one the sketch hands in (LEDMatrixDriver allows
both), and what that costs on the Uno against today's fixed array.

### 15 Font format

`SpriteTable` is one built-in font. Open: variable-width characters, a font the sketch
supplies, a second smaller one - and whether to adopt MD_MAX72XX's font format and its
tooling rather than invent one. Whatever comes keeps the latin-1 lookup behind `decodeChar`.

### 16 Static analysis in CI

`clang-tidy` and `cppcheck` over the library, as Wordclock's `static-analysis.yml` does.
Needs the `compile_commands.json` from 1.

### 17 Documentation from the Doxygen blocks

Every function already carries a Doxygen block; generate HTML from them, for instance on
GitHub Pages, together with the pictures in `docu/`.

### 18 Releases and the Library Manager

Version tags, a changelog, and the submission to the Arduino Library Manager -
`arduino-lint --library-manager submit` already passes. After 13, so the first published
version has the API that is meant to stay.

### 19 More of the MAX7219 in the public API

Shutdown and display test as public functions, and the decode mode for 7-segment displays,
which LedControl offers. Today only `RegisterWrite` reaches them. The model from 3 already
knows the registers, so the tests come with it.

### 20 More examples

Besides `Basics` and `ScrollText`: an animation, sprites, more than one text. Each one is
compiled by CI on the Uno, and with 5 it can be watched in the simulator.

The libraries collected in `AndreasBur/MaxMatrix` under `LED/LED Anzeige/Max7219/` suggest
more: `MAX7219_8x8_matrix` has a clock, numbers, a mask and a rotation as examples, the old
Arduino `Matrix` / `Sprite` pair shows sprites as objects of their own. And a `SerialControl`
example that takes the commands from 21 over `Serial`.

### 21 Control page for simulator and board

`AndreasBur/Led-Matrix` had a Windows tool (VB.NET, WinForms and WPF) that drove the matrix
over a serial port: click an LED to set it, a text to show or to shift, sliders for intensity
and speed. The sketch on the other side (also `MaxMatrix/async/…/Sketch.cpp` in
`AndreasBur/MaxMatrix`) took one line per command: `text=`, `textShift=`, `intensity=`,
`speed=`, `space=` / `spaceBetweenChars=`, `setDot=column,row,value`.

The page from 6 can be that tool again, for both ends: against the simulator over the
WebSocket, and against a real board over the browser's Web Serial API (Chromium-based
browsers), with the `SerialControl` example from 20 on the board. One page, no installer,
no Windows. Inside the simulator, the commands reach the sketch through a simulated `Serial`.

### 22 Fast pin access for bit banging

`MAXMATRIX_USE_DIGITAL_WRITE_FAST` changes nothing as it stands. Measured on 2026-10-29
with `digitalWriteFast` 1.3.1 from the Library Manager (ArminJo's, which is what
`arduino-cli lib install digitalWriteFast` fetches):

- `digitalWriteFast` is only fast when the pin is a compile-time constant
  (`__builtin_constant_p`). The pins here are members set at run time, so every call falls
  back to `digitalWrite`: `Basics` is 4316 bytes with the switch on against 4318 off, and
  with `-DTHROW_ERROR_IF_NOT_FAST` the build fails on all four calls (now in `MaxMatrixChain.cpp`)
  ("Parameter for digitalWriteFast() function is not constant"). NicksonYap's variant of the
  header fails that way without being asked to.
- The dependency is not declared: `library.properties` has no `depends=digitalWriteFast`,
  and no test or CI job builds with the switch on.

The old TODO in `AndreasBur/MaxMatrix` already says "SPI und digitalwritefast testen".
Options: drop the switch; or look up port register and bit mask once in `init()`
(`portOutputRegister(digitalPinToPort(Pin))`, `digitalPinToBitMask(Pin)`) and write them
directly, which is fast with pins chosen at run time. Measure the frame time either way; the
mock from 3 checks that the bits stay the same.

### 23 The library keeps the shift timing

Every sketch times `shiftTask()` itself: `ScrollText` with `millis()`, the old sketches with
a TimerOne interrupt that set a flag. A speed setting in the library and a `shiftTask()` that
is called on every `loop()` and decides itself whether a step is due - as `setSpeed` and
`displayAnimate()` in MD_Parola - takes that out of every sketch. The interrupt must still
only set a flag: a transfer from inside an ISR would cut into an SPI transaction of the
sketch.

### 24 No pause for a skipped char

A char that is not in the font is skipped while shifting, but `stringShiftTask()` spends a
whole `shiftTask()` call on it and sends nothing, so the text stands still for one step per
unknown char. Found while porting the tests (#5): `"a\x01b"` takes one call more than `"ab"`
for the same frames. The fix is to read on to the next known char or the end of the string
within the same call.

*Done when* `UnknownChars` also compares the number of `shiftTask()` calls, and that check is
red without the fix. If 23 lands first, check whether its timing still shows the pause.

### 25 Matrix orientation apart from module orientation

Two settings, as first planned (the commented-out `MaxMatrixMatrixOrientationType` in
`MaxMatrix.h`):

- the module orientation 0, 90, 180, 270 turns every module in its place;
- the matrix orientation 0, 180 turns the whole panel, module order included, for a board
  mounted upside down.

Today the module orientation 180 does what the matrix orientation 180 should, as the model
in 4 showed: 90 and 270 turn each module, 180 the panel. So there is no way to turn a
module by 180 degrees in its place, and `GENERIC` modules with the input on the right
cannot be shown upright. With the split, the four rotations each fit one module
(`DR0CR0RR1`, `ICSTATION`, `GENERIC`, `FC16`), and every one of them also mounted upside
down. The module orientation 180 changes meaning; a sketch that uses it today switches to the
matrix orientation 180.

*Done when* `MaxMatrixModuleTest` covers all eight combinations of the two settings, each with
its wiring and input side, and the README table lists them.

## Decisions

Why it is planned this way rather than the obvious alternative.

- **Browser, not wxWidgets, for the simulator (6).** See the item.
- **CMake, not Bazel (1).** Bazel pays off for large multi-language builds; here it would
  bring its own toolchain and build language for three source files, in an ecosystem where
  nobody uses it. CMake gives `ctest`, `compile_commands.json` and the same layout as
  Wordclock.
- **GoogleTest, not the own harness or doctest (2).** The own harness reports only that a
  check failed, not with which values or in which orientation. doctest would avoid the
  dependency, but GoogleTest comes from apt (no download through the proxy) and integrates
  with `ctest` and VS Code's test view without extra scripts. Wordclock keeps its own harness
  for reasons that do not apply here: its tests depend on their order through singletons, and
  the same cases are built by two harnesses without CMake.
