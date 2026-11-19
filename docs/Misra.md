# MISRA C++:2023

How far the code of MaxMatrix keeps to MISRA C++:2023, rule by rule, and where it
deviates. The C++ Core Guidelines and MISRA C++ are the standards the code follows;
[CodingStyle.md](CodingStyle.md) makes them concrete.

## How to read the matrix

The matrix lists the 4 directives and 175 rules of MISRA C++:2023 with their categories. The
texts of the rules are MISRA's and licensed; the topic below is a short note of our own, not the
rule. Read the rule in the standard.

- **Covered by**: the check that enforces the rule, the compiler's warning, a test, a rule of
  CodingStyle.md, or *Review*. *In part* means the check finds only some of the cases the rule
  names, and review takes the rest; *Review* means no free tool is known to check it. Empty
  means not yet assessed; *n/a* means it cannot occur in this code, with the reason.
- **Deviation**: where the project departs from the rule, as a whole, with its reason. A
  deviation at one place is a `NOLINT` that names the check and the rule
  (`// NOLINT(cppcoreguidelines-pro-type-reinterpret-cast): MISRA 8.2.5, F() of the core`).
- **Mandatory** rules allow no deviation, **Required** ones a documented deviation,
  **Advisory** ones may be left out with a reason.

No free tool checks MISRA C++ as a whole: clang-tidy has no MISRA module, Cppcheck's free addon
knows MISRA C only. Where a project needs evidence of compliance, a commercial checker, as
PVS-Studio or Cppcheck Premium, fills the rows marked *Review* (ROADMAP I-4).

## The matrix

### Directives

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Dir 0.3.1 | Advisory | Floating-point arithmetic used appropriately | CodingStyle H-7 | |
| Dir 0.3.2 | Required | Calls meet the preconditions of the callee | CodingStyle E-2, Review | |
| Dir 5.7.2 | Advisory | No commented-out code | AGENTS.md, Review | |
| Dir 15.8.1 | Required | Assignment handles self-assignment | `bugprone-unhandled-self-assignment`, in part | |

### Rules of chapter 0

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 0.0.1 | Required | Unreachable statements | `-Wunreachable-code-aggressive` |  |
| Rule 0.0.2 | Advisory | Invariant controlling expressions | `-Wtautological-compare`, `clang-analyzer-*`, in part |  |
| Rule 0.1.1 | Advisory | Redundant writes to local objects | `clang-analyzer-deadcode.DeadStores` |  |
| Rule 0.1.2 | Required | Return values used | `-Wunused-result`, in part: a result marked as CodingStyle FN-10 asks | A result whose drop is no mistake, as the pointer `memcpy()` returns or the count of `printf()`, is dropped as it is: FN-10 marks a result only where dropping it is a mistake, so that a mark means something, and a drop of a marked one says why |
| Rule 0.2.1 | Advisory | Variables of limited visibility are used | `-Wunused-variable` |  |
| Rule 0.2.2 | Required | Named parameters are used | `-Wunused-parameter` |  |
| Rule 0.2.3 | Advisory | Types of limited visibility are used | `-Wunused-local-typedef`, in part |  |
| Rule 0.2.4 | Advisory | Functions of limited visibility are used | `-Wunused-function`, `-Wunused-member-function`, `-Wunused-template` |  |

### Rules of chapter 4

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 4.1.1 | Required | Conforms to C++17 | Review | C++11 as the least standard, and every standard to C++20; `gnu++11` on the AVR (targets/avr.md); the tests C++17. `-fno-exceptions -fno-rtti` on the AVR (CodingStyle M-2) |
| Rule 4.1.2 | Advisory | No deprecated features | `-Wdeprecated` |  |
| Rule 4.1.3 | Required | No undefined or critical unspecified behaviour | `make sanitize`, `clang-analyzer-*` |  |
| Rule 4.6.1 | Required | Memory operations sequenced | `-Wunsequenced`, `-Wsequence-point` |  |

