// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings in the font, drawn on the matrix as the viewer sees it, standing or shifting
 * in.
 */

#ifndef MAXMATRIX_SRC_TEXT_MAXMATRIXTEXT_H_
#define MAXMATRIX_SRC_TEXT_MAXMATRIXTEXT_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "Font/MaxMatrixFont.h"
#include "Matrix/MaxMatrixPanel.h"

/*!
 * \brief Text on the matrix: chars and strings in the font, standing or shifting in.
 *
 * Every function that draws is given the panel it draws on. A text shifts in from the right, one
 * column per call of CharShiftTask() or StringShiftTask(), which MaxMatrix::ShiftTask() makes.
 */
class MaxMatrixText
{
  private:
    /*
     * Every function that draws is given the panel rather than the class keeping a reference to
     * it. A reference costs two bytes of RAM, and avr-gcc no longer sees through it that the panel
     * is the one inside the global MaxMatrix of the sketch: ScrollText was 86 bytes of flash larger
     * with it.
     */

    /*
     * The columns of a char shifted in so far, its space included: kShiftIdle while no char is
     * shifting, and kShiftStart when one begins. A count rather than a state, so the two values are
     * constants, not an enum.
     */
    static constexpr byte kShiftIdle = 0;
    static constexpr byte kShiftStart = 1;

    MaxMatrixSprite sprite_buffer_;
    byte sprite_shift_counter_;
    byte space_between_chars_;
    char* string_;

  public:
    /*! \brief Makes a text with nothing to shift. */
    MaxMatrixText();

    /*! \brief Returns the empty columns between two chars of a text. */
    byte GetSpaceBetweenChars() const { return space_between_chars_; }

    /*!
     * \brief Draws a char with its top left corner at `x`, `y`.
     *
     * What lies off the panel is left out.
     *
     * \param[in,out] panel The panel to draw on.
     * \param[in] x The column of the left edge; it may lie off the panel.
     * \param[in] y The row of the top edge; it may lie off the panel.
     * \param[in] c The char, latin-1.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kUnknownChar if the font has no sprite for
     *         the char; nothing is drawn then.
     */
    static MaxMatrixStatus SetChar(MaxMatrixPanel& panel, int16_t x, int16_t y, char c);

    /*!
     * \brief Starts shifting a char in from the right, until it stands at the right edge.
     *
     * \param[in] c The char, latin-1.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kUnknownChar if the font has no sprite for
     *         the char; nothing starts then.
     */
    MaxMatrixStatus SetCharWithShift(char c);

    /*!
     * \brief Draws a text from the left edge, as far as it fits.
     *
     * \param[in,out] panel The panel to draw on.
     * \param[in] string The text, UTF-8 or latin-1.
     * \return MaxMatrixError::kOk; MaxMatrixError::kNoText if `string` is null; or
     *         MaxMatrixError::kUnknownChar if the font lacks a char of the text, with the chars
     *         before it drawn.
     */
    MaxMatrixStatus SetText(MaxMatrixPanel& panel, char const* string) const;

    /*!
     * \brief Starts shifting a text in from the right, until its last char stands at the right
     *        edge.
     *
     * \param[in] string The text, UTF-8 or latin-1. It is kept as a pointer, so it has to live
     *                   until the text stands.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kNoText if `string` is null; nothing starts
     *         then.
     */
    MaxMatrixStatus SetTextWithShift(char* string);

    /*!
     * \brief Sets the empty columns between two chars of a text.
     *
     * \param[in] space_between_chars The number of empty columns.
     */
    void SetSpaceBetweenChars(byte space_between_chars);

    /*!
     * \brief Draws a sprite with its top left corner at `x`, `y`.
     *
     * What lies off the panel is left out.
     *
     * \param[in,out] panel The panel to draw on.
     * \param[in] x The column of the left edge; it may lie off the panel.
     * \param[in] y The row of the top edge; it may lie off the panel.
     * \param[in] sprite Its width, its height and up to five columns, bit 0 the top row.
     */
    static void SetSprite(MaxMatrixPanel& panel, int16_t x, int16_t y, MaxMatrixSprite const sprite);

    /*!
     * \brief Shifts the char one column further.
     *
     * Called by MaxMatrix::ShiftTask(), or by StringShiftTask() for each char of a text, until it
     * returns false.
     *
     * \param[in,out] panel The panel to draw on.
     * \return True while the char or the space after it is still shifting.
     */
    bool CharShiftTask(MaxMatrixPanel& panel);

    /*!
     * \brief Shifts the text one column further.
     *
     * Called by MaxMatrix::ShiftTask() until it returns false.
     *
     * \param[in,out] panel The panel to draw on.
     * \return True while the text is still shifting.
     */
    bool StringShiftTask(MaxMatrixPanel& panel);
};

#endif
