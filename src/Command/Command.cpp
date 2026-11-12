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

#include "MaxMatrix.h"

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
constexpr byte kLowNibble{0x0FU};
constexpr uint8_t kNibbleWidth{4U};

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

  bool const too_long = line_too_long_;
  bool const empty = line_length_ == 0;

  line_[line_length_] = internal::kNullCharacter;
  line_length_ = 0;
  line_too_long_ = false;
  if (empty) {
    return CommandState::kPending;
  }
  Result<CommandState> result = Error::kLineTooLong;
  if (!too_long) {
    result = Execute(matrix, line_, answer);
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
Result<CommandState> Command::Execute(Matrix& matrix, char* text,
                                      Print& answer) {
  char* value = strchr(text, kValueSeparator);
  Status status = Error::kOk;

  if (value == nullptr) {
    if (0 == strcmp_P(text, PSTR("picture"))) {
      AnswerPicture(matrix, answer);
      return CommandState::kAnswered;
    }
    if (0 != strcmp_P(text, PSTR("clear"))) {
      return Error::kUnknownCommand;
    }
    matrix.Clear();
  } else {
    *value++ = internal::kNullCharacter;
    if (0 == strcmp_P(text, PSTR("text"))) {
      status = ExecuteText(matrix, value);
    } else if (0 == strcmp_P(text, PSTR("textShift"))) {
      status = ExecuteTextShift(matrix, value);
    } else if (0 == strcmp_P(text, PSTR("setDot"))) {
      status = ExecuteDot(matrix, value);
    } else {
      status = ExecuteSetting(matrix, text, value);
    }
  }
  if (!status.IsOk()) {
    return status.GetError();
  }
  return CommandState::kDone;
}

// textShift=TEXT copies the text out of the line buffer first: the shift reads
// it for as long as it runs, and the next line would overwrite it there.
Status Command::ExecuteTextShift(Matrix& matrix, char const* value) {
  /*
   * The text is part of Line, so it fits into shift_text_ as long as that is no
   * shorter; strlcpy() would check the same at run time, for 30 bytes of flash
   * in SerialControl
   */
  static_assert(sizeof(shift_text_) >= sizeof(line_),
                "ShiftText has to hold any text of a line");

  if (*value == internal::kNullCharacter) {
    return Error::kNoText;
  }
  // strcpy() into a buffer that the static_assert above holds large enough.
  // NOLINTNEXTLINE(clang-analyzer-security.insecureAPI.strcpy)
  strcpy(shift_text_, value);  // NOLINT(runtime/printf)
  return matrix.SetTextWithShift(shift_text_);
}

// text=TEXT clears the matrix first and sends clear and text as one picture; an
// empty text only clears it.
Status Command::ExecuteText(Matrix& matrix, char const* value) {
  Batch const batch{matrix};

  matrix.Clear();
  return matrix.SetText(value);
}

/*
 * setDot=COLUMN,ROW,VALUE reads three numbers separated by commas, overwriting
 * the commas with ends of text; the value is 0 or 1. The dot is checked against
 * the panel here, because SetDot() leaves a dot off the panel out without a
 * word, and whoever typed the line should learn that it did nothing.
 */
Status Command::ExecuteDot(Matrix& matrix, char* value) {
  char* row_text = strchr(value, kListSeparator);
  char* lit_text =
      row_text == nullptr ? nullptr : strchr(row_text + 1, kListSeparator);

  if (lit_text == nullptr) {
    return Error::kNotANumber;
  }
  *row_text = internal::kNullCharacter;
  row_text++;
  *lit_text = internal::kNullCharacter;
  lit_text++;
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
  matrix.SetDot(static_cast<int16_t>(column.GetValue()),
                static_cast<int16_t>(row.GetValue()), lit.GetValue() == 1);
  return Error::kOk;
}

// The commands that set a number: intensity=0..15, speed=MILLISECONDS and
// space=COLUMNS.
Status Command::ExecuteSetting(Matrix& matrix, char const* name,
                               char const* value) {
  if (0 == strcmp_P(name, PSTR("intensity"))) {
    Result<uint16_t> const number{
        ConvertToNumber(value, max7219::kIntensityMaxValue)};
    if (!number.IsOk()) {
      return number.GetError();
    }
    // ConvertToNumber() held it to the intensity's 15.
    matrix.SetIntensity(static_cast<byte>(number.GetValue()));
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
    matrix.SetSpaceBetweenChars(static_cast<byte>(number.GetValue()));
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
  int16_t const columns = matrix.GetNumberOfColumns();

  answer.print(F("picture="));
  for (int16_t column = 0; column < columns; column++) {
    byte const value = matrix.GetColumn(column).GetValue();
    byte const high_nibble{static_cast<byte>(value >> kNibbleWidth)};
    byte const low_nibble{static_cast<byte>(value & kLowNibble)};
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
Result<uint16_t> Command::ConvertToNumber(char const* text, uint16_t maximum) {
  uint32_t value = 0;

  if (*text == internal::kNullCharacter) {
    return Error::kNotANumber;
  }
  for (; *text != internal::kNullCharacter; text++) {
    if (*text < '0' || *text > '9') {
      return Error::kNotANumber;
    }
    if (value <= maximum) {
      value = (value * kDecimalBase) + static_cast<uint32_t>(*text - '0');
    }
  }
  if (value > maximum) {
    return Error::kOutOfRange;
  }
  return static_cast<uint16_t>(value);
}

// Computed rather than looked up: a table of the digits would lie in RAM on the
// Uno.
char Command::ConvertToHexDigit(byte value) {
  return static_cast<char>(value < kDecimalBase ? '0' + value
                                                : 'A' + value - kDecimalBase);
}

}  // namespace max_matrix
