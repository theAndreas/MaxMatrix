# Open Issue List

What is planned for MaxMatrix, in the order it can be done. A pull request that works on an
item sets its status and links itself in the same PR; an item is `done` once that PR is
merged. New items go at the end with the next free number, so a number never changes meaning.

Status: `open`, `in progress`, `done`, `dropped` (with the reason in the item).

## Items

| #  | Item | Depends on | Status | PR |
|----|------|------------|--------|----|
| 1  | Build with CMake | – | open | |
| 2  | Tests on GoogleTest | 1 | open | |
| 3  | MAX7219 model, driven from the wire | 2 | open | |
| 4  | Module wiring variants in the model | 3 | open | |
| 5  | Simulator that runs the example sketches | 3 | open | |
| 6  | Web front end for the simulator | 5 | open | |
| 7  | Simulator in CI | 6 | open | |
| 8  | Batched updates in the library | 13 | open | |
| 9  | Mirrored modules in the library | 4 | open | |
| 10 | Adafruit GFX as an option | 8 | open | |
| 11 | Wokwi as a complement | – | open | |
| 12 | Later ideas | 8 | open | |
| 13 | Split the library into modules | 3 | open | |

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
chain at once (see `setColumnLL`). A mode that only updates the buffer and a call that
flushes it, as `control(UPDATE, OFF)` / `update()` in MD_MAX72XX, `display()` in
LEDMatrixDriver and `write()` in Max72xxPanel do. A full refresh is eight frames however much
was drawn, and animations stop flickering.

*Done when* a test counts the frames of a drawing in both modes, and flash and RAM of every
example on the Uno are in the PR.

### 9 Mirrored modules in the library

`MaxMatrixModuleOrientationType` covers the four rotations; a module wired mirrored is
probably not reachable with any of them. Decide between adding the mirror and adopting the
three-bit wiring description from 4.

Needs a mirrored module to try it on; the PR says whether it ran on hardware.

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