### Rules of chapter 5

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 5.0.1 | Advisory | No trigraph-like sequences | `-Wtrigraphs` |  |
| Rule 5.7.1 | Required | No `/*` inside a C comment | `-Wcomment` |  |
| Rule 5.7.3 | Required | No line splicing in a `//` comment | `-Wcomment` |  |
| Rule 5.10.1 | Required | Form of identifiers | `readability-identifier-naming`, `bugprone-reserved-identifier` |  |
| Rule 5.13.1 | Required | Backslash only in defined escapes | `-Wunknown-escape-sequence` |  |
| Rule 5.13.2 | Required | Escape sequences terminated | Review |  |
| Rule 5.13.3 | Required | No octal constants | Review |  |
| Rule 5.13.4 | Required | Unsigned literals with a suffix | Review |  |
| Rule 5.13.5 | Required | No lower-case `l` first in a suffix | `readability-uppercase-literal-suffix` |  |
| Rule 5.13.6 | Required | No single `L` in a `long long` suffix | `readability-uppercase-literal-suffix` |  |
| Rule 5.13.7 | Required | No mixed string prefixes | Review |  |

### Rules of chapter 6

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 6.0.1 | Required | Block declarations not ambiguous | `-Wvexing-parse`, in part |  |
| Rule 6.0.2 | Advisory | External arrays with a size | Review | A text declared `extern` in a header, as CodingStyle M-4 asks for one whose address is taken, leaves its size to its definition, so that the text is written once |
| Rule 6.0.3 | Advisory | Few declarations in the global namespace | CodingStyle F-9 |  |
| Rule 6.0.4 | Required | `main` only as the global function | n/a: a sketch has no `main`, the library none either |  |
| Rule 6.2.1 | Required | One definition rule | Review |  |
| Rule 6.2.2 | Required | Same types across declarations | the compiler and linker |  |
| Rule 6.2.3 | Required | An entity defined once | Review |  |
| Rule 6.2.4 | Required | No non-inline definitions in headers | `misc-definitions-in-headers` |  |
| Rule 6.4.1 | Required | No variable hides another | `-Wshadow-all` |  |
| Rule 6.4.2 | Required | No derived function hides a base one | `-Woverloaded-virtual`, in part |  |
| Rule 6.4.3 | Required | No unqualified lookup into a dependent base | Review |  |
| Rule 6.5.1 | Advisory | External entities declared in a header | `misc-use-internal-linkage` |  |
| Rule 6.5.2 | Advisory | Internal linkage by an unnamed namespace, not `static` | `readability-static-definition-in-anonymous-namespace`, in part |  |
| Rule 6.7.1 | Required | No static local variables | Review |  |
| Rule 6.7.2 | Required | No global variables | `cppcoreguidelines-avoid-non-const-global-variables` |  |
| Rule 6.8.1 | Required | No access outside an object's lifetime | `make sanitize`, `clang-analyzer-*` |  |
| Rule 6.8.2 | Mandatory | No reference or pointer to a local returned | `-Wreturn-stack-address` |  |
| Rule 6.8.3 | Required | No local address kept longer | `clang-analyzer-core.StackAddressEscape`, in part |  |
| Rule 6.8.4 | Advisory | Members returning a reference into their object ref-qualified | CodingStyle C-16, Review |  |
| Rule 6.9.1 | Required | Same type aliases in all declarations | Review |  |
| Rule 6.9.2 | Advisory | No standard integer type names | CodingStyle T-1, Review |  |

