// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Commands for the matrix, one per line of text.
 *
 * The names are compared with strcmp_P against strings in flash: in RAM, on the
 * Uno, they would take their length again for nothing.
 */

#include "Command.h"

#include <avr/pgmspace.h>
#include <stdint.h>
#include <string.h>

#include "MaxMatrix.h"
#include "Utility/Bit.h"
#include "Utility/Byte.h"

namespace max_matrix {

namespace {

/* the chars that end a line and divide it */
constexpr char kLineEnd{'\n'};
constexpr char kCarriageReturn{'\r'};
constexpr char kValueSeparator{'='};
constexpr char kListSeparator{','};

/* the largest numbers the commands take: a column, row or space fits a byte,
 * the shift interval a uint16_t */
constexpr uint16_t kMaxByte{0xFF};
constexpr uint16_t kMaxInterval{0xFFFF};

/* the digits a number is written in, and the first that hex writes as a letter
 */
constexpr uint8_t kDecimalBase{10};

/* a byte of `picture` is two hex digits, the high nibble first */
constexpr uint8_t kLowNibble{0x0Fu};
constexpr uint8_t kNibbleWidth{4u};

}  // namespace

// P-1: a line ends at its null character, so only that is set; emptying both
// buffers whole would cost flash and time for bytes no one reads.
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init)
Command::Command() {
  line_[0] = internal::kNullCharacter;
  shift_text_[0] = internal::kNullCharacter;
}

/*
 * Every line but an empty one gets one line back, so that a program can tell
 * which answer is whose by counting; the answer to `picture` stands in for its
 * `ok`.
 */
Result<CommandState> Command::Feed(Matrix& matrix, char c, Print& answer) {
  if (c == kCarriageReturn) {
    return CommandState::kPending;
  }
  if (c != kLineEnd) {
    if (line_length_ < MAXMATRIX_COMMAND_LINE_LENGTH) {
      line_[line_length_++] = c;
    } else {
      line_too_long_ = true;
    }
    return CommandState::kPending;
  }

  bool const too_long{line_too_long_};
  bool const empty{line_length_ == 0};

  line_[line_length_] = internal::kNullCharacter;
  line_length_ = 0;
  line_too_long_ = false;
  if (empty) {
    return CommandState::kPending;
  }
  Result<CommandState> result{Error::kLineTooLong};
  if (!too_long) {
    result = Execute(matrix, answer);
  }
  if (!result.IsOk()) {
    answer.print(F("error: "));
    answer.println(result.GetErrorText());
  } else if (result.GetValue() == CommandState::kDone) {
    answer.println(F("ok"));
  }
  return result;
}

// Splits the line at its first '=' into name and value, overwriting the '='
// with the end of the name, and passes it on by name.
Result<CommandState> Command::Execute(Matrix& matrix, Print& answer) {
  uint8_t const separator{Find(0, kValueSeparator)};
  Status status{Error::kOk};

  if (line_[separator] == internal::kNullCharacter) {
    if (0 == strcmp_P(line_.data(), PSTR("picture"))) {
      AnswerPicture(matrix, answer);
      return CommandState::kAnswered;
    }
    if (0 != strcmp_P(line_.data(), PSTR("clear"))) {
      return Error::kUnknownCommand;
    }
    matrix.Clear();
  } else {
    line_[separator] = internal::kNullCharacter;
    auto const value{static_cast<uint8_t>(separator + 1)};
    if (0 == strcmp_P(line_.data(), PSTR("text"))) {
      status = ExecuteText(matrix, value);
    } else if (0 == strcmp_P(line_.data(), PSTR("textShift"))) {
      status = ExecuteTextShift(matrix, value);
    } else if (0 == strcmp_P(line_.data(), PSTR("setDot"))) {
      status = ExecuteDot(matrix, value);
    } else {
      status = ExecuteSetting(matrix, value);
    }
  }
  if (!status.IsOk()) {
    return status.GetError();
  }
  return CommandState::kDone;
}

// Returns the position of the first `c` in the line from `from` on, or that of
// the line's null character if there is none. The line always ends with one,
// as Feed() puts it there.
// FN-16: private; its callers pass a position in the line and a named
// separator
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
uint8_t Command::Find(uint8_t from, char c) const {
  uint8_t position{from};

  while ((line_[position] != internal::kNullCharacter) &&
         (line_[position] != c)) {
    position++;
  }
  return position;
}

// textShift=TEXT copies the text out of the line buffer first: the shift reads
// it for as long as it runs, and the next line would overwrite it there.
Status Command::ExecuteTextShift(Matrix& matrix, uint8_t value) {
  /*
   * The text is part of Line, so it fits into shift_text_ as long as that is no
   * shorter; strlcpy() would check the same at run time, for 30 bytes of flash
   * in SerialControl
   */
  static_assert(sizeof(shift_text_) >= sizeof(line_),
                "ShiftText has to hold any text of a line");

  if (line_[value] == internal::kNullCharacter) {
    return Error::kNoText;
  }
  // strcpy() into a buffer that the static_assert above holds large enough.
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcpy(shift_text_.data(), &line_[value]);  // NOLINT(runtime/printf)
  return matrix.SetTextWithShift(shift_text_);
}

// text=TEXT clears the matrix first and sends clear and text as one picture; an
// empty text only clears it.
Status Command::ExecuteText(Matrix& matrix, uint8_t value) const {
  Batch const batch{matrix};

  matrix.Clear();
  return matrix.SetText(span<char const>{line_}.subspan(value));
}

