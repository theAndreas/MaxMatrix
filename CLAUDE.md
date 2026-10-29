# MaxMatrix

An Arduino library for chained 8x8 LED dot matrix modules driven by the MAX7219.

## Layout

- `MaxMatrix.h` / `MaxMatrix.cpp` — the library: configuration macros, the sprite table
  (font) and the `MaxMatrix` class.
- `StandardTypes.h` — `stdReturnType`, `STD_ON`/`STD_OFF` and bit macros.
- `examples/` — one sketch per folder, compiled by `arduino-cli` and checked by `arduino-lint`.
- `docu/` — how rows, columns and bits map onto a module in each orientation.
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
`docu/LED_Matrix.png`. Every write to the hardware updates the buffer as well, so the
buffer is what `get…` functions read — the MAX7219 cannot be read back.

### Two layers

- **Public functions** (`setColumn`, `setRow`, `setDot`, `shiftLeft`, …) take coordinates as
  the viewer sees them and translate them according to `Orientation`.
- **`…LL` functions** work on the buffer and the hardware as wired and know nothing about
  orientation.

A public function picks the `…LL` function per orientation; an `…LL` function never looks at
`Orientation`. A new feature that works on coordinates belongs in the public layer and has to
handle all four orientations.

### Hardware access

A transfer to the MAX7219 chain is framed by `startTransmission()` and `stopTransmission()`,
which drive chip select and, in SPI mode, begin and end the SPI transaction. Every module in
the chain gets one address/value pair per frame; a module that is not addressed gets
`MAX7219_REG_NO_OP_ADDRESS`. Bit banging and SPI are selected with `#if` on
`MAXMATRIX_USE_SPI`, and both branches have to keep working.

## Code conventions

These are observed throughout the existing code — follow them rather than reformatting to a
different taste.

### Naming

- Functions `lowerCamelCase`; `get`/`set` for accessors, `convert…`/`decode…` for
  conversions. Low level functions end in `LL`.
- Variables, parameters and members `PascalCase`.
- A setter parameter that collides with its member is prefixed `s`
  (`setModuleOrientation(MaxMatrixModuleOrientationType sOrientation)` assigning `Orientation`).
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
make -C test
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

**A bug fix comes with a test that fails without the fix.** Put the bug back and run the
tests before committing; a test that stays green then does not test the bug. The comment above
the test names the bug it guards against.

The tests reach private members through `#define private public`, because some of what they
check has no public path — `setColumnOnAllModulesLL` is only called from `clear()`, which
zeroes the buffer afterwards and hides a wrong index. The define comes after the GoogleTest and
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
