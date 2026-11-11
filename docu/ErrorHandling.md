# Error handling: what was weighed for OIL item 27

`OIL.md` item 27 plans errors that say what went wrong, and its entry under "Decisions" gives the reason
in one paragraph. This is the longer record: the alternatives, what each costs on the Uno, and why the
obvious ones from AUTOSAR and Rust do not carry over as they are. Measured on 2026-11-01 with
`arduino-cli`, the Arduino AVR platform 1.8.8 (avr-gcc 7.3.0, `-Os -flto`) and `arduino:avr:uno`
(ATmega328P, as on the classic Nano).

The names are those before version 2.0.0, which put the library into the namespace `max_matrix`
(OIL 35): `MaxMatrixError`, `MaxMatrixStatus` and `MaxMatrixResult<T>` are now `max_matrix::Error`,
`max_matrix::Status` and `max_matrix::Result<T>`, `MaxMatrixReportError()` is
`max_matrix::ReportError()`, and the parts below the facade, as `MaxMatrixText`, lie in
`max_matrix::internal`.

## Where it starts

- `stdReturnType` has `E_OK` (0) and `E_NOT_OK` (1); a result goes through a pointer parameter.
- As `E_OK` is 0, `if(matrix.SetDot(…))` is true *on an error*.
- Five reasons lie behind `E_NOT_OK`: a coordinate, module or intensity out of range; a char the font
  lacks; a `NULL` text; and in `MaxMatrixCommand` a value that is empty, no number or too large.
  The command answers all of them with `MAXMATRIX_COMMAND_INVALID_VALUE`.
- No example checks a return value.
- `SetChar()` already clips a char that lies partly off the panel instead of failing.

## The alternatives

