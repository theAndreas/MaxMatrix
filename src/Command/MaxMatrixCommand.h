// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Commands for the matrix, one per line of text.
 *
 * The lines a control page or program sends to a board over Serial, read char by char and carried
 * out on a MaxMatrix. Knows nothing of where the chars come from.
 */

#ifndef MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_
#define MAXMATRIX_SRC_COMMAND_MAXMATRIXCOMMAND_H_

#include "Arduino.h"
#include "MaxMatrixStandardTypes.h"
#include "MaxMatrixError.h"
#include "MaxMatrixConfig.h"

/*!
 * \brief What MaxMatrixCommand::Feed() says about the char it was given, when the line did not
 *        fail; why it failed is the error beside it.
 */
enum class MaxMatrixCommandState : uint8_t {
    kPending, /*!< The line goes on, or was empty. */
    kDone     /*!< The line was a command, and it is carried out. */
};

class MaxMatrix;

/*!
 * \brief The lines of text a control page or program sends, carried out on a MaxMatrix.
 *
 * One command per line, as the Windows tool in AndreasBur/Led-Matrix sends them:
 *
 * | Line | Does |
 * |---|---|
 * | `text=TEXT` | Clears the matrix and shows the text from the left edge, as one picture. |
 * | `textShift=TEXT` | Shifts the text in from the right. |
 * | `intensity=0..15` | Sets the brightness. |
 * | `speed=MILLISECONDS` | Sets the shift interval per column. |
 * | `space=COLUMNS` | Sets the empty columns between the chars of a text. |
 * | `setDot=COLUMN,ROW,VALUE` | Lights a dot, with 1, or turns it off, with 0. |
 * | `clear` | Turns every LED off. |
 *
 * \code{.cpp}
 * MaxMatrixCommand command;
 *
 * void loop() {
 *     while(Serial.available()) {
 *         MaxMatrixResult<MaxMatrixCommandState> const state = command.Feed(matrix, Serial.read());
 *         if(!state.IsOk()) Serial.println(state.GetErrorText());
 *     }
 *     matrix.ShiftTask();
 * }
 * \endcode
 *
 * A sketch that does not use it pays nothing. It costs twice `MAXMATRIX_COMMAND_LINE_LENGTH` bytes
 * of RAM: the line, and the text of `textShift=`, which MaxMatrix::SetTextWithShift() reads for as
 * long as it shifts while the next line fills the line buffer.
 */
class MaxMatrixCommand
{
  private:
    /*
     * As MaxMatrixText is, the class is handed the matrix by every call rather than keeping a
     * reference to it.
     */
    char line_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
    char shift_text_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
    byte line_length_;
    bool line_too_long_;

    MaxMatrixStatus Execute(MaxMatrix& matrix, char* text);
    MaxMatrixStatus ExecuteTextShift(MaxMatrix& matrix, char const* value);
    static MaxMatrixStatus ExecuteText(MaxMatrix& matrix, char const* value);
    static MaxMatrixStatus ExecuteDot(MaxMatrix& matrix, char* value);
    static MaxMatrixStatus ExecuteSetting(MaxMatrix& matrix, char const* name, char const* value);
    static MaxMatrixResult<uint16_t> ConvertToNumber(char const* text, uint16_t maximum);

  public:
    /*! \brief Makes a reader at the start of an empty line. */
    MaxMatrixCommand();

    /*!
     * \brief Takes the next char of a command line, and carries the line out at its end.
     *
     * The chars are collected up to a line feed. A carriage return is skipped, so that lines may
     * end in `\\r\\n` as well; an empty line is no command. A line too long for the buffer is
     * dropped whole, rather than carried out cut short.
     *
     * \param[in,out] matrix The matrix the command acts on.
     * \param[in] c The next char, as `Serial.read()` gives it.
     * \return MaxMatrixCommandState::kPending while the line goes on or after an empty line, or
     *         MaxMatrixCommandState::kDone once it was carried out. MaxMatrixError::kLineTooLong if
     *         the line did not fit and is dropped; MaxMatrixError::kUnknownCommand if no command
     *         has its name; or why the command failed: kNoText, kNotANumber, kOutOfRange or
     *         kUnknownChar.
     */
    MaxMatrixResult<MaxMatrixCommandState> Feed(MaxMatrix& matrix, char c);
};

#endif