### Rules of chapter 7

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 7.0.1 | Required | No conversion from `bool` | `readability-implicit-bool-conversion`, in part |  |
| Rule 7.0.2 | Required | No conversion to `bool` | `readability-implicit-bool-conversion` |  |
| Rule 7.0.3 | Required | No numeric value of a character | Review | Where a char's numeric value is what the code is for: a digit parsed or printed, `c - '0'` and `'0' + value`, a char's code as an index, as of its glyph in a font, and the bytes of UTF-8 decoded. T-21 widens a char through `unsigned char` |
| Rule 7.0.4 | Required | Operands of bitwise and shift operators | CodingStyle H-8 and H-9: only in the bit class, `house-bitwise-in-bit-class`; there by review |  |
| Rule 7.0.5 | Required | Promotions keep signedness | `-Wsign-conversion`, in part: not the promotion of a type narrower than `int`; CodingStyle T-19 | The promotion of an unsigned type narrower than `int`, as `uint8_t`, to `int` in arithmetic: it keeps every value, and a cast at each operand would hide the computation it is part of. T-19 widens an operand where the result could overflow |
| Rule 7.0.6 | Required | Numeric assignments appropriate | `-Wconversion`, `bugprone-narrowing-conversions` |  |
| Rule 7.11.1 | Required | `nullptr` the only null pointer | `modernize-use-nullptr`, `-Wzero-as-null-pointer-constant` |  |
| Rule 7.11.2 | Required | No array decaying as an argument | Review | An array passed as a pointer and a size, as C APIs of the platforms take it; `cppcoreguidelines-pro-bounds-array-to-pointer-decay` is off, see `.clang-tidy` |
| Rule 7.11.3 | Required | Function-to-pointer conversion limited | Review |  |

### Rules of chapter 8

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 8.0.1 | Advisory | Parentheses make the meaning clear | `house-operator-precedence`, `-Wparentheses`, CodingStyle T-23; the expression of an `#if` by review |  |
| Rule 8.1.1 | Required | No implicit capture of `this` | `cppcoreguidelines-misleading-capture-default-by-value`, in part |  |
| Rule 8.1.2 | Advisory | Lambda captures explicit | Review |  |
| Rule 8.2.1 | Required | Virtual bases cast by `dynamic_cast` only | n/a: no virtual base (13.1.1), no RTTI |  |
| Rule 8.2.2 | Required | No C-style or functional cast | `google-readability-casting`, `cppcoreguidelines-pro-type-cstyle-cast`, `-Wold-style-cast` |  |
| Rule 8.2.3 | Required | No cast removing `const` or `volatile` | `cppcoreguidelines-pro-type-const-cast`, `-Wcast-qual` |  |
| Rule 8.2.4 | Required | No cast of function pointers | Review |  |
| Rule 8.2.5 | Required | No `reinterpret_cast` | `cppcoreguidelines-pro-type-reinterpret-cast` |  |
| Rule 8.2.6 | Required | No integer or `void*` cast to a pointer | `cppcoreguidelines-pro-type-reinterpret-cast`, in part |  |
| Rule 8.2.7 | Advisory | No pointer cast to an integer | `cppcoreguidelines-pro-type-reinterpret-cast`, in part |  |
| Rule 8.2.8 | Required | Pointer to integer via `intptr_t` only | CodingStyle T-9, Review |  |
| Rule 8.2.9 | Required | No polymorphic operand of `typeid` | n/a: no RTTI (CodingStyle M-2) |  |
| Rule 8.2.10 | Required | No recursion | `misc-no-recursion` |  |
| Rule 8.2.11 | Required | Arguments to an ellipsis of fitting types | `cppcoreguidelines-pro-type-vararg` |  |
| Rule 8.3.1 | Advisory | No unary minus on unsigned | `house-unary-minus-unsigned`, CodingStyle T-25 |  |
| Rule 8.3.2 | Advisory | No unary plus | Review |  |
| Rule 8.7.1 | Required | Pointer arithmetic stays valid | `cppcoreguidelines-pro-bounds-pointer-arithmetic` |  |
| Rule 8.7.2 | Required | Pointer subtraction within one array | `cppcoreguidelines-pro-bounds-pointer-arithmetic` |  |
| Rule 8.9.1 | Required | Pointer comparison within one array | Review |  |
| Rule 8.14.1 | Advisory | No side effect in the right operand of `&&`, `\|\|` | Review |  |
| Rule 8.18.1 | Mandatory | No copy into an overlapping object | Review |  |
| Rule 8.18.2 | Advisory | Result of an assignment not used | `bugprone-assignment-in-if-condition`, CodingStyle T-14 |  |
| Rule 8.19.1 | Advisory | No comma operator | `-Wcomma`, in part: not in the head of a `for` loop, which review takes |  |
| Rule 8.20.1 | Advisory | Constant unsigned arithmetic does not wrap | Review |  |

