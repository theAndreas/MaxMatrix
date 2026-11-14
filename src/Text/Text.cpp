// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings on the matrix, standing or shifting in from the right.
 */

#include "Text.h"

#include "Utility/Byte.h"

namespace max_matrix {
namespace internal {

Text::Text() = default;

/*
 * In the .cpp rather than inline in the header: avr-gcc 7.3 crashes on the
 * header alone, as tools/self_contained_headers.py compiles it, with -Os,
 * -Wnonnull and -fsyntax-only, when the span of the default font is made
 * inline there.
 */
span<uint8_t const> Text::DefaultFont() {
  return span<uint8_t const>{kDefaultFont};
}

Status Text::SetFont(span<uint8_t const> font) {
  Status const status{Font::CheckFont(font)};

  if (status.IsOk()) {
    font_ = font;
  }
  return status;
}

Status Text::SetChar(Panel& panel, int16_t x, int16_t y, char c) const {
  Result<Glyph> const found{Font::GetGlyph(GetFont(), c)};

  if (found.IsOk()) {
    SetGlyph(panel, x, y, found.GetValue());
  }
  return found.GetError();
}

// The glyph that shifts in stays as it was for a char the font lacks.
Status Text::SetCharWithShift(char c) {
  Result<Glyph> const found{Font::GetGlyph(GetFont(), c)};

  if (found.IsOk()) {
    glyph_ = found.GetValue();
    sprite_shift_counter_ = kShiftStart;
  }
  return found.GetError();
}

Status Text::SetText(Panel& panel, span<char const> text) const {
  int16_t char_column{0};
  size_t position{0};

  if (text.size() == 0) {
    return Error::kNoText;
  }
  while (!IsEndOfText(text, position)) {
    DecodedChar const decoded{Font::DecodeChar(text.subspan(position))};
    position += decoded.length;
    Result<Glyph> const found{Font::GetGlyph(GetFont(), decoded.c)};
    if (!found.IsOk()) {
      return found.GetError();
    }
    Glyph const glyph{found.GetValue()};
    SetGlyph(panel, char_column, 0, glyph);
    auto const width{static_cast<int16_t>(glyph.columns.size())};
    char_column =
        static_cast<int16_t>(char_column + width + space_between_chars_);
    if (char_column > panel.GetNumberOfColumns()) {
      break;
    }
  }
  return Error::kOk;
}

Status Text::SetTextWithShift(span<char const> text, bool repeat) {
  if (text.size() == 0) {
    return Error::kNoText;
  }
  sprite_shift_counter_ = kShiftIdle;
  text_ = text;
  position_ = 0;
  repeat_ = repeat;
  return Error::kOk;
}

void Text::SetSprite(Panel& panel, int16_t x, int16_t y, Sprite const& sprite) {
  // As in Panel::DrawBitmap(): right of the panel x plus a column
  // could overflow.
  if (x >= panel.GetNumberOfColumns()) {
    return;
  }
  for (uint8_t sprite_column{0}; sprite_column < sprite.width;
       sprite_column++) {
    panel.DrawColumn(static_cast<int16_t>(x + sprite_column), y, sprite.height,
                     Byte{sprite.columns[sprite_column]});
  }
}

void Text::SetGlyph(Panel& panel, int16_t x, int16_t y, Glyph const& glyph) {
  panel.DrawBitmap(x, y, glyph.height, glyph.columns);
}

void Text::SetSpaceBetweenChars(uint8_t space_between_chars) {
  /* every byte value is a valid number of spaces, so it cannot fail */
  space_between_chars_ = space_between_chars;
}

bool Text::CharShiftTask(Panel& panel) {
  // An empty column comes in, by the column rather than ShiftFill, so
  // that a sketch that only shifts text does not link the other fills.
  panel.ShiftLeft(Byte{});
  sprite_shift_counter_++;

  /* if sprite is completed set only spaces */
  if (sprite_shift_counter_ <= glyph_.columns.size()) {
    SetGlyph(panel,
             static_cast<int16_t>(panel.GetNumberOfColumns() -
                                  sprite_shift_counter_),
             0, glyph_);
  } else if (sprite_shift_counter_ ==
             static_cast<uint16_t>(glyph_.columns.size() +
                                   space_between_chars_ + 1)) {
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
  bool may_start_over{repeat_};

  while (sprite_shift_counter_ == kShiftIdle) {
    if (IsEndOfText(text_, position_)) {
      if (!may_start_over) {
        return;
      }
      position_ = 0;
      may_start_over = false;
    } else {
      DecodedChar const decoded{Font::DecodeChar(text_.subspan(position_))};
      position_ += decoded.length;
      // A char the font lacks leaves the counter idle, and the loop reads on.
      Discard(SetCharWithShift(decoded.c));
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
      panel.ShiftLeft(Byte{});
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
