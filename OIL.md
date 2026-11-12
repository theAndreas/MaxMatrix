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
| 9  | Mirrored modules in the library | 25 | in progress | #61 |
| 10 | Adafruit GFX as an option | 8 | done | #62 |
| 11 | Wokwi as a complement | – | done | #40 |
| 12 | Later ideas | 8 | open | |
| 13 | Split the library into modules | 3 | done | #13 |
| 14 | Chain length at run time | 13 | done | #60 |
| 15 | Font format | 13 | done | #42, #58 |
| 16 | Static analysis in CI | 1 | done | #36 |
| 17 | Documentation from the Doxygen blocks | – | done | #39 |
| 18 | Releases and the Library Manager | 13 | open | |
| 19 | More of the MAX7219 in the public API | 3 | done | #55 |
| 20 | More examples | 5 | done | #38 |
| 21 | Control page for simulator and board | 6 | done | #28 |
| 22 | Fast pin access for bit banging | 3 | in progress | #57 |
| 23 | The library keeps the shift timing | 13 | done | #22 |
| 24 | No pause for a skipped char | 2 | done | #12 |
| 25 | Matrix orientation apart from module orientation | 4 | done | #8 |
| 26 | Control program for Linux and Windows | 20 | in progress | #51, #52, #56 |
| 27 | Errors that say what went wrong | 13 | done | #29, #30, #31, #32, #33 |
| 28 | Errors and containers from Utl | 27 | dropped | #43 |
| 29 | The real binary against the model, in simavr | 3 | done | #41 |
| 30 | Documentation in the style of Utl | 17 | done | #47, #48, #50 |
| 31 | A shifting text that starts over | 23 | done | #63 |
| 32 | The column shifted in and out | 12 | done | #66 |
| 33 | A font checked against its length | 15 | done | #67 |
| 34 | On the template embedded-cpp-template | 16, 30 | done | #69 |
| 35 | Version 2.0.0: a namespace, and constants for macros | 34 | done | #70, #71 |
| 36 | The template's checks, one group at a time | 35 | in progress | #73, #74, #78, #79, #80, #82 |
| 37 | Files named as their class | 35 | in progress | #77 |

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
`Orientations[]` become `TEST_P`, the repeated `mock::Reset` / `initMatrix` a fixture.

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

Driven from the wire and not from `matrix_buffer_`, because the buffer only says what the
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

Separate drawing from sending: today every `SetDot` / `SetColumn` sends a frame to the whole
chain at once (see `MaxMatrixWired::SetColumn`). A mode that only updates the buffer and a call that
flushes it, as `control(UPDATE, OFF)` / `Update()` in MD_MAX72XX, `Display()` in
LEDMatrixDriver and `write()` in Max72xxPanel do. A full refresh is eight frames however much
was drawn, and animations stop flickering.

*Done when* a test counts the frames of a drawing in both modes, and flash and RAM of every
example on the Uno are in the PR.

As built, sending immediately stays the default, as in MD_MAX72XX: a sketch that never heard of
`Update()` keeps working. What changed underneath is that no call sends a frame per dot or
column any more. Drawing only changes the buffer, which keeps which columns of a module changed;
every public function that draws sends those before it returns, one frame per changed column
for all modules. So a call sends eight frames at most, and nothing when it changed nothing.
`SetAutoUpdate(false)` stops the sending until `Update()`, for a picture made of many calls.
`MaxMatrixBatch` does the same for one block, as `std::lock_guard` holds a mutex: it puts back
what it found when the block ends, so nothing has to be turned on again. A default parameter
`Update = true` on every function that draws was weighed and dropped: with it,
`SetColumn(module, column, value)` became ambiguous with `SetColumn(column, value, update)` and
no longer compiled.
`writeRegister()` and its no-op frames are gone: nothing sends to a single module any more.

### 9 Mirrored modules in the library

`MaxMatrixModuleOrientation` covers rotations only. The model in 4 shows four wirings
that are mirror images of the rotations and that no orientation reaches: `PAROLA` and the
unnamed `DR0CR0RR0`, `DR0CR1RR1`, `DR1CR0RR1`. Decide between a mirror setting next to the
orientation and the three-bit wiring description from 4 in place of it.

After 25, which gives the rotations their full set. Needs a mirrored module to try it on;
the PR says whether it ran on hardware.

As built, the three-bit description replaces the orientation: `SetModuleType()` takes a
`MaxMatrixModuleType`, the eight wirings as MD_MAX72XX names them, `kDr0Cr0Rr0` to `kDr1Cr1Rr1`,
with `kFc16`, `kGeneric`, `kParola` and `kIcstation` for the four that have a name. A mirror
setting would have kept the README's table of which rotation fits which module, then with eight
rows; a module type is what a sketch knows of its module, and what MD_MAX72XX already calls it.
`ConvertDotToBuffer()` follows the three bits, as the model does, rather than a case per
rotation, which made every example 12 bytes smaller. `MaxMatrixModuleTest` checks every type,
upright and upside down, on the wiring of its name, and the four pictures of `docu/` became
eight; the four that the rotations reached are the same LED for LED. Not tried on a mirrored
module.

### 10 Adafruit GFX as an option

Lines, circles, bitmaps and fonts through Adafruit GFX, switched by a
`MAXMATRIX_USE_ADAFRUIT_GFX` `STD_ON`/`STD_OFF`, as LEDMatrixDriver does with
`USE_ADAFRUIT_GFX`. Decide after measuring what it costs in flash on the Uno.