/*
 * setDot=COLUMN,ROW,VALUE reads three numbers separated by commas, overwriting
 * the commas with ends of text; the value is 0 or 1. The dot is checked against
 * the panel here, because SetDot() leaves a dot off the panel out without a
 * word, and whoever typed the line should learn that it did nothing.
 */
Status Command::ExecuteDot(Matrix& matrix, uint8_t value) {
  uint8_t const row_separator{Find(value, kListSeparator)};
  if (line_[row_separator] == internal::kNullCharacter) {
    return Error::kNotANumber;
  }
  auto const row_text{static_cast<uint8_t>(row_separator + 1)};
  uint8_t const lit_separator{Find(row_text, kListSeparator)};
  if (line_[lit_separator] == internal::kNullCharacter) {
    return Error::kNotANumber;
  }
  line_[row_separator] = internal::kNullCharacter;
  line_[lit_separator] = internal::kNullCharacter;
  auto const lit_text{static_cast<uint8_t>(lit_separator + 1)};
  Result<uint16_t> const column{
      ConvertToNumber(value, matrix.GetNumberOfColumns() - 1)};
  Result<uint16_t> const row{ConvertToNumber(row_text, kRowNumberOfModule - 1)};
  Result<uint16_t> const lit{ConvertToNumber(lit_text, 1)};
  if (!column.IsOk()) {
    return column.GetError();
  }
  if (!row.IsOk()) {
    return row.GetError();
  }
  if (!lit.IsOk()) {
    return lit.GetError();
  }
  // ConvertToNumber() held both to the panel, which an int16_t holds.
  Point const dot{X{static_cast<int16_t>(column.GetValue())},
                  Y{static_cast<int16_t>(row.GetValue())}};
  matrix.SetDot(dot, lit.GetValue() == 1);
  return Error::kOk;
}

// The commands that set a number: intensity=0..15, speed=MILLISECONDS and
// space=COLUMNS.
Status Command::ExecuteSetting(Matrix& matrix, uint8_t value) const {
  char const* const name{line_.data()};

  if (0 == strcmp_P(name, PSTR("intensity"))) {
    Result<uint16_t> const number{
        ConvertToNumber(value, max7219::kIntensityMaxValue)};
    if (!number.IsOk()) {
      return number.GetError();
    }
    // ConvertToNumber() held it to the intensity's 15.
    matrix.SetIntensity(static_cast<uint8_t>(number.GetValue()));
  } else if (0 == strcmp_P(name, PSTR("speed"))) {
    Result<uint16_t> const number{ConvertToNumber(value, kMaxInterval)};
    if (!number.IsOk()) {
      return number.GetError();
    }
    matrix.SetShiftInterval(number.GetValue());
  } else if (0 == strcmp_P(name, PSTR("space"))) {
    Result<uint16_t> const number{ConvertToNumber(value, kMaxByte)};
    if (!number.IsOk()) {
      return number.GetError();
    }
    // ConvertToNumber() held it to a byte.
    matrix.SetSpaceBetweenChars(static_cast<uint8_t>(number.GetValue()));
  } else {
    return Error::kUnknownCommand;
  }
  return Error::kOk;
}

/*
 * `picture` is answered with the panel as the viewer sees it, rather than the
 * buffer as wired, so that the answer does not depend on the modules, their
 * type and orientation. The column is on the panel, so GetColumn() cannot fail.
 */
void Command::AnswerPicture(Matrix& matrix, Print& answer) {
  int16_t const columns{matrix.GetNumberOfColumns()};

  answer.print(F("picture="));
  for (int16_t column{0}; column < columns; column++) {
    uint8_t const value{matrix.GetColumn(X{column}).GetValue().ToInteger()};
    uint8_t const high_nibble{Bit<uint8_t>::ShiftDown(value, kNibbleWidth)};
    uint8_t const low_nibble{Bit<uint8_t>::ReadBitGroup(value, kLowNibble)};
    answer.write(static_cast<uint8_t>(ConvertToHexDigit(high_nibble)));
    answer.write(static_cast<uint8_t>(ConvertToHexDigit(low_nibble)));
  }
  answer.println();
}

/*
 * Converts a decimal number of digits only, at least one, up to `maximum`. It
 * stops counting as soon as the number is past `maximum`, so that a long run of
 * digits cannot overflow into a small number, but reads on to the end all the
 * same, so that "999x" is no number rather than too large.
 */
// FN-16: private; its callers pass a position in the line and a named
// maximum
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Result<uint16_t> Command::ConvertToNumber(uint8_t text,
                                          uint16_t maximum) const {
  uint32_t value{0};

  if (line_[text] == internal::kNullCharacter) {
    return Error::kNotANumber;
  }
  for (uint8_t position{text}; line_[position] != internal::kNullCharacter;
       position++) {
    char const digit{line_[position]};
    if ((digit < '0') || (digit > '9')) {
      return Error::kNotANumber;
    }
    if (value <= maximum) {
      value = (value * kDecimalBase) + static_cast<uint32_t>(digit - '0');
    }
  }
  if (value > maximum) {
    return Error::kOutOfRange;
  }
  return static_cast<uint16_t>(value);
}

// Computed rather than looked up: a table of the digits would lie in RAM on the
// Uno.
char Command::ConvertToHexDigit(uint8_t value) {
  return static_cast<char>(
      (value < kDecimalBase) ? ('0' + value) : ('A' + value - kDecimalBase));
}

}  // namespace max_matrix
