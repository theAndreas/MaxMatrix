# Documenting the code

How the code of MaxMatrix is documented: which comments are Doxygen blocks, what each block says,
and how the style is checked. The style is the one of Utl, so that every project documents alike;
the reference built from the blocks of `src/` is published on GitHub Pages. It holds for `src/`,
`simulator/` and `docu/`; the tests in `test/` keep their lighter style, a comment at the top and
one above each test.

## Two kinds of comment

- **A Doxygen block `/*! ... */`** documents an entity for its user: what it is, how to use it,
  what it costs. It stands right above the declaration, in the header, and those of `src/` end
  up in the reference.
- **A comment on the implementation** explains it to the next person who changes it: why it is
  written this way, which alternative was rejected and why. It is not in the reference.
  - One or two lines, it is a `//` comment.
  - Three lines or more, or more than one paragraph, it is a block `/* ... */`, each line
    starting with ` * `, without the `!` that would make it Doxygen's.

A comment says what the code cannot say for itself. It does not repeat the code.

### Where a comment on the implementation stands

Where the code it explains is, never between a Doxygen block and its declaration, where it would
read as a second comment of no one:

- **In the body** of the function it explains, as why `Send()` sends a column of every module in
  one frame.
- **Above the private member** whose layout it explains, as how `matrix_buffer_` lies.
- **At the definition in the `.cpp`**, when it explains that.
- **In the Doxygen block** instead, when the user should know it after all, as a `\note`.

Only a `NOLINTNEXTLINE` stands between a block and its declaration, as it has to stand right above
the line it is for.

## The block

```cpp
    /*!
     * \brief Sets the column `column` of the panel to `value`, bit 0 the top row.
     *
     * Dots that lie off the panel are left out, as a sprite that moves in does.
     *
     * \param[in] column The column, from the left, as the viewer sees it.
     * \param[in] value One bit per row.
     */
    void SetColumn(int16_t column, byte value);
```

- `/*!` on a line of its own, each further line starting with ` * `, and ` */` on a line of its
  own. A block that holds only a brief is one line: `/*! \brief The width, in columns. */`.
- An enum value, a member or a struct field is documented behind it, with `/*!< ... */`, when the
  brief is all it needs and fits on the line. If it does not, a one-line block
  `/*! \brief ... */` stands above it.
- Commands start with a backslash, `\brief`, never with `@`.
- One space between a command, its direction and its name, `\param[in] column`; nothing is aligned
  in columns.
- A line that continues a command starts where the text after the command starts:

  ```cpp
   * \return The column, one bit per row, or MaxMatrixError::kOutOfRange if `column`
   *         lies off the panel.
  ```

  `tools/doc-indent.py --fix` indents it so; without `--fix`, it fails where a line is not indented
  so, as a job of `ci.yml`. It reads `src/`, `simulator/` and `docu/`.

### The parts, in this order

| Part | Says |
|---|---|
| `\brief` | One sentence, ending in a full stop. Always written out. |
| Description | After an empty line: what the user has to know beyond the brief, in a few short paragraphs. |
| `\tparam` | Each template parameter that the user chooses. |
| `\param[in]`, `[out]`, `[in,out]` | Each parameter, with its direction. |
| `\return` | What comes back. For a `MaxMatrixStatus` or a `MaxMatrixResult`, each error with when, and what is left as it was. A one-line block whose brief already says it, "Returns the width of the panel.", leaves it out. |
| `\pre` | What the caller must ensure, such as `Init()` first. |
| `\note`, `\warning` | What is easy to miss. `\warning` only where a mistake is costly. |
| `\see` | A related entity, where the link helps. |

A function that returns nothing has no `\return`. Every parameter has its `\param`, even the only
one. A part that has nothing to say is left out. `\details` is not written: the description is
the paragraphs after the brief.

### The brief

- A function: a verb in the third person, saying what it does. "Sets the column `column` of the
  panel.", "Shifts the text one column further.", "Returns what the matrix is doing."
- A type: a noun phrase, saying what it is. "The matrix a sketch draws on.", "A sprite: its width,
  its height and up to five columns."
- A value or a member: what it holds or means. "The module at the left, as the viewer sees it."

### The description

- Plain, complete sentences. Present tense, active voice, no "we".
- What the user needs to choose and to use the entity correctly: its cost on the Uno in flash and
  RAM where it matters, its limits, what it does at the edges, whether it sends to the chain.
- An example in `\code{.cpp} ... \endcode` where a call is clearer than a sentence. Keep it short
  and make it compile.
- Names in backticks: parameters as `column`, entities as `MaxMatrixPanel`. Doxygen links an
  entity by its name; write `MaxMatrix::SetText()` to link a member.

### What is documented

- **Everything a sketch reads**, documented whole: in `src/MaxMatrix.h`, `MaxMatrixConfig.h`,
  `MaxMatrixError.h`, `MaxMatrixStandardTypes.h` and `Command/`, every class, function, member,
  type, enum value and macro, and the types the facade takes from the parts, such as
  `MaxMatrixSprite` and the orientations. Each header has a `\file` block.
- **Below the facade**, in `Max7219/`, `Matrix/`, `Font/` and `Text/`, a block is welcome but not
  required; `tools/docs.py` lets a gap there pass.
- **A function that only passes a call on** takes the block of the function it calls with
  `\copydoc`, rather than a second text that would drift from it. **Overloads** that differ
  only in their parameters share one text the same way.
- **The simulator and `docu/`** are documented as `src/` is, at their declarations, though not
  in the reference: they are read beside the library.
- **Tests and examples** keep `//` comments.

### A file

Every file of `src/`, `simulator/` and `docu/` starts with the licence, as `//` lines that tools
such as REUSE read, and then a block for the file:

```cpp
// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The font of the MaxMatrix library.
 *
 * A sprite per char, and the way from a char of a string to its sprite. Knows nothing of the
 * matrix.
 */
```

The year is the one of the file's first commit.

## Checking

- **`tools/docs.py`** builds the reference with Doxygen and fails on every warning: a block
  Doxygen cannot read, an entity a sketch reads without a block. It lets a gap below the facade
  pass.
- **clang's `-Wdocumentation`**, through clang-tidy in `static-analysis.yml` and through clangd,
  checks each block against the declaration it stands at: a `\param` whose name the function
  does not have, a `\return` on a function that returns nothing, an empty `\details`, an unknown
  command. clangd shows these in the editor as you type. A backslash meant as a char is written
  `\\`, as in `\\u00XX`, or clang takes it for a command.

- **`tools/doc-indent.py`** checks that the lines continuing a command are indented under its
  text.

CI runs `tools/docs.py` on every pull request and publishes the reference from `main`.

## Writing a block

The dev container has the extension "Doxygen Documentation Generator". Type `/*!` above a
declaration and press Enter: it writes the skeleton of this style, with a `\param` for each
parameter and a `\return` for a function that returns a value. The text is yours to write.
