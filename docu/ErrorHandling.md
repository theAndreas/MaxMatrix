# Error handling: what was weighed for OIL item 27

`OIL.md` item 27 plans errors that say what went wrong, and its entry under "Decisions" gives the reason
in one paragraph. This is the longer record: the alternatives, what each costs on the Uno, and why the
obvious ones from AUTOSAR and Rust do not carry over as they are. Measured on 2026-11-01 with
`arduino-cli`, the Arduino AVR platform 1.8.8 (avr-gcc 7.3.0, `-Os -flto`) and `arduino:avr:uno`
(ATmega328P, as on the classic Nano).

## Where it starts

- `stdReturnType` has `E_OK` (0) and `E_NOT_OK` (1); a result goes through a pointer parameter.
- As `E_OK` is 0, `if(Matrix.setDot(…))` is true *on an error*.
- Five reasons lie behind `E_NOT_OK`: a coordinate, module or intensity out of range; a char the font
  lacks; a `NULL` text; and in `MaxMatrixCommand` a value that is empty, no number or too large.
  The command answers all of them with `MAXMATRIX_COMMAND_INVALID_VALUE`.
- No example checks a return value.
- `setChar()` already clips a char that lies partly off the panel instead of failing.

## The alternatives

| Alternative | Size | Fixed RAM | Texts only when used | Errors from outside the library | For MaxMatrix |
|---|---|---|---|---|---|
| `stdReturnType` (today) | 1 B | 0 | – | no | the trap of `E_OK == 0` |
| `bool` for the setters | 1 B | 0 | – | no | simple, the Arduino idiom |
| **Flat enum of one byte**, in a wrapper without a conversion to `bool` | 1 B | 0 | yes, a free function | no | **chosen** |
| Domain with a vtable, `{const Domain*, uint8_t}` | 3 B | vtable and object per domain | no | yes | no: costs RAM |
| Domain as a table in `PROGMEM` | 3 B | 0 | only with a switch | yes | possible, with a trap |
| Domain id and code, e.g. ErrorKit below | 2 B | 0 | yes, decoded on the host | yes | ids must be handed out |
| Rust: an enum per part under one enum | 1–2 B | 0 | yes, `Display` | no | the model for the flat enum |

A getter of a single value returns value and error together (`MaxMatrixResult<T>`, 2 to 3 bytes, in
registers). `getRow()` and `getSprite()` keep their pointer, as C++ cannot return an array.

## Measured

Each sketch has two error sources, each a `noinline` function that returns its error, and a `loop()`
that feeds them from port pins and writes the result to a port, so that nothing folds away. Flash and
RAM in bytes:

| Sketch | with LTO (the default) | without LTO |
|---|---|---|
| Flat enum | 486 / 9 | 492 / 9 |
| Domain with a vtable, `getMessage()` never called | 684 / **69** | 690 / 69 |
| Domain with a vtable, `getMessage()` called once | 718 / 69 | – |
| Domain in `PROGMEM`, texts linked, never read | 628 / 9 | 634 / 9 |
| Domain in `PROGMEM`, a text read | 674 / 9 | – |
| Domain in `PROGMEM`, texts switched off | 580 / 9 | 586 / 9 |

- The 60 bytes of RAM of the vtable are 2 × 8 bytes of vtable, 2 × 2 bytes of domain object and
  40 bytes of text that no one reads (`avr-nm`, `.data`).
- The domain in `PROGMEM` costs no RAM, but its 48 bytes of text stay linked until a switch takes them
  out.
- Without LTO every sketch is 6 bytes larger, all of it from the core; LTO changes nothing between them.

## Why a vtable lives in RAM on the Uno

- The ATmega328P is a Harvard machine: flash and SRAM are separate address spaces. `LD`/`LDS` read
  the data space only; flash is read with `LPM`.
- avr-gcc takes every pointer as an address in the data space. A virtual call loads the vptr and the
  function pointer with `LD`, so the vtable has to be in RAM.
- The linker script `avr5.x` puts all of `.rodata` (vtables, string literals, `const` tables) into
  `.data`, `> data AT> text`: the image in flash, copied into RAM by `__do_copy_data` at start-up.
  It costs flash and RAM.
- `PROGMEM` only helps data that the code reads itself with `pgm_read_*`. The named address space
  `__flash`, with which the compiler emits `LPM` on its own, exists in C only.
