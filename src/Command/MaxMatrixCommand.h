// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Commands for the matrix, one per line of text.
 *
 * The lines a control page or program sends to a board over Serial, read char
 * by char, carried out on a MaxMatrix and answered on a Print. Knows nothing of
 * where the chars come from.
 */

#ifndef MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_
#define MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "MaxMatrixStandardTypes.h"

/*!
 * \brief What MaxMatrixCommand::Feed() says about the char it was given, when
 *        the line did not fail; why it failed is the error beside it.
 */
enum class MaxMatrixCommandState : uint8_t {
  kPending, /*!< The line goes on, or was empty. */
  kDone,    /*!< The line was a command, and it is carried out. */
  kAnswered /*!< The line asked for something, and the answer is printed. */
};

class MaxMatrix;

/*!
 * \brief The lines of text a control page or program sends, carried out on a
 *        MaxMatrix.
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
 * MaxMatrixCommand command;
 *
 * void loop() {
 *     while(Serial.available() > 0) command.Feed(matrix, Serial.read(),
 * Serial); matrix.ShiftTask();
 * }
 * \endcode
 *
 * A sketch that does not use it pays nothing. It costs twice
 * `MAXMATRIX_COMMAND_LINE_LENGTH` bytes of RAM: the line, and the text of
 * `textShift=`, which MaxMatrix::SetTextWithShift() reads for as long as it
 * shifts while the next line fills the line buffer.
 */
class MaxMatrixCommand {
 private:
  /*
   * As MaxMatrixText is, the class is handed the matrix by every call rather
   * than keeping a reference to it.
   */
  char line_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
  char shift_text_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
  byte line_length_;
  bool line_too_long_;

  MaxMatrixResult<MaxMatrixCommandState> Execute(MaxMatrix& matrix, char* text,
                                                 Print& answer);
  MaxMatrixStatus ExecuteTextShift(MaxMatrix& matrix, char const* value);
  static MaxMatrixStatus ExecuteText(MaxMatrix& matrix, char const* value);
  static MaxMatrixStatus ExecuteDot(MaxMatrix& matrix, char* value);
  static MaxMatrixStatus ExecuteSetting(MaxMatrix& matrix, char const* name,
                                        char const* value);
  static void AnswerPicture(MaxMatrix& matrix, Print& answer);
  static MaxMatrixResult<uint16_t> ConvertToNumber(char const* text,
                                                   uint16_t maximum);
  static char ConvertToHexDigit(byte value);

 public:
  /*! \brief Makes a reader at the start of an empty line. */
  MaxMatrixCommand();

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
   * \return MaxMatrixCommandState::kPending while the line goes on or after an
   *         empty line, MaxMatrixCommandState::kDone once it was carried out,
   *         or MaxMatrixCommandState::kAnswered once the answer to `picture` is
   *         printed. MaxMatrixError::kLineTooLong if the line did not fit and
   *         is dropped; MaxMatrixError::kUnknownCommand if no command has its
   *         name; or why the command failed: kNoText, kNotANumber, kOutOfRange
   *         or kUnknownChar.
   */
  MaxMatrixResult<MaxMatrixCommandState> Feed(MaxMatrix& matrix, char c,
                                              Print& answer);
};

#endif  // MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_
