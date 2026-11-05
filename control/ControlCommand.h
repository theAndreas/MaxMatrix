// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The command lines of SerialControl, as the control program sends them.
 *
 * The other half of MaxMatrixCommand, which reads them on the board.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLCOMMAND_H_
#define MAXMATRIX_CONTROL_CONTROLCOMMAND_H_

#include <stdint.h>
#include <string>

/*!
 * \brief The command lines of SerialControl, as the control program sends them.
 *
 * Each function makes one line, without its line feed. A text is UTF-8, which MaxMatrix decodes
 * into latin-1 on the board; the old Windows tool sent the code page of Windows instead.
 *
 * It has no state of its own, so every function is static, as in MaxMatrixFont.
 */
class ControlCommand
{
  public:
    /*!
     * \brief Makes the line that shows a text, from the left edge or shifting in from the right.
     *
     * \param[in] text The text, UTF-8.
     * \param[in] shift True to shift it in, false to show it at once.
     * \return The line.
     */
    static std::string MakeText(std::string const& text, bool shift);

    /*!
     * \brief Makes the line that sets the brightness.
     *
     * \param[in] intensity The brightness, 0 to 15.
     * \return The line.
     */
    static std::string MakeIntensity(uint8_t intensity);

    /*!
     * \brief Makes the line that sets how fast a text shifts.
     *
     * \param[in] milliseconds The time per column.
     * \return The line.
     */
    static std::string MakeSpeed(uint16_t milliseconds);

    /*!
     * \brief Makes the line that sets the empty columns between the chars of a text.
     *
     * \param[in] columns The columns.
     * \return The line.
     */
    static std::string MakeSpace(uint8_t columns);

    /*!
     * \brief Makes the line that lights a dot or turns it off.
     *
     * \param[in] column The column, from the left, as the viewer sees it.
     * \param[in] row The row, from the top.
     * \param[in] lit True to light it, false to turn it off.
     * \return The line.
     */
    static std::string MakeDot(uint8_t column, uint8_t row, bool lit);

    /*!
     * \brief Makes the line that turns every LED off.
     *
     * \return The line.
     */
    static std::string MakeClear();

    /*!
     * \brief Returns whether SerialControl takes `line` whole.
     *
     * It drops a line longer than `MAXMATRIX_COMMAND_LINE_LENGTH` bytes, which a long text in UTF-8
     * soon is, and a line break would end the line early.
     *
     * \param[in] line The line, without its line feed.
     * \return True if it fits into the board's line buffer and holds no line break.
     */
    static bool Fits(std::string const& line);
};

#endif