### Rules of chapter 9

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 9.2.1 | Required | No explicit conversion as a statement | `bugprone-unused-raii`, in part |  |
| Rule 9.3.1 | Required | Bodies are compound statements | `readability-braces-around-statements` |  |
| Rule 9.4.1 | Required | `if … else if` ends with `else` | `house-else-if-without-else`, CodingStyle T-24 |  |
| Rule 9.4.2 | Required | Structure of a `switch` | `bugprone-switch-missing-default-case`, `-Wswitch`, `-Wimplicit-fallthrough`, CodingStyle T-15, in part: the rest of the structure, as where the labels stand and how many branches there are, by review | A `switch` over an `enum class` names every value and has no `default` (T-15): `-Wswitch` then reports a value added to the `enum` later, which a `default` would take in unnoticed. A value the `enum` does not name is handled after the `switch`, so that a way out stays |
| Rule 9.5.1 | Advisory | Simple counting `for` loops | CodingStyle T-26, Review |  |
| Rule 9.5.2 | Required | Range-for initializer with one call at most | Review |  |
| Rule 9.6.1 | Advisory | No `goto` | `cppcoreguidelines-avoid-goto` |  |
| Rule 9.6.2 | Required | `goto` to a label in an enclosing block | n/a: no `goto` (9.6.1) |  |
| Rule 9.6.3 | Required | `goto` jumps forward | n/a: no `goto` (9.6.1) |  |
| Rule 9.6.4 | Required | `noreturn` functions do not return | `-Winvalid-noreturn` |  |
| Rule 9.6.5 | Required | Every path of a non-void function returns | `-Wreturn-type` |  |

### Rules of chapter 10

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 10.0.1 | Advisory | One variable per declaration | `readability-isolate-declaration`, in part: not in the head of a `for` loop, which review takes |  |
| Rule 10.1.1 | Advisory | `const` on pointer and reference parameters | `misc-const-correctness`, `readability-non-const-parameter` |  |
| Rule 10.1.2 | Required | `volatile` where it fits | CodingStyle H-6, Review |  |
| Rule 10.2.1 | Required | Enumerations with an underlying type | `performance-enum-size`, CodingStyle T-7 |  |
| Rule 10.2.2 | Advisory | No unscoped enumerations | `cppcoreguidelines-use-enum-class` |  |
| Rule 10.2.3 | Required | No numeric use of an unscoped enumeration | `cppcoreguidelines-use-enum-class` |  |
| Rule 10.3.1 | Advisory | No unnamed namespace in a header | `google-build-namespaces` |  |
| Rule 10.4.1 | Required | No `asm` | Review | Where a target needs it, with its reason (targets/avr.md AVR-7) |

### Rules of chapter 11

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 11.3.1 | Advisory | No variables of array type | `modernize-avoid-c-arrays` | A text in flash is a `char` array, as a target may need one (targets/avr.md AVR-1) (`modernize-avoid-c-arrays.AllowStringArrays`) |
| Rule 11.3.2 | Advisory | At most two levels of pointer indirection | Review |  |
| Rule 11.6.1 | Advisory | Every variable initialized | `cppcoreguidelines-init-variables` |  |
| Rule 11.6.2 | Mandatory | No read before an assignment | `-Wuninitialized`, `-Wconditional-uninitialized`, `clang-analyzer-core.uninitialized.*` |  |
| Rule 11.6.3 | Required | Implicit enumerator values unique | Review |  |

### Rules of chapter 12

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 12.2.1 | Advisory | No bit-fields | Review |  |
| Rule 12.2.2 | Required | Bit-fields of fitting types | Review |  |
| Rule 12.2.3 | Required | No named one-bit signed bit-field | Review |  |
| Rule 12.3.1 | Required | No `union` | `cppcoreguidelines-pro-type-union-access`, in part |  |

