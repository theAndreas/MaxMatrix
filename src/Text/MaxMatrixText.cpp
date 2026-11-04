// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings on the matrix, standing or shifting in from the right.
 */

#include "MaxMatrixText.h"

/*! \brief          MaxMatrixText constructor
 *  \details        Instantiation of the text on a matrix
 */
MaxMatrixText::MaxMatrixText()
{
    string_ = NULL;
    sprite_shift_counter_ = kShiftIdle;
    space_between_chars_ = MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE;
    for(byte i = 0; i < MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS; i++) sprite_buffer_[i] = 0;
}

/*! \brief          prints a char on the matrix
 *                  
 *  \param[in]      panel       the matrix the text is drawn on
 *  \param[in]      x           x coordinate
 *  \param[in]      y           y coordinate
 *  \param[in]      c        char to print on matrix
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kUnknownChar    the font has no sprite for the char; nothing is drawn
 */
MaxMatrixStatus MaxMatrixText::SetChar(MaxMatrixPanel& panel, int16_t x, int16_t y, char c)
{
    MaxMatrixSprite sprite;
    MaxMatrixStatus status = MaxMatrixFont::GetCharSprite(c, &sprite);

    if(status.IsOk()) SetSprite(panel, x, y, sprite);
    return status;
}

/*! \brief          shifts a char into the matrix and stops on the right side
 *  \details        this function shifts a given char into the matrix and stops shifting on the right side.
 *                  Every shift process will be triggered by calling the shiftMainFunction. 
 *                  
 *  \param[in]      c        char to print on matrix
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kUnknownChar    the font has no sprite for the char; nothing starts
 */
MaxMatrixStatus MaxMatrixText::SetCharWithShift(char c)
{
    MaxMatrixStatus status = MaxMatrixFont::GetCharSprite(c, &sprite_buffer_);

    if(status.IsOk()) sprite_shift_counter_ = kShiftStart;
    return status;
}

/*! \brief          prints a string on the matrix
 *                  
 *  \param[in]      panel       the matrix the text is drawn on
 *  \param[in]      string        string to print on matrix
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kNoText         the string is NULL
 *                  MaxMatrixError::kUnknownChar    the font lacks a char; the chars before it are drawn
 */
MaxMatrixStatus MaxMatrixText::SetText(MaxMatrixPanel& panel, char const* string) const
{
    MaxMatrixSprite sprite;
    int16_t char_column = 0;
    char c;

    if(string == NULL) return MaxMatrixError::kNoText;
    while(*string != STD_NULL_CHARACTER)
    {
        string += MaxMatrixFont::DecodeChar(string, &c);
        MaxMatrixStatus status = MaxMatrixFont::GetCharSprite(c, &sprite);
        if(!status.IsOk()) return status;
        SetSprite(panel, char_column, 0, sprite);
        char_column = static_cast<int16_t>(char_column + sprite[kSpriteWidth] + space_between_chars_);
        if(char_column > MAXMATRIX_NUMBER_OF_COLUMNS) break;
    }
    return MaxMatrixError::kOk;
}

/*! \brief          shifts a string into the matrix and stops after the last char on the right side
 *  \details        this function shifts a given string into the matrix and stops shifting on the right side,
 *                  when the last char appears on the matrix.
 *
 *  \param[in]      string      string to print on matrix
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kNoText         the string is NULL; nothing starts
 */
MaxMatrixStatus MaxMatrixText::SetTextWithShift(char* string)
{
    if(string == NULL) return MaxMatrixError::kNoText;
    sprite_shift_counter_ = kShiftIdle;
    string_ = string;
    return MaxMatrixError::kOk;
}

/*! \brief          print sprite on matrix
 *  \details        this function prints an sprite on the matrix located to the given coordinates
 *                  
 *  \param[in]      panel       the matrix the text is drawn on
 *  \param[in]      x           x coordinate
 *  \param[in]      y           y coordinate
 *  \param[in]      sprite      sprite from sprite table (width, height, value column 1 ... 5)
 */
void MaxMatrixText::SetSprite(MaxMatrixPanel& panel, int16_t x, int16_t y, MaxMatrixSprite const sprite)
{
    byte sprite_width = sprite[kSpriteWidth];
    byte sprite_height = sprite[kSpriteHeight];
    
    /* if height of Sprite = height of Matrix set whole column; the panel leaves out what lies off it */
    if(sprite_height == MAXMATRIX_ROW_NUMBER_OF_MODULE && y == 0) {
        for(byte sprite_column = 0; sprite_column < sprite_width; sprite_column++)
        {
            panel.SetColumn(static_cast<int16_t>(x + sprite_column), sprite[sprite_column + kSpriteColumn1]);
        }
    } else { /* otherwise we have to set every single dot */
        for(byte sprite_column = 0; sprite_column < sprite_width; sprite_column++)
        {
            for(byte sprite_row = 0; sprite_row < sprite_height; sprite_row++)
            {
                panel.SetDot(static_cast<int16_t>(x + sprite_column), static_cast<int16_t>(y + sprite_row), bitRead(sprite[sprite_column + kSpriteColumn1], sprite_row));
            }
        }
    }
}

/*! \brief          set space between chars
 *  \details        this function sets the number of spaces between chars
 *                  
 *  \param[in]      space_between_chars     number of spaces
 */
void MaxMatrixText::SetSpaceBetweenChars(byte space_between_chars)
{
    /* every byte value is a valid number of spaces, so it cannot fail */
    space_between_chars_ = space_between_chars;
}

/*! \brief          shifts char one column to the left
 *  \details        this function has to be called periodically until it returns false
 *  \param[in]      panel       the matrix the text is drawn on
 *  \return         true while the char or the space after it is still shifting
 *  \pre            has to be called from MaxMatrix::ShiftTask() or StringShiftTask()
 */
bool MaxMatrixText::CharShiftTask(MaxMatrixPanel& panel)
{
    panel.ShiftLeft(false, true);
    sprite_shift_counter_++;

    /* if sprite is completed set only spaces */
    if(sprite_shift_counter_ <= sprite_buffer_[kSpriteWidth]) {
        SetSprite(panel, MAXMATRIX_NUMBER_OF_COLUMNS - sprite_shift_counter_, 0, sprite_buffer_);
    }
    /* if sprite and spaces are completed task has finished */
    else if(sprite_shift_counter_ == sprite_buffer_[kSpriteWidth] + space_between_chars_ + 1) {
        sprite_shift_counter_ = kShiftIdle;
        return false;
    }
    return true;
}

/*! \brief          shifts string one column to the left
 *  \details        this function has to be called periodically until it returns false
 *  \param[in]      panel       the matrix the text is drawn on
 *  \return         true while the string is still shifting
 *  \pre            has to be called from MaxMatrix::ShiftTask()
 */
bool MaxMatrixText::StringShiftTask(MaxMatrixPanel& panel)
{
    char c;

    /* has char shift task finished shifting sprite or char */
    if(sprite_shift_counter_ == kShiftIdle)
    {
        /* Unknown chars are skipped within this call: a call spent on one sent nothing, and the text
         * stood still for a step. So read on to the next known char or to the end of the string. */
        while(*string_ != STD_NULL_CHARACTER && sprite_shift_counter_ == kShiftIdle) {
            string_ += MaxMatrixFont::DecodeChar(string_, &c);
            if(MaxMatrixFont::GetCharSprite(c, &sprite_buffer_).IsOk()) sprite_shift_counter_ = kShiftStart;
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