- LTO does not help. What is referenced stays: every error takes the address of its domain, the
  domain points to its vtable, the vtable to the functions and their texts. `-fdevirtualize` speeds up
  a call and leaves the vtable; only Clang removes unused virtual functions
  (`-fvirtual-function-elimination`).
- On the Nano Every (ATmega4809, `avrxmega3`) flash is mapped into the data space, and `avrxmega3.x`
  keeps `.rodata` in flash (`__RODATA_PM_OFFSET__ = 0x8000`). There a vtable costs no RAM.

## Domains: what makes one unique

**By its address**, as `{const Domain*, uint8_t}`. Within one statically linked program two objects
never share an address, so no one has to hand out anything. The object must be defined in exactly one
`.cpp` and declared `extern` in the header: a `const` or `static const` at namespace scope in a header
has internal linkage, every translation unit gets its own copy, and the comparison depends on the file
it is made in. C++17 `inline` variables would solve that; the library is gnu++11.

An `operator==` per domain enum compares domain *and* code, so codes of different domains may overlap:

```cpp
inline bool operator==(const MaxMatrixError& Error, MaxMatrixFontErrorType Code)
{
    return Error.getDomain() == &MaxMatrixFontErrors && Error.getCode() == Code;
}
```

**By an id**, as AUTOSAR does with 64 bits and ErrorKit with 8. Needed as soon as an error leaves the
program, over `Serial` or into EEPROM, where an address means nothing. Then no one guarantees that ids
are unique; AUTOSAR fixes the ids of its own domains in the standard and reserves ranges for vendors.

**`ara::core::ErrorDomain`** has a 64-bit id, the virtual `Name()`, `Message()` and
`ThrowAsException()`, and an `ErrorCode` of about 10 bytes. Adaptive needs that at run time, as errors
cross shared libraries and `ara::com`; on the Uno it costs what the table above shows. A rough kind
across domains (`kind()` below) is not part of it; it would be `std::error_condition`.

**A domain in flash without a vtable**, a table read with `pgm_read_ptr()`, costs no RAM. The domain
pointer is then a flash address: read through directly, it returns whatever is in RAM at that address
on the AVR, and the right value on the host, where `PROGMEM` is empty, so the tests pass. The public
header would have to declare the domain type only, as an incomplete type, so that it can be compared
but not read through.

## Rust for comparison

How embedded Rust does it:

- **HAL** (`embedded-hal` 1.0): every bus trait has its own `type Error`, which maps to a shared
  `#[non_exhaustive]` `ErrorKind` through `kind()` — statically, with generics, without a vtable.
- **Driver:** an enum generic over the bus errors beside its own (`Error<SpiE, PinE>`).
- **Drawing into a buffer** (`embedded-graphics`): the error is `Infallible`; a pixel off the
  display is clipped, not an error.
- **Output** with `defmt`: only an index goes over the wire, the texts stay on the host.
- `Option<T>` for a getter, like `slice::get()`; `Result<(), E>` for a setter.

| | C++: domain pointer and code | Rust: an enum per part under one enum |
|---|---|---|
| What makes the domain | an address, checked at run time | the type or variant, checked when compiled |
| Every case handled | no, `-Wswitch` at most | `match` is exhaustive |
| Passing an error on | `if(…) return` | `?` with `From` |
| New domain without changing the library | **yes** | only through generics or `dyn Error` |
| One type for every error | **yes** | only the closed outer enum |
| Data with the error | none, `uint8_t` | anything per variant |
| "OK" as an error value | code 0 means ok | impossible |
| Ignoring the result | silent | a warning, `#[must_use]` |

A static `kind()` can be built in C++11: an overload of `getErrorKind()` per error enum, called from
templates, and `typename TransferType::ErrorType` for the associated type. What is missing against
Rust: trait bounds (concepts need C++20 and GCC 10; avr-gcc is 7.3), `?` and `#[non_exhaustive]`. It
pays off only with error sources the library does not know, such as a transfer that can fail. Rust on
AVR itself is no option for an Arduino library: the target is tier 3 and needs nightly.

## Readable errors

- **`getErrorText()`** returns a `const __FlashStringHelper*` from `PROGMEM`, which `Serial.print`
  reads from flash. A sketch that does not call it pays nothing, without any switch.
