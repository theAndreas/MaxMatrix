// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings on the matrix, standing or shifting in from the right.
 */

#include "MaxMatrixText.h"

MaxMatrixText::MaxMatrixText() {
  string_ = NULL;
  repeat_from_ = NULL;
  sprite_shift_counter_ = kShiftIdle;
  space_between_chars_ = MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE;
  font_ = NULL;
  glyph_.columns = NULL;
  glyph_.width = 0;
  glyph_.height = 0;
}

MaxMatrixStatus MaxMatrixText::SetFont(uint8_t const* font, size_t size) {
  MaxMatrixStatus const status = MaxMatrixFont::CheckFont(font, size);

  if (status.IsOk()) font_ = font;
  return status;
}

MaxMatrixStatus MaxMatrixText::SetChar(MaxMatrixPanel& panel, int16_t x,
                                       int16_t y, char c) const {
  MaxMatrixGlyph glyph;
  MaxMatrixStatus const status = FindGlyph(c, &glyph);

  if (status.IsOk()) SetGlyph(panel, x, y, glyph);
  return status;
}

MaxMatrixStatus MaxMatrixText::SetCharWithShift(char c) {
  MaxMatrixStatus const status = FindGlyph(c, &glyph_);

  if (status.IsOk()) sprite_shift_counter_ = kShiftStart;
  return status;
}

MaxMatrixStatus MaxMatrixText::SetText(MaxMatrixPanel& panel,
                                       char const* text) const {
  int16_t char_column = 0;
  char c;

  if (text == NULL) return MaxMatrixError::kNoText;
  while (*text != STD_NULL_CHARACTER) {
    text += MaxMatrixFont::DecodeChar(text, &c);
    MaxMatrixGlyph glyph;
    MaxMatrixStatus const status = FindGlyph(c, &glyph);
    if (!status.IsOk()) return status;
    SetGlyph(panel, char_column, 0, glyph);
    char_column =
        static_cast<int16_t>(char_column + glyph.width + space_between_chars_);
    if (char_column > panel.GetNumberOfColumns()) break;
  }
  return MaxMatrixError::kOk;
}

MaxMatrixStatus MaxMatrixText::SetTextWithShift(char* text, bool repeat) {
  if (text == NULL) return MaxMatrixError::kNoText;
  sprite_shift_counter_ = kShiftIdle;
  string_ = text;
  repeat_from_ = repeat ? text : NULL;
  return MaxMatrixError::kOk;
}

/*
 * The glyph of `c` in the font that is set, into `glyph`, which stays as it was
 * if the font has none. One place that unpacks the result of
 * MaxMatrixFont::GetGlyph() rather than four: 62 bytes less of flash in
 * SerialControl, which draws through all four.
 */
MaxMatrixStatus MaxMatrixText::FindGlyph(char c, MaxMatrixGlyph* glyph) const {
  MaxMatrixResult<MaxMatrixGlyph> const found =
      MaxMatrixFont::GetGlyph(GetFont(), c);

  if (found.IsOk()) *glyph = found.GetValue();
  return found.GetError();
}

void MaxMatrixText::SetSprite(MaxMatrixPanel& panel, int16_t x, int16_t y,
                              MaxMatrixSprite const sprite) {
  // As in MaxMatrixPanel::DrawBitmap(): right of the panel x plus a column
  // could overflow.
  if (x >= panel.GetNumberOfColumns()) return;
  for (byte sprite_column = 0; sprite_column < sprite[kSpriteWidth];
       sprite_column++) {
    panel.DrawColumn(static_cast<int16_t>(x + sprite_column), y,
                     sprite[kSpriteHeight],
                     sprite[kSpriteColumn1 + sprite_column]);
  }
}

void MaxMatrixText::SetGlyph(MaxMatrixPanel& panel, int16_t x, int16_t y,
                             MaxMatrixGlyph const& glyph) {
  panel.DrawBitmap(x, y, glyph.columns, glyph.width, glyph.height);
}

void MaxMatrixText::SetSpaceBetweenChars(byte space_between_chars) {
  /* every byte value is a valid number of spaces, so it cannot fail */
  space_between_chars_ = space_between_chars;
}

bool MaxMatrixText::CharShiftTask(MaxMatrixPanel& panel) {
  // An empty column comes in, by the column rather than MaxMatrixShiftFill, so
  // that a sketch that only shifts text does not link the other fills.
  panel.ShiftLeft(static_cast<byte>(0));
  sprite_shift_counter_++;

  /* if sprite is completed set only spaces */
  if (sprite_shift_counter_ <= glyph_.width) {
    SetGlyph(panel,
             static_cast<int16_t>(panel.GetNumberOfColumns() -
                                  sprite_shift_counter_),
             0, glyph_);
  } else if (sprite_shift_counter_ ==
             static_cast<uint16_t>(glyph_.width + space_between_chars_ + 1)) {
    /* if sprite and spaces are completed task has finished */
    sprite_shift_counter_ = kShiftIdle;
    return false;
  }
  return true;
}

/*
 * Reads on to the next char of the text the font has, and starts shifting it
 * in. Unknown chars are skipped within the call that reads them: a call spent
 * on one sent nothing, and the text stood still for a step. At the end of a
 * text that repeats it reads on from its start, in the same call, so that no
 * column comes between its end and its start; but once only, so that a text
 * without a char of the font ends rather than reading itself over and over. At
 * the end of any other text nothing starts.
 */
void MaxMatrixText::ReadNextChar() {
  char* start_over_at = repeat_from_;
  char c;

  while (sprite_shift_counter_ == kShiftIdle) {
    if (*string_ == STD_NULL_CHARACTER) {
      if (start_over_at == NULL) return;
      string_ = start_over_at;
      start_over_at = NULL;
    } else {
      string_ += MaxMatrixFont::DecodeChar(string_, &c);
      if (FindGlyph(c, &glyph_).IsOk()) sprite_shift_counter_ = kShiftStart;
    }
  }
}

bool MaxMatrixText::StringShiftTask(MaxMatrixPanel& panel) {
  /* has char shift task finished shifting sprite or char */
  if (sprite_shift_counter_ == kShiftIdle) {
    ReadNextChar();
    if (sprite_shift_counter_ == kShiftStart) {
      CharShiftTask(panel);
    } else { /* end of string reached, task has finished */
      panel.ShiftLeft(static_cast<byte>(0));
      return false;
    }
  } else {
    /* go on shifting Sprite; the end of a char is not the end of the string */
    CharShiftTask(panel);
  }
  return true;
}