### Rules of chapter 13

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 13.1.1 | Advisory | No virtual inheritance | Review |  |
| Rule 13.1.2 | Required | No base both virtual and non-virtual | Review |  |
| Rule 13.3.1 | Required | `virtual`, `override`, `final` as fits | `modernize-use-override`, CodingStyle C-10 |  |
| Rule 13.3.2 | Required | Overrides keep the default arguments | `google-default-arguments`, in part |  |
| Rule 13.3.3 | Required | Same parameter names in all declarations | `readability-inconsistent-declaration-parameter-name` |  |
| Rule 13.3.4 | Required | Virtual member pointers compared with `nullptr` only | Review |  |

### Rules of chapter 14

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 14.1.1 | Advisory | Data members all private or all public | `misc-non-private-member-variables-in-classes`, CodingStyle C-1 |  |

### Rules of chapter 15

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 15.0.1 | Required | Special member functions as needed | `cppcoreguidelines-special-member-functions`, CodingStyle C-3 |  |
| Rule 15.0.2 | Advisory | Signatures of copy and move functions | `misc-unconventional-assign-operator`, `performance-noexcept-move-constructor`, in part |  |
| Rule 15.1.1 | Required | No dynamic type used in a constructor or destructor | `clang-analyzer-optin.cplusplus.VirtualCall` |  |
| Rule 15.1.2 | Advisory | Constructors initialize every base | Review |  |
| Rule 15.1.3 | Required | Conversions by one argument explicit | cpplint `runtime/explicit` for a constructor, a conversion operator by review; CodingStyle C-4 | A conversion that is meant, as of a span from an array, is implicit, with a `NOLINT(runtime/explicit)` that says why (C-4) |
| Rule 15.1.4 | Advisory | Members initialized before use | `cppcoreguidelines-pro-type-member-init` |  |
| Rule 15.1.5 | Required | Initializer-list constructor only as the only one | Review |  |

### Rules of chapter 16

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 16.5.1 | Required | No overloaded `&&` or `\|\|` | Review |  |
| Rule 16.5.2 | Required | No overloaded address-of | `google-runtime-operator` |  |
| Rule 16.6.1 | Advisory | Symmetric operators as non-members | CodingStyle C-15, as hidden friends: `house-operator-form` |  |

### Rules of chapter 17

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 17.8.1 | Required | No explicit specialization of a function template | Review |  |

### Rules of chapter 18

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 18.1.1 | Required | No pointer as an exception | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.1.2 | Required | Empty `throw` only in a handler | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.3.1 | Advisory | A catch-all handler | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.3.2 | Required | Class exceptions caught by reference | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.3.3 | Required | Handlers of constructors without non-static members | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.4.1 | Required | Functions that must not throw are `noexcept` | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.5.1 | Advisory | A `noexcept` function does not throw | n/a: no exceptions (CodingStyle M-2) |  |
| Rule 18.5.2 | Advisory | No functions that end the program | `bugprone-unsafe-functions`, by the list of `.clang-tidy` | A fuzzer reports what does not hold by `std::abort()` (Testing.md TS-29), with a `NOLINT` |

### Rules of chapter 19

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 19.0.1 | Required | Lines with `#` are directives | Review |  |
| Rule 19.0.2 | Required | No function-like macros | `cppcoreguidelines-macro-usage` |  |
| Rule 19.0.3 | Advisory | `#include` only after directives or comments | Review |  |
| Rule 19.0.4 | Advisory | `#undef` only of the file's own macros | Review |  |
| Rule 19.1.1 | Required | `defined` used properly | Review |  |
| Rule 19.1.2 | Required | Conditional directives in one file | Review |  |
| Rule 19.1.3 | Required | Identifiers in `#if` defined | `-Wundef` |  |
| Rule 19.2.1 | Required | Header guarded against a second inclusion | cpplint `build/header_guard` |  |
| Rule 19.2.2 | Required | `#include` of a proper file name | Review |  |
| Rule 19.2.3 | Required | No special characters in header names | Review |  |
| Rule 19.3.1 | Advisory | No `#` and `##` operators | Review |  |
| Rule 19.3.2 | Required | No `##` after a `#` parameter | Review |  |
| Rule 19.3.3 | Required | Mixed-use macro argument not expanded | Review |  |
| Rule 19.3.4 | Required | Macro arguments in parentheses | `bugprone-macro-parentheses` |  |
| Rule 19.3.5 | Required | No directive-like tokens in macro arguments | Review |  |
| Rule 19.6.1 | Advisory | No `#pragma` | Review |  |