- **`MAXMATRIX_DEV_ERROR_DETECT`**, `STD_OFF` by default, as the DET of AUTOSAR Classic: where a check
  fails, the library reports the function and the error to a `weak` `maxMatrixReportError()`, so that a
  sketch which checks nothing still learns where it went wrong.
  - The default prints on `Serial`. `SerialControl` brings its own, as its `Serial` carries the
    protocol of the control page.
  - Off, the preprocessor removes every call; on, each costs roughly 6 to 10 bytes of flash.
- **Arduino has no debug build**, and the IDE does not pass a sketch's defines to a library. The switch
  goes into `MaxMatrixConfig.h`, or `arduino-cli` gets
  `--build-property "compiler.cpp.extra_flags=-D…"`. Not `build.extra_flags`, which some boards use for
  their USB flags.

## Testing

- The return value becomes easier to check, not harder:
  `EXPECT_EQ(Matrix.setDot(100, 3, true).getError(), MAXMATRIX_E_OUT_OF_RANGE)`. Today a test stays
  green when the wrong check fires.
- A `PrintTo()` for the enum in `test/`, not in the library, makes a failure print the name.
- **Reporting:** the test build defines `maxMatrixReportError()` in the mock, which records into
  `Mock::ReportedErrors`. The printing on `Serial` is a function of its own, tested through
  `Mock::SerialOutput`, as the `weak` default cannot run in the same binary.
- **One table of every error path** as a `TEST_P`: the call, the error and the function expected. With
  the switch on it also checks the report, in a small binary of its own under a `det/` prefix.

## ErrorKit, a draft looked at

ErrorKit is a separate draft of an error library for AVR, written outside this repository. Its idea:

- an `ErrorCode` of an 8-bit domain id and an 8-bit value, 2 bytes;
- the domain found through ADL, `constexpr errorDomainId(Enum)`: no RAM, no vtable, no text on the
  target;
- `Result<T, E>`, `fail()` and `ERR_TRY` / `TRY` in place of `?`;
- a fatal handler that writes the code and the line into EEPROM and resets through the watchdog.

It is the id variant above, decoded on the host as `defmt` does. A sketch for the Uno built without a
warning under `--warnings all`, 1652 bytes of flash and 202 of RAM with `Serial`. Found:

1. **Ids are not checked for uniqueness**, although its header says so. `DomainCheck` checks only
   `constexpr` and `!= 0`; two enums with the same id compile, and their errors compare equal
   (confirmed in the sketch). Within one project an enum of ids makes them unique by construction;
   between libraries it remains a matter of agreement.
2. **An ignored `Result` is silent under gnu++11**: `[[nodiscard]]` applies from C++17 only
   (confirmed).
3. **`kHex` in `printTo()` sits in RAM**, 17 bytes in `.data`, for the reason above. Computing the
   digit (`d < 10 ? '0' + d : 'A' + d - 10`) needs no table.
4. **The macro `TRY`** is short enough to collide: every library of a sketch shares the preprocessor.
5. **The fatal handler** (read, not built) resets with a 15 ms watchdog. That needs a bootloader which
   jumps to the sketch after a watchdog reset, as Optiboot on the Uno and newer Nanos does; the old Nano
   bootloader does not, and the board keeps resetting. It also clears `MCUSR`, so the sketch cannot
   learn why it was reset; saving it into a `.noinit` variable first would keep that.
6. Value 0 is reserved by its comments, but not checked.

For MaxMatrix, ErrorKit itself would mean an outside dependency (`depends=`), a style apart from the
conventions here, and a domain id the user has to configure. A fatal handler that resets the board
does not belong in a display library.

## Decided, and open

- **Chosen:** a flat enum of one byte, `MaxMatrixErrorType`, in a wrapper with `isOk()` and no
  conversion to `bool`; `MaxMatrixResult<T>` for a getter of a single value; `getErrorText()` in
  flash; `MAXMATRIX_DEV_ERROR_DETECT` with a `weak` report function.
- **Not domains**, because no one needs the open set they buy: every error comes from the library
  itself. A domain costs RAM with a vtable, or keeps its texts linked in `PROGMEM`; either breaks "the
  parts cost no RAM" and "a sketch that does not use it pays nothing".
- **Open, and first:** whether a dot off the panel is an error at all. If `setDot()`, `setColumn()`
  and `setRow()` clip as `setChar()` and `embedded-graphics` do, out of range is left for the
  intensity and the commands, and `MaxMatrixCommand` checks the coordinates it is sent itself.