Measured on the Uno: a sketch that draws a line, a circle and a filled rectangle through a class
on top of `Adafruit_GFX` took 7522 bytes of flash and 261 of RAM, against 1564 and 75 for the same
sketch setting two dots; the same three shapes written on `SetDot()` took 1822 and 75. GFX costs
about 6 KB of flash and 186 bytes of RAM: the vtable of `Adafruit_GFX` links every one of its
virtual functions, called or not, and avr-gcc keeps a vtable in RAM beside the object. A switch
would not have helped: the Arduino IDE passes a sketch's defines to no library.

As built, without GFX: `DrawLine()`, `DrawRect()`, `FillRect()`, `DrawCircle()` and
`DrawBitmap()` in `MaxMatrixPanel`, through the facade, in the viewer's coordinates and leaving
out what lies off the panel, as `SetDot()` does. A bitmap is a column per byte in `PROGMEM`, as a
glyph is, and the text draws its glyphs and sprites through the same `DrawColumn()`. The fonts of
GFX are not missed on a panel eight dots high, which has `SetFont()`. The new example `Shapes`
shows all of it, 4124 bytes of flash; examples that draw no shapes grew by 18 to 38 bytes, for the
check that keeps a sprite at the right edge from overflowing an `int`.

### 11 Wokwi as a complement

A `diagram.json` with a Uno and a `wokwi-max7219-matrix` (`chain`, layout `parola` / `fc16`),
to see the real AVR binary run, including the bit banging timing the mock cannot show. Free
for open source in VS Code; clarify whether `wokwi-cli` runs in CI without the Pro plan,
whose pricing page is the only one listing CI minutes.

A complement to the simulator, not a replacement: it has two of the eight wirings, no view of
the frames and no test framework.

As built, `diagram.json` and `wokwi.toml` in the root put a Uno and four FC-16 modules on pins 11,
10 and 13, as the examples use them, and `tools/wokwi.py EXAMPLE [--spi]` builds an example into
the one file `wokwi.toml` names. It is there to look at, in the VS Code extension, not in CI.

`wokwi-cli` does run in CI on the free plan, with a token of a Wokwi account as a secret: 50
minutes of simulated time in 30 days, five per run. It could even compare a screenshot of the
matrix with a picture at a fixed simulated time. But a check that needs an account and a budget,
and fails when Wokwi draws its LEDs differently, is worse than the one in 29, which needs neither.

### 12 Later ideas

Not planned yet, collected so they are not lost:

- Callbacks for the column shifted in and out, as `setShiftDataInCallback` /
  `setShiftDataOutCallback` in MD_MAX72XX: became item 32, without callbacks.
- More than one row of modules, as `setPosition` in Max72xxPanel.
- Other architectures: an `SPIClass&` instead of the global `SPI`, and more than `avr` in
  `library.properties`.
- Text effects and zones, as in MD_Parola. A shifting text that starts over by itself became
  item 31.

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
scheduler (every instance stays on its own, the sketch keeps calling `ShiftTask()`), and
virtual functions (a vtable costs RAM on the Uno; the parts are composed at compile time).

*Done when* the public API is unchanged - `#include <MaxMatrix.h>` and every example compile
as before - the tests from 3 pass without a change to what reaches the wire, and the PR
gives flash and RAM of every example on the Uno before and after. Breaking the API would be
a 2.0 and a decision of its own.

After 3, because only the model's tests check what reaches the wire and so tell whether the
split changed anything; before 8, which is simpler once the buffer is a class of its own.

The `…LL` suffix goes with it. "Low level" names a height, not what differs: `SetColumn` and
`setColumnLL` work in two coordinate systems, the viewer's and the modules' as wired, and
only `CLAUDE.md` tells a reader so. Once each layer is a class, both may be called
`SetColumn` and the object says which is meant - as Adafruit GFX's `drawPixel` applies the
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

Decided on 2026-11-06: the sketch holds the buffer. Not one allocated at run time, which brings
the heap of the Uno and a way to fail; not the whole library as a template, which would put every
part into headers and compile it once per length.

As built, `MaxMatrix` takes a buffer and the number of modules, and `MaxMatrixOf<N>` brings the
buffer for N modules in a base class, made before the `MaxMatrix` it is handed to, and refuses a
length outside 1 to 31 when the sketch is compiled; every example is a `MaxMatrixOf<4>` now, and
`MAXMATRIX_NUMBER_OF_MODULES` is gone. No array of the library is sized by the chain any more: the
shifts work a column at a time in place; an orientation changes a pair of modules at a time, m and
n - 1 - m, which hold their own picture in every orientation, in 16 bytes on the stack for any
chain; `Send()` hands the chain a column of every module straight from the buffer. A row of the
whole panel is an array the sketch passes, whose length the template of `GetRow()` and `SetRow()`
takes from its type, rather than `MaxMatrixRow`, a reference so that `GetRow(module, 0)` stays
unambiguous. The simulator draws as many modules as its model has; the control program, the
tests and `docu/` name their chain of four themselves. The tests run again over chains of 1, 3
and 6 modules in all eight orientations.