### Rules of chapter 21

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 21.2.1 | Required | No `atof`, `atoi`, `atol`, `atoll` | `cert-err34-c` |  |
| Rule 21.2.2 | Required | No C string functions | `bugprone-unsafe-functions`, by the list of `.clang-tidy` |  |
| Rule 21.2.3 | Required | No `system()` | `cert-env33-c` |  |
| Rule 21.2.4 | Required | No `offsetof` | Review |  |
| Rule 21.6.1 | Advisory | No dynamic memory | `cppcoreguidelines-no-malloc`, `cppcoreguidelines-owning-memory`, CodingStyle M-1 |  |
| Rule 21.6.2 | Required | Dynamic memory managed automatically | n/a: no heap (21.6.1) |  |
| Rule 21.6.3 | Required | No advanced memory management | n/a: no heap (21.6.1) |  |
| Rule 21.6.4 | Required | Sized and unsized `operator delete` | n/a: no heap (21.6.1) |  |
| Rule 21.6.5 | Required | No `delete` of an incomplete type | n/a: no heap (21.6.1) |  |
| Rule 21.10.1 | Required | No `<cstdarg>` | `cppcoreguidelines-pro-type-vararg` |  |
| Rule 21.10.2 | Required | No `<csetjmp>` | `cert-err52-cpp` |  |
| Rule 21.10.3 | Required | No `<csignal>` | `bugprone-unsafe-functions`, by the list of `.clang-tidy`, in part: its functions, not its macros and types |  |

### Rules of chapter 22

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 22.3.1 | Required | `assert` not of a constant | `misc-static-assert`, in part |  |
| Rule 22.4.1 | Required | Only zero assigned to `errno` | Review |  |

### Rules of chapter 23

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 23.11.1 | Advisory | No smart pointer made from a raw pointer | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |

### Rules of chapter 24

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 24.5.1 | Required | No `<cctype>` functions | `bugprone-unsafe-functions`, by the list of `.clang-tidy` |  |
| Rule 24.5.2 | Required | No `memcpy`, `memmove`, `memcmp` | `bugprone-unsafe-functions`, by the list of `.clang-tidy` | A fuzzer takes a value out of libFuzzer's bytes by `std::memcpy()`, the one way without undefined behaviour before C++20's `std::bit_cast`, with a `NOLINT` |

### Rules of chapter 25

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 25.5.1 | Required | No `setlocale` | `bugprone-unsafe-functions`, by the list of `.clang-tidy` |  |
| Rule 25.5.2 | Mandatory | Pointers from the locale as `const` | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |
| Rule 25.5.3 | Mandatory | No pointer used after the next call | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |

### Rules of chapter 26

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 26.3.1 | Advisory | No `std::vector<bool>` | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |

### Rules of chapter 28

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 28.3.1 | Required | Predicates without side effects | Review |  |
| Rule 28.6.1 | Required | `std::move` of a non-const lvalue | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |
| Rule 28.6.2 | Required | Forwarding references with `std::forward` | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |
| Rule 28.6.3 | Required | No moved-from object used | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |
| Rule 28.6.4 | Required | Results of `remove`, `unique`, `empty` used | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |

### Rules of chapter 30

| Id | Category | Topic | Covered by | Deviation |
|---|---|---|---|---|
| Rule 30.0.1 | Required | No C library I/O | `bugprone-unsafe-functions`, by the list of `.clang-tidy` |  |
| Rule 30.0.2 | Required | Reads and writes on a stream separated | Review where `src/` uses the standard library, which it may where every target has it (CodingStyle M-3); n/a otherwise |  |
