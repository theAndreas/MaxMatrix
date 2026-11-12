// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings on the matrix, standing or shifting in from the right.
 */

#include "Text.h"

namespace max_matrix {
namespace internal {

Text::Text() {
  string_ = nullptr;
  repeat_from_ = nullptr;
  sprite_shift_counter_ = kShiftIdle;
  space_between_chars_ = MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE;
  font_ = nullptr;
  glyph_.columns = nullptr;
  glyph_.width = 0;
  glyph_.height = 0;
}

Status Text::SetFont(uint8_t const* font, size_t size) {
  Status const status = Font::CheckFont(font, size);

  if (status.IsOk()) font_ = font;
  return status;
}

Status Text::SetChar(Panel& panel, int16_t x, int16_t y, char c) const {
  Glyph glyph;
  Status const status = FindGlyph(c, &glyph);

  if (status.IsOk()) SetGlyph(panel, x, y, glyph);
  return status;
}

Status Text::SetCharWithShift(char c) {
  Status const status = FindGlyph(c, &glyph_);

  if (status.IsOk()) sprite_shift_counter_ = kShiftStart;
  return status;
}

Status Text::SetText(Panel& panel, char const* text) const {
  int16_t char_column = 0;
  char c;

  if (text == nullptr) return Error::kNoText;
  while (*text != internal::kNullCharacter) {
    text += Font::DecodeChar(text, &c);
    Glyph glyph;
    Status const status = FindGlyph(c, &glyph);
    if (!status.IsOk()) return status;
    SetGlyph(panel, char_column, 0, glyph);
    char_column =
        static_cast<int16_t>(char_column + glyph.width + space_between_chars_);
    if (char_column > panel.GetNumberOfColumns()) break;
  }
  return Error::kOk;
}

Status Text::SetTextWithShift(char const* text, bool repeat) {
  if (text == nullptr) return Error::kNoText;
  sprite_shift_counter_ = kShiftIdle;
  string_ = text;
  repeat_from_ = repeat ? text : nullptr;
  return Error::kOk;
}

/*
 * The glyph of `c` in the font that is set, into `glyph`, which stays as it was
 * if the font has none. One place that unpacks the result of
 * Font::GetGlyph() rather than four: 62 bytes less of flash in
 * SerialControl, which draws through all four.
 */
Status Text::FindGlyph(char c, Glyph* glyph) const {
  Result<Glyph> const found = Font::GetGlyph(GetFont(), c);

  if (found.IsOk()) *glyph = found.GetValue();
  return found.GetError();
}

void Text::SetSprite(Panel& panel, int16_t x, int16_t y, Sprite const sprite) {
  // As in Panel::DrawBitmap(): right of the panel x plus a column
  // could overflow.
  if (x >= panel.GetNumberOfColumns()) return;
  for (byte sprite_column = 0; sprite_column < sprite[kSpriteWidth];
       sprite_column++) {
    panel.DrawColumn(static_cast<int16_t>(x + sprite_column), y,
                     sprite[kSpriteHeight],
                     sprite[kSpriteColumn1 + sprite_column]);
  }
}

void Text::SetGlyph(Panel& panel, int16_t x, int16_t y, Glyph const& glyph) {
  panel.DrawBitmap(x, y, glyph.columns, glyph.width, glyph.height);
}

void Text::SetSpaceBetweenChars(byte space_between_chars) {
  /* every byte value is a valid number of spaces, so it cannot fail */
  space_between_chars_ = space_between_chars;
}

bool Text::CharShiftTask(Panel& panel) {
  // An empty column comes in, by the column rather than ShiftFill, so
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
void Text::ReadNextChar() {
  char const* start_over_at = repeat_from_;
  char c;

  while (sprite_shift_counter_ == kShiftIdle) {
    if (*string_ == internal::kNullCharacter) {
      if (start_over_at == nullptr) return;
      string_ = start_over_at;
      start_over_at = nullptr;
    } else {
      string_ += Font::DecodeChar(string_, &c);
      if (FindGlyph(c, &glyph_).IsOk()) sprite_shift_counter_ = kShiftStart;
    }
  }
}

bool Text::StringShiftTask(Panel& panel) {
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

}  // namespace internal
}  // namespace max_matrix