What it costs on the Uno, against main: RAM +3 bytes, the pointer to the buffer and the number of
modules. Flash: Animation 2318 to 2426, Basics 3584 to 3730, Messages 3704 to 3726, ScrollText
3638 to 3658, SerialControl 6124 to 6076, Sprites 2378 to 2444 bytes; the length in a local
constant in each loop rather than read again after every store through the buffer, and memset()
to empty it, took 80 to 110 bytes off what the first version cost.

### 15 Font format

`SpriteTable` is one built-in font. Open: variable-width characters, a font the sketch
supplies, a second smaller one - and whether to adopt MD_MAX72XX's font format and its
tooling rather than invent one. Whatever comes keeps the latin-1 lookup behind `DecodeChar`.

The font now has the ß and the degree sign as well, `0xDF` and `0xB0` in latin-1. Without them
`SetText("Grüße")` failed and a shifting "Grüße" or "21 °C" lost the char without a word, as
`Messages` showed when it was written. They cost 30 to 38 bytes of flash in the examples that
draw text, none of RAM. The € was left out: it is not in latin-1, and `DecodeChar()` reads two
bytes of UTF-8, not the three it takes.

Decided on 2026-11-06: the font format of MD_MAX72XX, rather than a format of our own with an index
of each char's place, which no font and no tool would have come with, or that of Adafruit GFX, whose
bitmaps lie in rows with seven bytes of header per char and fit an 8-row matrix badly.

As built, a font is a `PROGMEM` array in that format, version 1 or 2, and `SetFont()` sets the one
every char is drawn in; `CheckFont()` refuses an array that does not start as a font with
`kNotAFont`. The sprite table became `kMaxMatrixDefaultFont` in the same format, converted once
with the same look, the chars at their latin-1 codes and a width of 0 between them: 614 bytes
against 721. A char is found by walking from the first to it, as MD_MAX72XX does: 34 us for an A
and 211 us for an ü on the Uno, measured in simavr; an index would cost two bytes per char. A char of a font is a
`MaxMatrixGlyph`, its columns in flash, so it may be wider than a sprite's five columns; a sprite
of the sketch stays a `MaxMatrixSprite`. `GetSprite()` by an index into the table is gone with the
table. The text keeps `NULL` for the default font, so that a sketch that draws no char does not
link it: Animation stays 2282 bytes. Basics 3608 to 3548, Messages 3742 to 3698, ScrollText 3668
to 3632, SerialControl 6048 to 6088, Sprites 2314 to 2342 bytes of flash; RAM as it was, as the
font pointer and the glyph take the seven bytes the sprite buffer took. The count of a shifting
char's columns became 16 bits, as with a space of 255 it never reached a char's width and space.
A second, smaller font is not shipped: any font of MD_MAX72XX serves.

### 16 Static analysis in CI

`clang-tidy` and `cppcheck` over the library, as Wordclock's `static-analysis.yml` does.
Needs the `compile_commands.json` from 1.

As built, `.github/workflows/static-analysis.yml` has Wordclock's checks and its jobs:
clang-tidy over the host build's database, which holds the library in all three variants and
the simulator; clang-tidy over the database arduino-cli writes for the Uno, once with
`MAXMATRIX_DEV_ERROR_DETECT` off and once on, read through `tools/cross-clang-tidy.py` so that
clang sees avr-libc and a 16-bit int; and cppcheck over `src/` and `simulator/` against the
mock. `.clang-tidy` says why each check that is off is off: the include guards, the
coordinates side by side, a byte counting the columns, and the simulator's prints and statics.

What the checks found was fixed rather than switched off where a fix was cheap:

- `--for` of the simulator went through `atof()`, which took `2x` as two seconds and passed a
  negative number to a cast to `uint32_t` that is undefined for it; it is checked now, as
  `--port` is, with a test.
- Nothing kept the chain under 32 modules, past which a byte no longer counts the columns;
  `MaxMatrixConfig.h` asserts it now.
- Five enums got `uint8_t` as their base, as `MaxMatrixError` has: 44 to 80 bytes of flash
  and 3 of RAM in every example.
- `SpriteTable` is an array of sprites rather than of bytes, the getters are `const`, functions
  without state `static`, and the unused bit macros of `MaxMatrixStandardTypes.h` are gone.

cppcheck found nothing; that it reads the tree was checked with a write past `matrix_buffer_`.

### 17 Documentation from the Doxygen blocks

Every function already carries a Doxygen block; generate HTML from them, for instance on
GitHub Pages, together with the pictures in `docu/`.

As built, `.github/workflows/docs.yml` runs Doxygen over `src/`, with the README as the main page
and `docu/README.md` with the pictures of the wirings as the page of `docu/`, and publishes the
HTML on GitHub Pages from `main`. Every pull request builds it, and `tools/docs.py` fails on any
warning but a member left undocumented below the facade.

Every function did not carry a block after all: the one-line functions of the facade in
`MaxMatrix.h` had none, nor did the configuration, the error classes and the enums - what a sketch
reads most. They have one now; a function that only passes a call on takes the block below with
`\copydoc`. `\return -` went from every function that returns nothing, as Doxygen warns of it,
and the README's links to the examples point at GitHub, as Pages has no copy of them.

### 18 Releases and the Library Manager

Version tags, a changelog, and the submission to the Arduino Library Manager -
`arduino-lint --library-manager submit` already passes. After 13, so the first published
version has the API that is meant to stay.

