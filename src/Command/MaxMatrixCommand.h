// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Commands for the matrix, one per line of text.
 *
 * The lines a control page or program sends to a board over Serial, read char
 * by char, carried out on a Matrix and answered on a Print. Knows nothing of
 * where the chars come from.
 */

#ifndef MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_
#define MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "MaxMatrixStandardTypes.h"

namespace max_matrix {

/*!
 * \brief What Command::Feed() says about the char it was given, when the line
 *        did not fail; why it failed is the error beside it.
 */
enum class CommandState : uint8_t {
  kPending, /*!< The line goes on, or was empty. */
  kDone,    /*!< The line was a command, and it is carried out. */
  kAnswered /*!< The line asked for something, and the answer is printed. */
};

class Matrix;

/*!
 * \brief The lines of text a control page or program sends, carried out on a
 *        Matrix.
 *
 * One command per line, as the Windows tool in AndreasBur/Led-Matrix sends
 * them:
 *
 * | Line | Does |
 * |---|---|
 * | `text=TEXT` | Clears the matrix and shows the text from the left edge, as
 * one picture. | | `textShift=TEXT` | Shifts the text in from the right. | |
 * `intensity=0..15` | Sets the brightness. | | `speed=MILLISECONDS` | Sets the
 * shift interval per column. | | `space=COLUMNS` | Sets the empty columns
 * between the chars of a text. | | `setDot=COLUMN,ROW,VALUE` | Lights a dot,
 * with 1, or turns it off, with 0. | | `clear` | Turns every LED off. | |
 * `picture` | Asks what the matrix shows. |
 *
 * Every line is answered with one line: `ok`, `error: ` and the reason, such as
 * `error: out of range` for `intensity=16`, or the answer to `picture`,
 * `picture=` and two hex digits per column as the viewer sees it, from the
 * left, bit 0 the top row. A program that shows the matrix asks for it over and
 * over; a board cannot be told what it shows otherwise.
 *
 * \code{.cpp}
 * Command command;
 *
 * void loop() {
 *     while(Serial.available() > 0) command.Feed(matrix, Serial.read(),
 * Serial); matrix.ShiftTask();
 * }
 * \endcode
 *
 * A sketch that does not use it pays nothing. It costs twice
 * `MAXMATRIX_COMMAND_LINE_LENGTH` bytes of RAM: the line, and the text of
 * `textShift=`, which Matrix::SetTextWithShift() reads for as long as it
 * shifts while the next line fills the line buffer.
 */
class Command {
 private:
  /*
   * As internal::Text is, the class is handed the matrix by every call rather
   * than keeping a reference to it.
   */
  char line_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
  char shift_text_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
  byte line_length_;
  bool line_too_long_;

  Result<CommandState> Execute(Matrix& matrix, char* text, Print& answer);
  Status ExecuteTextShift(Matrix& matrix, char const* value);
  static Status ExecuteText(Matrix& matrix, char const* value);
  static Status ExecuteDot(Matrix& matrix, char* value);
  static Status ExecuteSetting(Matrix& matrix, char const* name,
                               char const* value);
  static void AnswerPicture(Matrix& matrix, Print& answer);
  static Result<uint16_t> ConvertToNumber(char const* text, uint16_t maximum);
  static char ConvertToHexDigit(byte value);

 public:
  /*! \brief Makes a reader at the start of an empty line. */
  Command();

  /*!
   * \brief Takes the next char of a command line, and carries the line out and
   *        answers it at its end.
   *
   * The chars are collected up to a line feed. A carriage return is skipped, so
   * that lines may end in `\\r\\n` as well; an empty line is no command and
   * gets no answer. A line too long for the buffer is dropped whole, rather
   * than carried out cut short.
   *
   * \param[in,out] matrix The matrix the command acts on.
   * \param[in] c The next char, as `Serial.read()` gives it.
   * \param[out] answer Where the answer to a line goes, as a rule `Serial`.
   * \return CommandState::kPending while the line goes on or after an empty
   *         line, CommandState::kDone once it was carried out, or
   *         CommandState::kAnswered once the answer to `picture` is printed.
   *         Error::kLineTooLong if the line did not fit and is dropped;
   *         Error::kUnknownCommand if no command has its name; or why the
   *         command failed: kNoText, kNotANumber, kOutOfRange or kUnknownChar.
   */
  Result<CommandState> Feed(Matrix& matrix, char c, Print& answer);
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_
