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

/* Every function that draws is given the panel rather than the class keeping a reference to it. A reference costs two
 * bytes of RAM, and avr-gcc no longer sees through it that the panel is the one inside the global MaxMatrix of the
 * sketch: ScrollText was 86 bytes of flash larger with it. */
class MaxMatrixText
{
  private:
    /* the columns of a char shifted in so far, its space included: kShiftIdle while no char is shifting, and
     * kShiftStart when one begins. A count rather than a state, so the two values are constants, not an enum. */
    static constexpr byte kShiftIdle = 0;
    static constexpr byte kShiftStart = 1;

    MaxMatrixSprite sprite_buffer_;
    byte sprite_shift_counter_;
    byte space_between_chars_;
    char* string_;

  public:
    MaxMatrixText();

    // get methods
    byte GetSpaceBetweenChars() const { return space_between_chars_; }

    // set methods
    static MaxMatrixStatus SetChar(MaxMatrixPanel&, int16_t, int16_t, char);
    MaxMatrixStatus SetCharWithShift(char);
    MaxMatrixStatus SetText(MaxMatrixPanel&, char const*) const;
    MaxMatrixStatus SetTextWithShift(char*);
    void SetSpaceBetweenChars(byte);
    static void SetSprite(MaxMatrixPanel&, int16_t, int16_t, MaxMatrixSprite const);

    // methods
    bool CharShiftTask(MaxMatrixPanel&);
    bool StringShiftTask(MaxMatrixPanel&);
};

#endif