A version is tagged only once `docu/HardwareCheck.md` has passed on a matrix, and the release says on
which board.

### 19 More of the MAX7219 in the public API

Shutdown and display test as public functions, and the decode mode for 7-segment displays,
which LedControl offers. Today only `RegisterWrite` reaches them. The model from 3 already
knows the registers, so the tests come with it.

As built, `SetShutdown()` and `SetDisplayTest()`, both sent at once whatever auto update is, and
both leaving the buffer and the intensity alone, so that the picture is back when they end. The
decode mode got no function: it turns a digit's byte into the segments of a 7-segment digit, so
on a dot matrix the LEDs no longer show the buffer, which every getter and every drawing function
rely on. `RegisterWrite()` still reaches it for whoever drives a 7-segment display.

That the LEDs really go dark and light up is checked on a matrix with the example `Shutdown`, as
`docu/HardwareCheck.md` lists it; the registers it writes are the model's, tested against the
datasheet.

### 20 More examples

Besides `Basics` and `ScrollText`: an animation, sprites, more than one text. Each one is
compiled by CI on the Uno, and with 5 it can be watched in the simulator.

The libraries collected in `AndreasBur/MaxMatrix` under `LED/LED Anzeige/Max7219/` suggest
more: `MAX7219_8x8_matrix` has a clock, numbers, a mask and a rotation as examples, the old
Arduino `Matrix` / `Sprite` pair shows sprites as objects of their own. And a `SerialControl`
example that takes the commands from 21 over `Serial`.

`SerialControl` is built. The commands are a class of the library, `MaxMatrixCommand`, rather
than code in the example: so they are tested with GoogleTest, and 21, 26 and the simulator rely
on one implementation of the lines. The sketch feeds it the chars from `Serial` and answers each
line with `ok` or `error`, at 115200 baud. Of the old tool's names, `spaceBetweenChars=` is left
out beside `space=`; `clear` is new.

`Animation`, `Sprites` and `Messages` are built as well. `Animation` bounces a ball off the edges
without blocking `loop()`, each step one picture through `MaxMatrixBatch`; `Sprites` walks a sprite
of its own with two frames across the panel, from beyond one edge to beyond the other; `Messages`
scrolls several texts in turn, each at its own speed, one of them with umlauts. Each runs for two
seconds in the simulator in CI, `simulator/run/<example>`, which fails on a frame the chain cannot
take; what they draw is not compared, as it depends on the host's clock. The clock, the numbers and
the mask of `MAX7219_8x8_matrix` were left out: they show nothing of the library the six do not.

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
A program of its own beside the page, for Firefox users and without a browser, is 26.

As built, every command goes to the simulator and to the board at once, whichever is
connected, so with `SerialControl` in both the panel shows what the board should. The page
sends `serial LINE` over the WebSocket, the simulator appends the line to the sketch's `Serial`,
and what the sketch prints returns with the next panel; an answer that lights nothing asks for
a panel of its own rather than waiting for the next frame. A click on an LED sends `setDot=`.
The page is served by the simulator on `localhost`, which Web Serial takes as secure, so it
drives a board alone as well; a page on GitHub Pages would be 17. Opening the port resets an
Uno, so nothing goes to the board until `SerialControl` has said `ready`. The Web Serial side is
tested against a fake port under Node, not yet with a board.

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
Options: drop the switch; or look up port register and bit mask once in `Init()`
(`portOutputRegister(digitalPinToPort(Pin))`, `digitalPinToBitMask(Pin)`) and write them
directly, which is fast with pins chosen at run time. Measure the frame time either way; the
mock from 3 checks that the bits stay the same.

As built, the second, and the switch is gone. `Init()` looks up port and bit of each pin, and a
byte goes out through the port registers with the interrupts held off, as `digitalWrite()` holds
them off; the register's address is read where it is written, since the core reads it from
flash with a statement expression that `decltype()` cannot take outside a function. In simavr a
frame of four modules took 12 600 cycles through `shiftOut()` and takes 2 600 to 3 500 now, about
four times as fast; flash grew by 6 to 36 bytes per example, RAM by 6. simavr's SPI is no faster
than the old bit banging, so it says nothing of the SPI path, whose code did not change but for
chip select. The mock now follows the pins bit by bit, as SimulatorAvr does, and counts a port
written with the interrupts on as an error. Not run on hardware yet.

### 23 The library keeps the shift timing

Every sketch times `ShiftTask()` itself: `ScrollText` with `millis()`, the old sketches with
a TimerOne interrupt that set a flag. A speed setting in the library and a `ShiftTask()` that
is called on every `loop()` and decides itself whether a step is due - as `setSpeed` and
`displayAnimate()` in MD_Parola - takes that out of every sketch. The interrupt must still
only set a flag: a transfer from inside an ISR would cut into an SPI transaction of the
sketch.

As built, `SetShiftInterval()` takes milliseconds per column, as `setSpeed` in MD_Parola does,
and starts at `MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE`, 50. So the library keeps the pace out of
the box; a sketch that timed itself at 50 ms or slower, as `ScrollText` and the README did,
behaves as before, and one that wants a step on every call sets 0. The first step comes on the
first call after `SetTextWithShift()`; a late call takes one step and does not catch up, so
that the text never jumps after `loop()` was held up. Starting a text over by itself was left
for 12, beside the text effects.

### 24 No pause for a skipped char

