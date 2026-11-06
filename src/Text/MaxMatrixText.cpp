// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings on the matrix, standing or shifting in from the right.
 */

#include "MaxMatrixText.h"

MaxMatrixText::MaxMatrixText()
{
    string_ = NULL;
    sprite_shift_counter_ = kShiftIdle;
    space_between_chars_ = MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE;
    font_ = NULL;
    glyph_.columns = NULL;
    glyph_.width = 0;
    glyph_.height = 0;
}

MaxMatrixStatus MaxMatrixText::SetFont(uint8_t const* font)
{
    MaxMatrixStatus const status = MaxMatrixFont::CheckFont(font);

    if(status.IsOk()) font_ = font;
    return status;
}

MaxMatrixStatus MaxMatrixText::SetChar(MaxMatrixPanel& panel, int16_t x, int16_t y, char c) const
{
    MaxMatrixGlyph glyph;
    MaxMatrixStatus const status = FindGlyph(c, &glyph);

    if(status.IsOk()) SetGlyph(panel, x, y, glyph);
    return status;
}

MaxMatrixStatus MaxMatrixText::SetCharWithShift(char c)
{
    MaxMatrixStatus const status = FindGlyph(c, &glyph_);

    if(status.IsOk()) sprite_shift_counter_ = kShiftStart;
    return status;
}

MaxMatrixStatus MaxMatrixText::SetText(MaxMatrixPanel& panel, char const* string) const
{
    int16_t char_column = 0;
    char c;

    if(string == NULL) return MaxMatrixError::kNoText;
    while(*string != STD_NULL_CHARACTER)
    {
        string += MaxMatrixFont::DecodeChar(string, &c);
        MaxMatrixGlyph glyph;
        MaxMatrixStatus const status = FindGlyph(c, &glyph);
        if(!status.IsOk()) return status;
        SetGlyph(panel, char_column, 0, glyph);
        char_column = static_cast<int16_t>(char_column + glyph.width + space_between_chars_);
        if(char_column > MAXMATRIX_NUMBER_OF_COLUMNS) break;
    }
    return MaxMatrixError::kOk;
}

MaxMatrixStatus MaxMatrixText::SetTextWithShift(char* string)
{
    if(string == NULL) return MaxMatrixError::kNoText;
    sprite_shift_counter_ = kShiftIdle;
    string_ = string;
    return MaxMatrixError::kOk;
}

/*
 * The glyph of `c` in the font that is set, into `glyph`, which stays as it was if the font has
 * none. One place that unpacks the result of MaxMatrixFont::GetGlyph() rather than four: 62 bytes
 * less of flash in SerialControl, which draws through all four.
 */
MaxMatrixStatus MaxMatrixText::FindGlyph(char c, MaxMatrixGlyph* glyph) const
{
    MaxMatrixResult<MaxMatrixGlyph> const found = MaxMatrixFont::GetGlyph(GetFont(), c);

    if(found.IsOk()) *glyph = found.GetValue();
    return found.GetError();
}

void MaxMatrixText::SetSprite(MaxMatrixPanel& panel, int16_t x, int16_t y, MaxMatrixSprite const sprite)
{
    for(byte sprite_column = 0; sprite_column < sprite[kSpriteWidth]; sprite_column++) {
        SetSpriteColumn(panel, static_cast<int16_t>(x + sprite_column), y, sprite[kSpriteHeight], sprite[kSpriteColumn1 + sprite_column]);
    }
}

void MaxMatrixText::SetGlyph(MaxMatrixPanel& panel, int16_t x, int16_t y, MaxMatrixGlyph const& glyph)
{
    for(byte glyph_column = 0; glyph_column < glyph.width; glyph_column++) {
        SetSpriteColumn(panel, static_cast<int16_t>(x + glyph_column), y, glyph.height, pgm_read_byte(glyph.columns + glyph_column));
    }
}

/*
 * Draws one column of a sprite or glyph `height` rows high, bit 0 at `y`: a whole column at once
 * if it fills one, otherwise dot by dot, as the rows below it are not its own. The panel leaves out
 * what lies off it.
 */
void MaxMatrixText::SetSpriteColumn(MaxMatrixPanel& panel, int16_t x, int16_t y, byte height, byte column)
{
    if(height == MAXMATRIX_ROW_NUMBER_OF_MODULE && y == 0) {
        panel.SetColumn(x, column);
    } else {
        for(byte row = 0; row < height; row++) panel.SetDot(x, static_cast<int16_t>(y + row), bitRead(column, row));
    }
}

void MaxMatrixText::SetSpaceBetweenChars(byte space_between_chars)
{
    /* every byte value is a valid number of spaces, so it cannot fail */
    space_between_chars_ = space_between_chars;
}

bool MaxMatrixText::CharShiftTask(MaxMatrixPanel& panel)
{
    panel.ShiftLeft(false, true);
    sprite_shift_counter_++;

    /* if sprite is completed set only spaces */
    if(sprite_shift_counter_ <= glyph_.width) {
        SetGlyph(panel, MAXMATRIX_NUMBER_OF_COLUMNS - sprite_shift_counter_, 0, glyph_);
    }
    /* if sprite and spaces are completed task has finished */
    else if(sprite_shift_counter_ == static_cast<uint16_t>(glyph_.width + space_between_chars_ + 1)) {
        sprite_shift_counter_ = kShiftIdle;
        return false;
    }
    return true;
}

bool MaxMatrixText::StringShiftTask(MaxMatrixPanel& panel)
{
    char c;

    /* has char shift task finished shifting sprite or char */
    if(sprite_shift_counter_ == kShiftIdle)
    {
        /*
         * Unknown chars are skipped within this call: a call spent on one sent nothing, and the
         * text stood still for a step. So read on to the next known char or to the end of the
         * string.
         */
        while(*string_ != STD_NULL_CHARACTER && sprite_shift_counter_ == kShiftIdle) {
            string_ += MaxMatrixFont::DecodeChar(string_, &c);
            if(FindGlyph(c, &glyph_).IsOk()) sprite_shift_counter_ = kShiftStart;
        }
        if(sprite_shift_counter_ == kShiftStart) {
            CharShiftTask(panel);
        } else { /* end of string reached, task has finished */
            panel.ShiftLeft(false, true);
            return false;
        }
    } else {
        /* go on shifting Sprite; the end of a char is not the end of the string */
        CharShiftTask(panel);
    }
    return true;
}
