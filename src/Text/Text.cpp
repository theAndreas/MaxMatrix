// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Text on the matrix.
 *
 * Chars and strings on the matrix, standing or shifting in from the right.
 */

#include "Text.h"

#include <stddef.h>
#include <stdint.h>

#include "Font/Font.h"
#include "Matrix/Panel.h"
#include "MaxMatrixAttributes.h"
#include "MaxMatrixError.h"
#include "MaxMatrixPgmspace.h"
#include "MaxMatrixStandardTypes.h"
#include "Utility/Byte.h"
#include "Utility/array.h"
#include "Utility/span.h"

namespace max_matrix {
namespace internal {

namespace {

// How far the span of a text in flash reaches, see ToFlashText(): the
// greatest index of a text on the Uno, whose size_t has 16 bits.
constexpr size_t kFlashTextLength{0xFFFFu};

}  // namespace

Text::Text() = default;

Status Text::SetFont(span<uint8_t const> font) {
  Status const status{Font::CheckFont(font)};

  if (status.IsOk()) {
    font_ = font;
  }
  return status;
}

Status Text::SetChar(Panel& panel, Point top_left, char c) const {
  Result<Glyph> const found{Font::GetGlyph(GetFont(), c)};

  if (found.IsOk()) {
    SetGlyph(panel, top_left, found.GetValue());
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
  if (text.size() == 0) {
    return Error::kNoText;
  }
  return DrawText(panel, text, false);
}

// A null pointer is no text, as an empty span; `{}` is one, as a sketch may
// write for an empty text. F("") is a text of no char, as "" is in RAM, which
// draws nothing.
Status Text::SetText(Panel& panel, __FlashStringHelper const* text) const {
  if (text == nullptr) {
    return Error::kNoText;
  }
  return DrawText(panel, ToFlashText(text), true);
}

Status Text::DrawText(Panel& panel, span<char const> text,
                      bool in_flash) const {
  int16_t char_column{0};
  size_t position{0};
  array<char, 2> buffer{};
  span<char const> chars{ReadChars(text, position, in_flash, buffer)};

  while (!IsEndOfText(chars, 0)) {
    DecodedChar const decoded{Font::DecodeChar(chars)};
    position += decoded.length;
    Result<Glyph> const found{Font::GetGlyph(GetFont(), decoded.c)};
    if (!found.IsOk()) {
      return found.GetError();
    }
    Glyph const glyph{found.GetValue()};
    SetGlyph(panel, Point{X{char_column}, Y{0}}, glyph);
    auto const width{static_cast<int16_t>(glyph.columns.size())};
    char_column =
        static_cast<int16_t>(char_column + width + space_between_chars_);
    if (char_column > panel.GetNumberOfColumns()) {
      break;
    }
    chars = ReadChars(text, position, in_flash, buffer);
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
  text_in_flash_ = false;
  return Error::kOk;
}

// As SetText(), a null pointer is no text.
Status Text::SetTextWithShift(__FlashStringHelper const* text, bool repeat) {
  if (text == nullptr) {
    return Error::kNoText;
  }
  sprite_shift_counter_ = kShiftIdle;
  text_ = ToFlashText(text);
  position_ = 0;
  repeat_ = repeat;
  text_in_flash_ = true;
  return Error::kOk;
}

/*
 * A text in flash as a span, which ends at its null character alone: its
 * length is not known without reading it all, so the span reaches as far as an
 * index of a text can, and only ReadChars() reads it.
 */
span<char const> Text::ToFlashText(__FlashStringHelper const* text) {
  // AVR-1: F() of the core makes a text in flash this type; it is a char
  // array, as Print reads it
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  return span<char const>{reinterpret_cast<char const*>(text),
                          kFlashTextLength};
}

/*
 * The chars of a text from `position` on, as DecodeChar() reads them: a text
 * in RAM as it is; from flash its next char, the byte, or the two of a UTF-8
 * sequence, copied into `buffer`. The second byte is read only behind a first
 * that is not the null character, so never past the end of the text.
 */
span<char const> Text::ReadChars(span<char const> text, size_t position,
                                 bool in_flash, array<char, 2>& buffer) {
  span<char const> const rest{text.subspan(position)};
  if (!in_flash) {
    return rest;
  }
  buffer[0] = static_cast<char>(pgm_read_byte(rest.data()));
  size_t count{1u};
  if (buffer[0] != kNullCharacter) {
    buffer[1] = static_cast<char>(pgm_read_byte(&rest[1]));
    count = 2u;
  }
  return span<char const>{buffer.data(), count};
}

void Text::SetSpaceBetweenChars(uint8_t space_between_chars) {
  /* every byte value is a valid number of spaces, so it cannot fail */
  space_between_chars_ = space_between_chars;
}

void Text::SetSprite(Panel& panel, Point top_left, Sprite const& sprite) {
  int16_t const x{top_left.x.number};
  int16_t const y{top_left.y.number};

  // As in Panel::DrawBitmap(): right of the panel x plus a column
  // could overflow.
  if (x >= panel.GetNumberOfColumns()) {
    return;
  }
  // A width beyond its columns read past them.
  uint8_t const width{(sprite.width > kSpriteMaxWidth) ? kSpriteMaxWidth
                                                       : sprite.width};
  for (uint8_t sprite_column{0}; sprite_column < width; sprite_column++) {
    X const column{static_cast<int16_t>(x + sprite_column)};
    panel.DrawColumn(Point{column, Y{y}}, sprite.height,
                     Byte{sprite.columns[sprite_column]});
  }
}

void Text::SetGlyph(Panel& panel, Point top_left, Glyph const& glyph) {
  panel.DrawBitmap(top_left, glyph.height, glyph.columns);
}

bool Text::CharShiftStep(Panel& panel) {
  // An empty column comes in, by the column rather than ShiftFill, so
  // that a sketch that only shifts text does not link the other fills.
  panel.ShiftLeft(Byte{});
  uint16_t const step{sprite_shift_counter_};
  auto const width{static_cast<uint16_t>(glyph_.columns.size())};

  if (step <= width) {
    X const left{static_cast<int16_t>(panel.GetNumberOfColumns() - step)};
    SetGlyph(panel, Point{left, Y{0}}, glyph_);
  }
  // Done with the step that brings the last column of its space: one more
  // brought an empty column no char asked for.
  auto const last_step{static_cast<uint16_t>(width + space_between_chars_)};
  bool const going_on{step < last_step};
  sprite_shift_counter_ =
      going_on ? static_cast<uint16_t>(step + 1u) : kShiftIdle;
  return going_on;
}

/*
 * The next char is read once the one before has come in, in the call after the
 * step that brought its space; at the end of the text that call shifts
 * nothing. Read in the step that ends a char instead, the text ended without
 * that call, but avr-gcc kept ReadNextChar() out of line for its second caller,
 * 46 to 64 bytes of flash in the examples that shift text.
 */
bool Text::StringShiftStep(Panel& panel) {
  if (sprite_shift_counter_ == kShiftIdle) {
    ReadNextChar();
  }
  bool const going_on{sprite_shift_counter_ != kShiftIdle};
  if (going_on) {
    // Whether the char has come in shows in the counter, which the next
    // call reads.
    Discard(CharShiftStep(panel));
  }
  return going_on;
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
    array<char, 2> buffer{};
    span<char const> const chars{
        ReadChars(text_, position_, text_in_flash_, buffer)};
    if (IsEndOfText(chars, 0)) {
      if (!may_start_over) {
        return;
      }
      position_ = 0;
      may_start_over = false;
    } else {
      DecodedChar const decoded{Font::DecodeChar(chars)};
      position_ += decoded.length;
      // A char the font lacks leaves the counter idle, and the loop reads on.
      Discard(SetCharWithShift(decoded.c));
    }
  }
}

/*
 * In the .cpp rather than inline in the header: avr-gcc 7.3 crashes on the
 * header alone, as tools/self_contained_headers.py compiles it, with -Os,
 * -Wnonnull and -fsyntax-only, when the span of the default font is made
 * inline there.
 */
span<uint8_t const> Text::DefaultFont() {
  return span<uint8_t const>{kDefaultFont};
}

}  // namespace internal
}  // namespace max_matrix