A char that is not in the font is skipped while shifting, but `StringShiftTask()` spends a
whole `ShiftTask()` call on it and sends nothing, so the text stands still for one step per
unknown char. Found while porting the tests (#5): `"a\x01b"` takes one call more than `"ab"`
for the same frames. The fix is to read on to the next known char or the end of the string
within the same call.

*Done when* `UnknownChars` also compares the number of `ShiftTask()` calls, and that check is
red without the fix. If 23 lands first, check whether its timing still shows the pause.

### 25 Matrix orientation apart from module orientation

Two settings, as first planned (the commented-out `MaxMatrixMatrixOrientation` in
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

### 26 Control program for Linux and Windows

The control of 21 as a program of its own, beside the page: for a browser without Web
Serial, and for whoever wants a program rather than a page. wxWidgets in C++, as Wordclock's
simulator window. It drives a board over its serial port and the simulator over TCP, with the
lines of 21 (`text=`, `textShift=`, `intensity=`, `speed=`, `space=`, `setDot=column,row,value`),
so that to the program both ends look alike, and to the sketch the program and the page.

- **Serial port:** a small class of its own, over `termios` on Linux and
  `CreateFile`/`SetCommState` on Windows, rather than libserialport: one port, text only, and
  no further dependency.
- **Simulator:** a plain TCP port beside the WebSocket of 6, which takes the same lines as the
  simulated `Serial` of 21. wxWidgets has `wxSocketClient`, but no WebSocket.
- **A thin window:** the commands, the protocol and the connections are classes without
  wxWidgets, tested with GoogleTest like the rest. The window itself is checked by hand, or
  under `xvfb-run` in CI.
- **Dev container:** wxWidgets from apt in the `Dockerfile`; showing the window needs a
  display from the host, which WSLg gives the `wsl2` variant and the `linux` variant has to
  forward.

How it is shipped, a Windows `.exe` and an AppImage, is decided with 18.

As built, the window follows the old tool's layout, in English as the rest of the repository, but
for its two tabs: every LED of the panel on top, rather than one module at a time, the text with the
intensity and speed sliders below it, and on the right the board, the simulator and a log. The old
tool only sent; the panel here shows what the board shows, as SerialControl answers `picture`, up
to twenty times a second, and a SerialControl that starts takes the window's settings, so that the
window and the matrix agree. For that `MaxMatrixCommand` answers every line itself, on a `Print`,
rather than leaving `ok` and `error:` to the sketch, which would have had to know that `picture`
answers in their place; SerialControl grew by 152 bytes of flash and no RAM. Then a stroke with the
button held paints every LED it passes, and the field counts the bytes the text may still take, so
that a line too long is not sent. Like the page,
it sends every command to both ends, whichever is connected, and nothing to a board until
SerialControl has said `ready`; the simulator's sketch said so before the program connected, so
there it is not waited for. The simulator's TCP port is `--tcp PORT`, in both simulators, with one
client at a time. Opened on Linux against the simulator, under Xvfb: text, LEDs and a simulator that
stops. Not yet on Windows, where only the serial port and socket code are compiled, with MinGW, and
not yet with a board.

*Done when* the program drives a board running `SerialControl` and the simulator, built from
the same CMake on Linux and Windows, its classes without a window have tests, and the PR says
on which systems and boards it ran.

After 20, which defines the lines and `SerialControl`; beside 21, which uses the same lines.

### 27 Errors that say what went wrong

`stdReturnType` says only that a call failed, and as `E_OK` is 0, `if(matrix.SetDot(…))` is
true on an error. Five reasons lie behind `E_NOT_OK` today: a coordinate, module or intensity
out of range, a char the font lacks, a `NULL` text, and in `MaxMatrixCommand` a value that is
empty, no number or too large. The command answers all of them with
`MAXMATRIX_COMMAND_INVALID_VALUE`, and no example checks a return value at all.

- **One flat enum of one byte**, `MaxMatrixError`, in a wrapper with `IsOk()` and no
  conversion to `bool`. A getter of a single value returns the value and the error together
  (`MaxMatrixResult<T>`) rather than through a pointer; both fit into registers on the AVR.
  `GetRow()` and `GetSprite()` keep their pointer, as C++ cannot return an array.
- **`GetErrorText()`** returns the text from `PROGMEM` as a `__FlashStringHelper const*`, which
  `Serial.print` reads from flash. A sketch that does not call it pays nothing; the linker
  drops it. `MaxMatrixCommand` passes the error on, so that `SerialControl` and the page of 21
  can say what was wrong.
- **`MAXMATRIX_DEV_ERROR_DETECT`**, `STD_OFF` by default, as the DET of AUTOSAR Classic: where
  a check fails, the library reports the function and the error to a `weak`
  `MaxMatrixReportError()`, so that a sketch which checks nothing still learns where it went
  wrong. The default prints on `Serial`, a sketch may define its own. `SerialControl` keeps the
  default, as the page shows any line and reads only `ready`. Switched off, the preprocessor removes every call. The Arduino IDE
  has no debug build and does not pass a sketch's defines to the library, so it is switched in
  `MaxMatrixConfig.h` or with `--build-property "compiler.cpp.extra_flags=-D…"`
  (`build.extra_flags` carries the USB flags of some boards). The tests build a third variant
  with it on and check that every error is reported, once, under the name of its function.

Drawing clips, decided first because it decides how many errors there are: `SetDot()`,
`SetColumn()` and `SetRow()` take `int16_t` as `SetChar()` does, leave out what lies off the
panel and return nothing, as `embedded-graphics` in Rust and Adafruit GFX's `drawPixel()` do.
Out of range is left for the getters, the intensity and the commands; `MaxMatrixCommand` checks
the coordinates it is sent itself, so that a typed line that would do nothing says so.

In four steps, a PR each: drawing clips; the enum, the wrapper and `MaxMatrixResult<T>`;
`GetErrorText()` with the reason in the answer of `SerialControl`, after 21 is merged, as the
page of 21 reads those answers; `MAXMATRIX_DEV_ERROR_DETECT`.

The alternatives weighed, AUTOSAR's error domains and Rust's enums among them, and what each
costs on the Uno are in `docu/ErrorHandling.md`.

*Done when* every function that can fail says why, the tests check each reason in both
variants and with the switch on, and the PR gives flash and RAM of every example against
today.

### 28 Errors and containers from Utl

27 set error domains aside, as their vtables cost RAM and keep their texts linked. The library
[Utl](https://github.com/theAndreas/Utl) has since built the domain by its address out on its
own, and found a way around both: a GCC plugin that moves the vtables into flash, leaving 2 bytes
of RAM a group, and a switch that leaves the texts out. It also brings what avr-gcc lacks of the
standard library: `array`, `span`, `optional`, `string_view`, and a `flash_string_view` that
compares a line in RAM with a keyword in flash. What it showed is in `docu/ErrorHandling.md`,
"Tried out: the domain by its address, in Utl".

To decide is whether MaxMatrix takes it over:

- `MaxMatrixStatus` and `MaxMatrixResult<T>` would become `utl::Status` and `utl::Result<T>`, with
  a group `MaxMatrixError` of its own. Errors from outside the library, a sketch's own commands or
  a transfer that can fail, would then pass through the same type.
- `MaxMatrixCommand` compares the lines it is sent with `strcmp_P()` by hand; `string_view` and
  `flash_string_view` would do that, and `span` could carry the picture buffers.
- Against it: a dependency (`depends=Utl`) for every user, a style apart from the conventions
  here, as Utl follows Google's, and the RAM of a vtable per group for a sketch built without the
  plugin.

After Utl is released, as a dependency has to come from the Library Manager. *Done when* it is
decided, and if taken, the examples' flash and RAM against today are in the PR.

Dropped, after a prototype that was not merged; its numbers are in `docu/ErrorHandling.md`, "Tried
out: MaxMatrix on Utl". The vtable and the texts linked into every sketch that draws text, 144 to
166 bytes of flash, are gone with Utl's groups without `virtual` (Utl #24, #25): a sketch that
prints no error is where it is today. So are the warnings at every dropped status (Utl #27) and the
mix of names (#44). What stays: a sketch that prints errors pays more, `SerialControl` 378 bytes of
flash and 8 of RAM, as an error is 3 bytes where an enum is one and the texts are looked up at run
time; and every user installs Utl. What it buys, one type for errors from several sources, has no user here, as
every error comes from the library itself.

Taken up again when an error comes from outside the library, from commands a sketch registers or a
transfer that can fail, such as I²C with a NACK; the wrapper is kept so that a sketch would not
change. The containers alone, `string_view` and `flash_string_view` for the commands, were not
measured and could be taken without the errors.

### 29 The real binary against the model, in simavr

The tests and the simulator build the library with the host's `g++`, against the mock; what the
Uno runs - `avr-gcc`'s code, the bit banging with `shiftOut()` and the SPI peripheral - is only
compiled, never run. simavr runs it: the `.elf` of an example, on a simulated ATmega328P, as Utl
runs its `SelfCheck`. A program on the host would load it with simavr as a library, hang the
MAX7219 model of `test/mock/Max7219.h` on the pins (DIN, CS and CLK, or the SPI output), and
compare what lights up with an expectation in the format of `simulator/Basics.txt`, for any of the
eight wirings and both transfer paths.

Simulated time is exact, so a moving example can be checked at a fixed time as well, which the
simulator on the host clock cannot. It needs no account, unlike Wokwi in CI (11). It does not
replace a run on a board: the timing of a real MAX7219 and its wiring are not simulated.

As built, `build/SimulatorAvr --elf FILE` is the simulator of 5 for the `.elf`: simavr runs it, and
its pins, its SPI output and its UART drive the `mock::` variables the Arduino stand-in drives, so
that the panel, the terminal and the page work unchanged, `SerialControl` included. The model now
shifts bit by bit, as the MAX7219 does, since bit banging clocks single bits; a byte is eight of
them. CMake builds every example with `arduino-cli`, with bit banging and with SPI, and the tests
`avr/` compare each panel at a simulated time with `simulator/EXAMPLE.txt`: both transfer paths
light the same, and the panels are the ones the examples are meant to show. Bit banging with the
low bit first fails all six of its tests. The CI job "Examples in simavr" runs them.

### 30 Documentation in the style of Utl

The comments followed a style of their own: banners around every section and function, a name
after every closing brace, and the Doxygen blocks of a function at its definition in the `.cpp`,
where a sketch's author does not look, in columns and in lower case. Utl has a style written
down in its `docs/Documentation.md`; MaxMatrix takes it, so that every project documents alike.

In steps, each its own pull request: the banners go and the header of a file becomes the one of
Utl, and the rules move into `docu/Documentation.md`, with clang's `-Wdocumentation` reading
every block; then the blocks of the facade, of `Command/`, of the errors and the configuration
move into the headers and are written anew; then those of the parts below; then those of the
simulator.

The facade and the parts below went in one step, #48, not two: the facade takes most of its texts
from MaxMatrixPanel and MaxMatrixFont with `\copydoc`, and would have shown their old texts until
the next one.

### 31 A shifting text that starts over

`ScrollText` started its text again itself, whenever the state was `MaxMatrixState::kReady`; a
sketch that scrolls a text without end, the most common one, had to know that. Taken out of 12,
where the rest of its ideas stay.

As built, `SetTextWithShift(text, true)` starts the text over with its first char in the same step
that would have ended it, so no column comes between its end and its start, where the restart in
the sketch had one; a few spaces at the end of the text keep the two apart. `StopShift()` ends it,
or any shift, with the picture as it stands, which a text without end needs and no function did
before. A text without a char of the font ends at once rather than reading itself over and over.
It costs 2 bytes of RAM for the start of the text, in every sketch, and 4 bytes of flash in a
sketch that shifts no text. `ScrollText` became 44 bytes smaller without its own restart,
`SerialControl` 42 larger, and `Messages` 124, of which 80 because avr-gcc now inlines
`SetTextWithShift()` at both calls of `Show()`, as an attribute `noinline` showed; one was not
added for a choice of the compiler's.

### 32 The column shifted in and out

MD_MAX72XX asks a callback for the column a shift brings in and tells another the one that falls
out, so that a sketch can run a band of any length, from flash, a sensor or a calculation, and
pass on what leaves the matrix. Taken out of 12.

Decided on 2026-11-06: no callbacks. `ShiftLeft(column)` and `ShiftRight(column)` take the column
that comes in and return the one that falls out. Two function pointers would have cost every
sketch 4 bytes of RAM and an indirect call per column, for one use the column as an argument does
not cover: reporting the columns the shifting text pushes out, to a second chain behind the first,
which since 14 is one longer chain. Up and down take no row, as a row of the panel is wider than a
byte.

As built, the bools of the shifts, `rotate` and `fill_with_zero`, became `MaxMatrixShiftFill`:
`kZero`, `kRotate` and `kKeep`, for all four directions. A literal such as `ShiftLeft(0x81)` was
ambiguous between a `bool` and a `byte` and did not compile; an `enum class` takes no `int`. The
two bools also had a combination without meaning, `fill_with_zero` with `rotate`, and up and down
had no keep. The text shifts by the column as well, so that a sketch that only shifts text does
not link the fills. The new example `Band` runs a heartbeat from flash.

### 33 A font checked against its length

`SetFont(uint8_t const* font)` of 15 checked only the header, as the format of MD_MAX72XX holds
no length. A header that promised more chars than the array held was taken, and `GetGlyph()`
walked on past its end, through whatever lay behind it in flash, and on ARM into undefined
behaviour. Nor could a pointer tell flash from RAM: a font without `PROGMEM` compiled, and on the
Uno was read from flash at its RAM address.

`SetFont()` now takes the array, `template<size_t Size> SetFont(uint8_t const (&font)[Size])`, as
`GetRow()` and `SetRow()` take theirs, and `CheckFont(font, size)` walks every char once: each has
to fit into the array, and the last has to end it. A font it took cannot lead `GetGlyph()` past
its end, which keeps walking without a bound, and the length is not kept, so it costs no RAM. A
font in RAM fails as well, almost always, as the bytes in flash at its address would have to add
up to its length exactly. A pointer no longer compiles; `SetFont(font, size)` takes a font from a
table of fonts. `kMaxMatrixDefaultFont` is declared with its length, 614, for the template.

Measured on the Uno: every example is the same size, as none sets a font; a sketch that sets one
takes 52 bytes more flash and no RAM.

### 34 On the template embedded-cpp-template

The checks, CI and the dev container from the copier template
[embedded-cpp-template](https://github.com/theAndreas/embedded-cpp-template), which Utl and
Wordclock share, so that a change to one reaches the others with `copier update`. What is
MaxMatrix's own stays its own: the variants, the simulator, the tests in simavr (`make avr`), the
control program, the pictures of `docu/`. The template's tools replace `tools/apt-install.sh`,
`cross-clang-tidy.py`, `east-const.py`, `doc-indent.py` and `docs.py`, and its
`docs/Documentation.md` `docu/Documentation.md`, as 30 meant it to.

In small steps, as the template checks more than MaxMatrix did: clang-format once, the warnings
of `-Wconversion`, `-Wsign-conversion` and `-Wshadow`, and of clang-tidy what the groups
MaxMatrix checked before find. The other groups wait for 35 and 36.

### 35 Version 2.0.0: a namespace, and constants for macros

The classes in a namespace `max_matrix`, without the prefix: `max_matrix::Matrix`,
`max_matrix::MatrixOf<4>`, `max_matrix::ModuleType::kFc16`, as the template's CodingStyle.md asks
of a library (F-9) and Utl does. The macros that are constants become `constexpr` in it, which
takes most of the some 670 places clang-tidy's groups find in `src/`; what a sketch sets with
`-D` stays a macro. The parts below the facade go into `max_matrix::internal`. A sketch has to
change its names: no aliases for the old ones, a clean break, version 2.0.0.

In two pull requests, released together as 2.0.0: first the namespace, the names without the
prefix and `max_matrix::internal`; then the macros that are constants as `constexpr`.

### 36 The template's checks, one group at a time

`.clang-tidy` turns off, for now, the groups of the template that MaxMatrix did not check before
34: `cppcoreguidelines-*`, `google-*`, `house-*`, `misc-*`, `modernize-*`, `portability-*`,
`readability-*` but three, and `bugprone-signed-bitwise`. After 35, each comes on in a pull
request of its own, with what it finds fixed, and leaves the list.

What each group found on 2026-11-11, after 35, with clang-tidy 23 as `make tidy` runs it: the
library and the examples in the three variants, the tests and the host code. A place reported
under two names, as `google-readability-braces-around-statements` and
`readability-braces-around-statements`, counts in both groups.

| Group | All | `src/` | `examples/` | Tests and host code |
|---|---|---|---|---|
| `portability-*` | 0 | 0 | 0 | 0 |
| `house-*` | 18 | 11 | 0 | 7 |
| `bugprone-signed-bitwise` | 94 | 30 | 1 | 63 |
| `google-*` | 133 | 30 | 5 | 98 |
| `misc-*` | 518 | 91 | 57 | 370 |
| `modernize-*` | 595 | 29 | 3 | 563 |
| `cppcoreguidelines-*` | 719 | 102 | 21 | 596 |
| `readability-*` | 776 | 148 | 50 | 578 |

They come on in this order, the smallest first: each pull request stays one a reviewer can
read, and what an early group fixes, a later one no longer finds, as the braces both `google-*`
and `readability-*` ask for. Some 510 of the places of `cppcoreguidelines-macro-usage` and
`modernize-macro-to-enum` lay in `test/mock/binary.h`, the stand-in for the core's `B00000001`
and its kin, which `tools/tidy.sh` means to leave out with the rest of the mock but reached
through the tests' header filter. CMake now includes the mock as `SYSTEM`, as the board
includes the core it stands in for: neither clang-tidy nor the compiler reports what lies in
its headers, which took the 513 places of the two checks in `binary.h`, seen from
`MaxMatrixTest.cpp`, to none, and `bitRead()` is the core's macro again. The mock's `NOLINT`s
stay, as clangd reads a header of the mock open in the editor as a file of its own, and
`bitClear()` stays a function, as `-Wconversion` reports a macro where it is expanded, which
`SYSTEM` does not cover. The model of the MAX7219 is no stand-in but our own code, so it
lies in `test/model/` instead, included as every header of the tests and checked as they are;
its source, which `tools/tidy.sh` left out with the mock until then, had 19 places to fix.

### 37 Files named as their class

35 put the classes into `max_matrix` without the prefix, and left the files as they were:
`internal::Panel` lay in `Matrix/MaxMatrixPanel.h`, which F-3 of CodingStyle.md does not allow.
The files in the folders of `src/` now take the name of their class, `Matrix/Panel.h`,
`Font/Font.h` and so on. Those at the root keep the prefix: `MaxMatrix.h`, as a sketch includes
it by the library's name, and the others because Arduino puts `src/` on the include path of
every sketch and library built with MaxMatrix, where an `Error.h` or `Config.h` would meet
another library's; a file in a folder is included with the folder's name. A sketch notices
nothing, as it includes `MaxMatrix.h` only.

## Decisions

Why it is planned this way rather than the obvious alternative.

- **Browser, not wxWidgets, for the simulator (6).** See the item.
- **wxWidgets, not Qt, Tauri or Electron, for the control program (26).** Native controls and
  a small program, on Windows a single `.exe`, as its licence allows static linking; and the
  toolkit Wordclock already uses. What Qt has and wxWidgets lacks is a serial port class and a
  WebSocket; a serial class of its own and a TCP port in the simulator make up for both.
  Tauri would bring Rust into the project and needs a serial plugin on Linux anyway, where its
  webview has no Web Serial; Electron is 100 MB for a few lines over a port. The browser
  restriction of 6 was about the simulator: no GUI packages in the container and no check in
  CI. Here the window stays thin and everything behind it is tested.
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
- **A flat enum, not an error domain (27).** Measured on 2026-11-01 with sketches on the Uno,
  two domains each: a flat enum takes 486 bytes of flash and 9 of RAM. An error of a domain
  pointer and a `uint8_t` takes 684 and 69 with a virtual function for the text, 628 and 9 with
  the domain as a table in `PROGMEM`, and 580 and 9 with that table's texts switched off. The
  error itself is 3 bytes and returns in registers. The vtable costs RAM because the ATmega328P
  reads flash only with `LPM`, so avr-gcc keeps all of `.rodata`, vtables included, in RAM
  (`avr5.x`); a table read with `pgm_read_ptr()` avoids that. Either way the domain keeps its
  texts linked, as every error takes its address, so only a switch takes them out. And the
  domain pointer is then a flash address that must not be dereferenced, which the tests on the
  host cannot see; the public header would have to declare the domain type only. What a domain
  buys, an open set of errors, has no user: every error comes from the library itself. A
  static `kind()` per error type, as `embedded-hal` has it, pays off only with error sources
  the library does not know, such as a transfer that can fail.