| Alternative | Size | Fixed RAM | Texts only when used | Errors from outside the library | For MaxMatrix |
|---|---|---|---|---|---|
| `stdReturnType` (until #30) | 1 B | 0 | – | no | the trap of `E_OK == 0` |
| `bool` for the setters | 1 B | 0 | – | no | simple, the Arduino idiom |
| **Flat enum of one byte**, in a wrapper without a conversion to `bool` | 1 B | 0 | yes, a free function | no | **chosen** |
| Domain with a vtable, `{Domain const*, uint8_t}` | 3 B | vtable and object per domain | no | yes | no: costs RAM |
| Domain as a table in `PROGMEM` | 3 B | 0 | only with a switch | yes | possible, with a trap |
| Domain id and code, e.g. ErrorKit below | 2 B | 0 | yes, decoded on the host | yes | ids must be handed out |
| Rust: an enum per part under one enum | 1–2 B | 0 | yes, `Display` | no | the model for the flat enum |

A getter of a single value returns value and error together (`MaxMatrixResult<T>`, 2 to 3 bytes, in
registers). `GetRow()` and `GetSprite()` keep their pointer, as C++ cannot return an array.

## Measured

Each sketch has two error sources, each a `noinline` function that returns its error, and a `loop()`
that feeds them from port pins and writes the result to a port, so that nothing folds away. Flash and
RAM in bytes:

| Sketch | with LTO (the default) | without LTO |
|---|---|---|
| Flat enum | 486 / 9 | 492 / 9 |
| Domain with a vtable, `GetMessage()` never called | 684 / **69** | 690 / 69 |
| Domain with a vtable, `GetMessage()` called once | 718 / 69 | – |
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

**By its address**, as `{Domain const*, uint8_t}`. Within one statically linked program two objects
never share an address, so no one has to hand out anything. The object must be defined in exactly one
`.cpp` and declared `extern` in the header: a `const` or `static const` at namespace scope in a header
has internal linkage, every translation unit gets its own copy, and the comparison depends on the file
it is made in. C++17 `inline` variables would solve that; the library is gnu++11.

An `operator==` per domain enum compares domain *and* code, so codes of different domains may overlap:

```cpp
inline bool operator==(MaxMatrixError const& error, MaxMatrixFontErrorType code)
{
    return error.getDomain() == &MaxMatrixFontErrors && error.getCode() == code;
}
```

The address of a static object as an identity is a known pattern well beyond AUTOSAR:

- **`std::error_category`** (C++11, from Boost.System): two categories are equal when they are the same
  object, `this == &rhs`, and `std::error_code` is `{int, error_category const*}`. `ara::core` took it
  from there.
- **LLVM's `static char ID`**: the passes of the legacy pass manager and `llvm::Error` with
  `ErrorInfo<T>` know their type by the address of a `static char ID;`, a hand-made RTTI.
  `isA<MyError>()` compares addresses.
- **Go's sentinel errors**: `var ErrNotFound = errors.New("not found")`, tested with
  `err == ErrNotFound`, a pointer comparison with the text attached. The closest to a domain with its
  texts.
- **`static char key;` in C APIs** such as Lua's registry: an object that exists only to have an
  address.

It has two known traps:

1. **Shared libraries.** A singleton exists once per DLL or `.so`, with an address of its own in each,
   and the comparison fails across them; it did so with `std::system_category()` across DLLs. Boost.System
   then gave its categories a 64-bit id, and `status_code` (P1028) uses ids from the start; it is the
   reason for AUTOSAR's 64 bits as well. An Arduino sketch has no shared libraries.
2. **Identical constants may be merged.** With `-fmerge-all-constants`, or a linker's identical code
   folding (lld and gold `--icf=all`, MSVC `/OPT:ICF`), two `const` objects with the same contents may
   end up at one address; that is why LLVM's `ID` is not `const`. Here it would hit the domains with
   their texts switched off, which are both `{nullptr, nullptr}`. The Arduino toolchain links with
   GNU ld without folding and does not set `-fmerge-all-constants`, but domains that differ in their
   contents, by an id byte for instance, do not depend on that.

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

Both domain patterns can be built in Rust without a heap; the sketches below were not compiled here.

- **An id and a code**, as ErrorKit: a trait `Domain` with `const ID: u8`, and `impl<E: Domain>
  From<E> for ErrorCode`. Rust closes ErrorKit's gaps: `?` converts through `From` without a macro,
  `#[must_use]` warns under every edition, and `err.downcast::<I2cErrc>()` returns the typed enum,
  whose `match` is exhaustive again. It does not check that ids are unique either.
- **An address**, as `ErrorCode { domain: &'static DomainInfo, value: u8 }`, compared with
  `core::ptr::eq()`. The domain has to be a `static`: a `const` is copied into every place it is
  used and has no address of its own, as a `const` in a C++ header has one per translation unit. A
  domain with contents (a name, an id byte) does not rely on two empty statics being kept apart.

A trait object works differently from a C++ object with virtual functions. The object carries no
vptr; `&dyn Trait` is a fat pointer of two words, the data and the vtable. The cost moves from the
object into every stored reference:

- **For it:** objects stay as small as their data, any type can get a trait afterwards (a foreign
  one, a `u8`), several traits need no adjusted pointers or thunks, and there is no vptr that changes
  while a constructor runs.
- **Against it:** every pointer is twice as large. On the AVR a domain as `&'static dyn ErrorDomain`
  makes the error 5 bytes, where C++ has 3; a `&'static DomainInfo` without `dyn` stays a thin
  pointer and keeps it at 3. An object does not know its type from its address alone, so a fat
  pointer does not fit a C callback's `void* context`, an `AtomicPtr` shared with an interrupt, or an
  intrusive list of different types. Converting between trait objects is clumsier than a cast to a
  base class, and a vtable may exist once per codegen unit, so two `&dyn` to the same object can
  differ: compare them with `core::ptr::addr_eq()`.

Many objects with few stored references favour Rust's way; few objects with many long-lived
references, or pointers that have to pass through C, atomics or intrusive structures, favour C++'s.

## Readable errors

- **`GetErrorText()`** returns a `__FlashStringHelper const*` from `PROGMEM`, which `Serial.print`
  reads from flash. A sketch that does not call it pays nothing, without any switch.
- **`MAXMATRIX_DEV_ERROR_DETECT`**, `STD_OFF` by default, as the DET of AUTOSAR Classic: where a check
  fails, the library reports the function and the error to a `weak` `MaxMatrixReportError()`, so that a
  sketch which checks nothing still learns where it went wrong.
  - The default prints on `Serial`; a sketch may define its own. `SerialControl` keeps the default:
    the control page shows any line and reads only `ready`, and a sketch that switches the reports on
    wants to see them.
  - Off, the preprocessor removes every call. What it costs on is under "Decided, and open".
- **Arduino has no debug build**, and the IDE does not pass a sketch's defines to a library. The switch
  goes into `MaxMatrixConfig.h`, or `arduino-cli` gets
  `--build-property "compiler.cpp.extra_flags=-D…"`. Not `build.extra_flags`, which some boards use for
  their USB flags.

## Testing

- The return value becomes easier to check, not harder:
  `EXPECT_EQ(matrix.SetDot(100, 3, true).GetError(), MaxMatrixError::kOutOfRange)`. Today a test stays
  green when the wrong check fires.
- A `PrintTo()` for the enum in `test/`, not in the library, makes a failure print the name.
- **Reporting:** the test file defines `MaxMatrixReportError()` itself, which records into a list of
  its own; the mock stays free of the library's types. The printing on `Serial` is a function of its own, tested through
  `mock::serial_output`, as the `weak` default cannot run in the same binary.
- **One table of every error path**: the call, the error and the function expected. With the switch on
  it also checks the report: the whole suite runs a third time under a `det/` prefix, which also shows
  that a report changes nothing else.

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
2. **An ignored `Result` is silent under gnu++11** (confirmed). Not for want of the compiler:
   avr-gcc 7.3 takes `[[nodiscard]]` on a type under gnu++11 as well, see Utl below.
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

## Tried out: the domain by its address, in Utl

The variant "by its address" above was built out on its own in November 2026, as the library
[Utl](https://github.com/theAndreas/Utl), to see how it reads and what it costs beyond a table.
Measured with `arduino-cli` and Arduino AVR Boards 1.8.8 on `arduino:avr:uno`; it has run in simavr,
not on a board. What follows is its first version, with virtual groups; Utl has since dropped
`virtual`, see "Tried out: MaxMatrix on Utl" below.

- **What it is.** `utl::Error` is a pointer to its group and a code, 3 bytes. A group is a class
  derived from `ErrorGroup` with a virtual `Name()` and `Message()`, one object per program, defined
  in one `.cpp` and declared `extern`. A `GetErrorGroup()` beside a group's enum, found by ADL, lets
  a code convert to an `Error`: `return FontError::kUnknownChar;`. `Status` is ok or an `Error`,
  `Result<T>` a value or one, side by side, with a fluent API after Rust (`AndThen()`, `Map()`,
  `OrElse()`, `ValueOr()`); a chain of three steps costs 14 bytes of flash more than the same `if`s
  written out, and no RAM.
- **"Ok" is not an error.** A first version had `Error()` mean "no error", as `std::error_code`
  does with 0, and it read as a contradiction. Splitting `Status`, ok or an error, from `Error`,
  always one, cleared it at no cost: a `Status` keeps an `Error` with a null group for "ok", which
  only `Status` can make, and stays 3 bytes. `MaxMatrixStatus` makes the same split already.
- **The vtables can move to flash.** The GCC plugin
  [avr-flash-vtbl](https://github.com/jcmvbkbc/avr-flash-vtbl), two lines that give the vtable types
  the address space `__flash`, loads into the `cc1plus` of the Arduino toolchain, and a virtual call
  then reads its function pointer with `LPM`. In Utl's `Basics`, with two groups, RAM falls from 208
  to 174 bytes for 2 bytes of flash: the 8 bytes of each group's vtable and the 18 of
  `HardwareSerial`. A group object keeps its 2 bytes of vptr in RAM, as its address is the group.
  The cost moves into the build: every C++ file has to be compiled with the plugin, the core
  included, or a virtual call reads a vtable in flash from RAM and jumps anywhere, which the linker
  does not notice; the IDE passes no flags per sketch, so the plugin goes into `platform.local.txt`;
  and it loads only into the `cc1plus` it was built for.
- **The texts stay linked, unless switched off.** A virtual `Message()` keeps every group's texts,
  whether anything prints them or not: GCC cannot drop an unused virtual function, only Clang can
  (`-fvirtual-function-elimination`). A switch, `UTL_ERROR_TEXTS`, leaves them out without a change
  to any group: the lookup no longer reads the table, so the table is unused and the linker drops
  it, and an error prints its code, `bus: 0`. It saves 46 bytes of flash in Utl's `Basics` and 62
  in its `SelfCheck`, once the code is printed with digits counted off: `Print::print(code)` pulled
  in the core's printing of an `unsigned long` and made `Basics` 62 bytes larger without texts than
  with them, and `code / 100` brought two division routines of libgcc, 80 bytes.
- **`[[nodiscard]]` on a type works under gnu++11.** avr-gcc 7.3 takes it and warns where a returned
  `Status` or `Result` is dropped, and nowhere else (checked with a sketch). Since
  [Utl #27](https://github.com/theAndreas/Utl/pull/27) only a `Result<T>` and an `Error` carry it,
  for the reason given below.
- **arduino-cli finds a library only by a header directly in its `src/`.** Utl's headers lie in
  `src/utl/`, so a sketch includes `<Utl.h>` first, before any header of its own that includes
  `<utl/...>`: the includes are gone through in order.

For MaxMatrix it changed the reason given below for not taking domains: with the vtables in flash a
group costs 2 bytes of RAM, and the texts can be switched off. What it did not change is that no
error comes from outside the library yet. Whether MaxMatrix takes Utl over was OIL item 28; the next
section is what a prototype of it showed.

## Tried out: MaxMatrix on Utl

A prototype on a branch that was not merged put MaxMatrix's errors onto Utl:
`MaxMatrixStatus` and `MaxMatrixResult<T>` became `utl::Status` and `utl::Result<T>`, the codes
kept their names in a group of their own, and `MaxMatrixError::kOk` went, as "ok" is no code there. It
was built for the Uno and run in simavr, where `SerialControl` answered as before; the host tests
were not moved over. Flash and RAM of the examples on the Uno, Arduino AVR Boards 1.8.8:

| Example | today | Utl | Utl, plugin | Utl, plugin, no texts | Utl without `virtual` |
|---|---|---|---|---|---|
| `Basics` | 3570 / 70 | 3714 / 70 | 3714 / 70 | 3590 / 70 | 3568 / 70 |
| `ScrollText` | 3632 / 82 | 3778 / 82 | 3778 / 82 | 3654 / 82 | 3632 / 82 |
| `Messages` | 3706 / 153 | 3872 / 163 | 3872 / 155 | 3748 / 155 | 3726 / 153 |
| `SerialControl` | 5866 / 377 | 6182 / 387 | 6184 / 361 | 6062 / 361 | 6244 / 385 |

`Animation` and `Sprites`, which draw no text, were the same in every column. The plugin alone,
without Utl, takes `SerialControl` to 5866 / 359, by moving the vtable of `HardwareSerial`; that
is no gain of Utl's.

- **With virtual groups, every sketch that draws text paid for texts it never printed**, 144 to
  166 bytes in the three examples that do not print an error, as `Message()` was virtual. Today
  they pay nothing: `GetErrorText()` is no virtual function, and the linker drops it with its
  texts where no one calls it. The switch `UTL_ERROR_TEXTS` takes the texts out, but for the whole
  build, and the Arduino IDE sets it only in `platform.local.txt`; `SerialControl` then answers
  `error: ` without a reason.
- **Without `virtual`**, the last column, with Utl's groups changed in
  [Utl #24](https://github.com/theAndreas/Utl/pull/24) and
  [#25](https://github.com/theAndreas/Utl/pull/25): a group is the address of its name in flash,
  with no object behind it, and a sketch turns a group's texts on with a call,
  `maxMatrixUseErrorTexts()` in `SerialControl`. The sketches that print no error are back where
  they are today, with no plugin and no switch, and `Messages` takes the RAM of today as well.
  Fully automatic, without the call, was tried first: a table in a section of its own that only
  the printing code names through `__start_`/`__stop_`. The linker dropped it when unused, but the
  AVR linker script does not know the section and placed it in the data space at the address of
  `.bss`, and the sections it places in flash have no `__start_` symbol and no `KEEP`.
- **A sketch that prints errors pays more than today**: `SerialControl` 378 bytes of flash and 8 of
  RAM. About 130 are the wider error, a group pointer and a code in 3 bytes where an enum takes one:
  each function that passes a `Result` on, `GetCharSprite()` and `ConvertToNumber()` among them,
  grows by 64. About 200 are the texts turned on: 70 of data, the texts, their table and the
  group's entry in the list, and 130 of code that hangs the entry in and looks a group up at run
  time. It is no matter of inlining; `noinline` made it 20 bytes larger.
- **`[[nodiscard]]` warned in sketches**: eight calls in the examples and two in the library that
  drop a status on purpose, `matrix.SetIntensity(2)` and `matrix.SetText("Hi!")` among them, each
  needed a `static_cast<void>()`. [Utl #27](https://github.com/theAndreas/Utl/pull/27) took the
  attribute off `Status` and kept it on `Result<T>`, where a dropped value is a mistake: the
  prototype then builds without a warning of its own, at the same size.
- **The names were Google's**, `IsOk()`, `error()->Message()`, beside MaxMatrix's `getState()`
  and `setText()`. Since MaxMatrix took Google's names as well (#44), they read alike;
  `Status == MaxMatrixError::kOutOfRange` still compiles.

## Why the virtual group was questioned late

The domain with a virtual `Name()` and `Message()` was taken from `std::error_category` and
`ara::core::ErrorDomain`, as the known form, not weighed against one without `virtual`. Both of
its costs come from `virtual`, the vtable in RAM and the texts that stay linked, and both were
answered with a workaround, the plugin and the switch, each measured against itself rather than
against a group without `virtual`. That GCC cannot drop an unused virtual function was written
down in Utl and above as the reason for the switch, not as a question to the design. And the
counterexample was at hand all along: `GetErrorText()` here, which is not virtual and costs nothing
where it is not called. It showed only when both were measured side by side in a real user of the
library, MaxMatrix, rather than in Utl's own examples, which have nothing to compare with.

What follows from it: where a design needs workarounds to keep its own costs down, measure the one
without the cause first, and in a library that uses it.

## Decided, and open

- **Chosen:** a flat enum of one byte, `MaxMatrixError`, in a wrapper with `IsOk()` and no
  conversion to `bool`; `MaxMatrixResult<T>` for a getter of a single value; `GetErrorText()` in
  flash; `MAXMATRIX_DEV_ERROR_DETECT` with a `weak` report function.
- **Not domains**, because no one needs the open set they buy: every error comes from the library
  itself. A domain costs RAM with a vtable, or keeps its texts linked in `PROGMEM`; either breaks "the
  parts cost no RAM" and "a sketch that does not use it pays nothing".
  Utl, without `virtual`, has since removed both costs for a sketch that prints no error, but one
  that does pays 378 bytes of flash more in `SerialControl`; see "Tried out: MaxMatrix on Utl"
  above. OIL item 28 is dropped until errors come from outside the library.
- **Drawing clips**, decided first: `SetDot()`, `SetColumn()` and `SetRow()` take `int16_t`, leave out
  what lies off the panel as `SetChar()`, `embedded-graphics` and Adafruit GFX do, and return nothing.
  Out of range is left for the getters, the intensity and the commands, and `MaxMatrixCommand` checks
  the coordinates it is sent itself. Measured on the Uno: `Basics` 28 bytes of flash more for the
  checks against negative coordinates, `ScrollText` 4 and `SerialControl` 34 less, as `SetSprite()`
  no longer checks on its own; RAM the same.
- **The enum and its wrappers**, the second step: `MaxMatrixError`, `MaxMatrixStatus` and
  `MaxMatrixResult<T>` in `src/MaxMatrixError.h`, in place of `stdReturnType` throughout the library.
  Measured on the Uno: `Basics` 18 bytes of flash less, `ScrollText` the same, `SerialControl` 80
  less, RAM the same. A getter returning its value in registers costs less than one writing it through
  a pointer, and `MaxMatrixFont::GetCharSprite()` replaced four copies of the lookup in
  `MaxMatrixText`.
- **The text of an error**, the third step: `GetErrorText()` on `MaxMatrixStatus` and
  `MaxMatrixResult<T>`. `MaxMatrixCommand::Feed()` passes the reason of a failed line on beside
  `PENDING` and `DONE`, with `MaxMatrixError::kUnknownCommand` and `MaxMatrixError::kLineTooLong` new, and
  `SerialControl` answers `error: out of range`.
  - A `switch` that returned `F("…")` for each error cost 14 bytes of RAM. avr-gcc turned it into a
    table of the seven pointers in `.rodata` (`CSWTCH.1` in the map), which the Uno copies into RAM
    like the vtables above. The texts and their table now lie in `PROGMEM`, read with `pgm_read_ptr()`.
  - Measured on the Uno: `Basics` and `ScrollText` the same, as the linker drops the texts.
    `SerialControl` needs 184 bytes of flash more: 94 for the texts, 14 for their table, the rest for
    code. The change to the command alone saves 6. RAM is the same.
- **The report**, the fourth step: `MAXMATRIX_DEV_ERROR_DETECT`. Each of the eleven functions of the
  facade that can fail wraps the call below in `MAXMATRIX_REPORT_ERROR("name", …)`, which reports to
  the weak `MaxMatrixReportError()` and is the bare call when the switch is off. The name is an `F()`
  string at the call site rather than an id as in the DET: a report of a sketch's own prints it with
  `Serial.print()` and needs no table. `F()` in inline functions and templates can end in a "section
  type conflict" with avr-gcc; 7.3 took it in a test sketch beside `F()` in ordinary functions.
  - The default lies in a file of its own, and the library is linked as an archive
    (`dot_a_linkage=true`). Linked as loose objects, a sketch that defined its own report still got
    `Serial`, 175 bytes of RAM: the reference to `Serial` in the default, left out as it was, pulled
    `HardwareSerial0.o` out of the core, and `serialEventRun()` in `main()` kept it alive.
  - Measured on the Uno: off, every example as before, to the byte. On, `Basics` 1158 bytes of flash
    and 179 of RAM more, `ScrollText` 840 and 175, both for `Serial` they did not use before;
    `SerialControl`, which has it, 126 bytes of flash and no RAM. A sketch with a report of its own
    that blinks paid 10 bytes of flash and no RAM.
- **`stdReturnType` gone**, a fifth step: the simulator and `DocuPictures` return `bool`, as each of
  their functions fails for one reason only, which its documentation names. `MaxMatrixError` has no
  word for a port that is taken, and `std::optional` would need C++17. The examples are the same to the
  byte.
- **To keep a later change cheap:** a sketch reaches the error only through the wrapper,
  `Status.IsOk()` and `Status == MaxMatrixError::kOutOfRange`, never through the raw byte. Then the
  wrapper can become `{Domain, Code}` inside without a sketch changing, should errors from outside the
  library ever pass through it: commands a sketch registers with errors of its own, or a transfer that
  can fail, such as I²C with a NACK. A domain in `PROGMEM` with an id byte in its contents would be the
  choice then.

## Beyond this library: embedded projects in general

What the comparison suggests for other embedded projects, where the answer differs from MaxMatrix's.

**First, tell errors from violations.** A violation is a programming error or a state that cannot
be: an index off its array, a broken invariant. It belongs to an assert or a fatal handler that
records it and resets in a defined way, not into an error code. An error the program can recover
from (a bus NACK, a timeout, a sensor gone) is a value in the return type. AUTOSAR keeps the two
apart (violations and `ErrorCode`), as ErrorKit does (`fatal()` and `Result`); where they are mixed,
half of the error codes are asserts in disguise.

**Within one component or library: an enum, statically.** The set of errors is closed, so the
cheapest and safest form fits: one enum per component and `Result<T, E>` with exactly that `E`. This
is Rust's model, and what MaxMatrix does.

**Across the components of an application: a domain id and a code.** Once errors pass through
layers (drivers, middleware, application) and no layer should map them, they need one type. An id
rather than an address:

- Errors in embedded systems nearly always leave the device: a fault memory in EEPROM or flash,
  logs, telemetry, diagnostics in the field. An address means nothing there; an id does.
- Two bytes, no RAM, no text on the target; the host decodes, as with `defmt`.
- The application keeps the registry, as an enum of ids rather than loose constants, so that they are
  unique by construction. A library takes its id as a configuration parameter or gets a reserved
  range. At the level of the application the set is closed again, since the application knows its
  components; Rust collapses the errors of its crates into one enum of the application the same way.

**The address as an identity fits a niche:** errors that never leave the program, texts wanted on
the target (a console on the device), and libraries developed apart that cannot agree on ids, for
which the linker hands out unique addresses for free. A domain object that carries an id byte
serves both: the address within the program, the id on the wire.

**What does not carry over from the AVR:** the RAM a vtable costs. On Cortex-M, ESP32, RISC-V and the
Nano Every, `.rodata` and with it every vtable stays in flash, and a virtual domain costs no RAM; what
remains are its texts, always linked, and the singleton trap. A design like `ara::core` is sound on a
Cortex-M with 256 KB of flash.

**Under a safety standard** (ASIL, MISRA), unions are restricted or banned, so a `Result` built on a
union, as ErrorKit's is, would need a deviation; the rule and its exceptions are for the standard in
use to say. A `struct` with the value and the error side by side costs a few bytes and complies.

In short: an enum within a component; between components an id and a code, with a registry and the
texts on the host; the address only when errors never leave the program and no one can hand out
ids.
