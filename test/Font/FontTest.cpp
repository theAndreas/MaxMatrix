// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Fonts: the chars beyond ASCII and how a text reaches them, the default
 * font, and a font of a sketch's own. */

#include <algorithm>
#include <array>
#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::DecodedChar;
using max_matrix::Error;
using max_matrix::Font;
using max_matrix::Glyph;
using max_matrix::kDefaultFont;
using max_matrix::Result;
using max_matrix::State;

/* fonts in the format of MD_MAX72XX for the tests of SetFont(): the digits 0
 * and 1, 7 rows high, with a 2 of width 0 that the font lacks, in version 1 and
 * in version 2; and a char of 8 columns. kNoFont, which the tests of errors
 * use as well, lies in MaxMatrixTestSupport.h */
// NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
static uint8_t const kDigitFont[] PROGMEM{
    'F', 1,         '0',       '2', 7, 3, B00111110, B01000001, B00111110,  // 0
    2,   B01000010, B01111111,                                              // 1
    0,  // 2, not in the font
};
// NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
static uint8_t const kDigitFontVersion2[] PROGMEM{
    'F', 2,         0,         '0',       0,         '1',
    7,   3,         B00111110, B01000001, B00111110,  // 0
    2,   B01000010, B01111111,                        // 1
};
// NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
static uint8_t const kWideFont[] PROGMEM{
    'F',  1,    'W',  'W',  8,    8,    0x01,
    0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,  // W, a diagonal
};
/* the Arduino IDE saves sketches as UTF-8, so an umlaut arrives as two bytes */
TEST_F(MaxMatrixTest, UmlautsFromUtf8AndLatin1Match) {
  std::vector<uint8_t> utf8;
  std::vector<uint8_t> latin1;

  matrix_.Clear();
  EXPECT_EQ(matrix_.SetText("\xC3\x84\xC3\xB6\xC3\xBC!").GetError(),
            Error::kOk);
  utf8 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  matrix_.Clear();
  EXPECT_EQ(matrix_.SetText("\xC4\xF6\xFC!").GetError(), Error::kOk);
  latin1 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  EXPECT_EQ(utf8, latin1);

  char shift_utf8[] =
      "Gr\xC3\xBC\xC3\x9F"
      "e \xC3\x84";
  char shift_latin1[] =
      "Gr\xFC\xDF"
      "e \xC4";
  ShiftWholeText(shift_utf8);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  utf8 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  ShiftWholeText(shift_latin1);
  latin1 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  EXPECT_EQ(utf8, latin1);
}

/* the chars beyond ASCII lie at their latin-1 codes in the default font, each
 * its own */
TEST_F(MaxMatrixTest, EveryCharBeyondAsciiHasItsOwnGlyph) {
  std::array<char const*, 8> const chars{{"\xC3\x84", "\xC3\x96", "\xC3\x9C",
                                          "\xC3\xA4", "\xC3\xB6", "\xC3\xBC",
                                          "\xC3\x9F", "\xC2\xB0"}};
  std::array<uint8_t, 8> const codes{
      {0xC4, 0xD6, 0xDC, 0xE4, 0xF6, 0xFC, 0xDF, 0xB0}};

  for (uint8_t i{0}; i < 8; i++) {
    SCOPED_TRACE(chars[i]);
    DecodedChar const decoded{Font::DecodeChar(chars[i])};
    EXPECT_EQ(decoded.length, 2);
    EXPECT_EQ(static_cast<uint8_t>(decoded.c), codes[i]);
    EXPECT_EQ(Font::GetGlyph(kDefaultFont, decoded.c).GetError(), Error::kOk);
  }

  /* the sprite of the U umlaut used to be a copy of the O */
  Glyph const ue{Font::GetGlyph(kDefaultFont, '\xDC').GetValue()};
  Glyph const o{Font::GetGlyph(kDefaultFont, 'O').GetValue()};
  EXPECT_NE(Bytes(ue.columns, ue.width), Bytes(o.columns, o.width));
}

/* Bug: the font had no sharp s and no degree sign, so SetText() failed on
 * them and a shifting text lost them without a word */
TEST_F(MaxMatrixTest, EszettAndDegreeAreDrawn) {
  std::array<char const*, 4> const texts{
      {"\xC3\x9F", "\xC2\xB0", "\xDF", "\xB0"}};

  for (char const* text : texts) {
    SCOPED_TRACE(text);
    matrix_.Clear();
    EXPECT_EQ(matrix_.SetText(text).GetError(), Error::kOk);
    std::vector<uint8_t> const shown = Display();
    EXPECT_TRUE(std::any_of(shown.begin(), shown.end(),
                            [](uint8_t column) { return column != 0; }));
  }
}

TEST_F(MaxMatrixTest, DecodeStopsAtEndOfString) {
  EXPECT_EQ(Font::DecodeChar("A").length, 1);
  EXPECT_EQ(Font::DecodeChar("A").c, 'A');
  EXPECT_EQ(Font::DecodeChar("\xC3").length, 1);

  char truncated[] = "a\xC3";
  ShiftWholeText(truncated);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* an unknown char used to be shifted in all the same, with whatever sprite was
 * left over; that the shift finishes does not show it, so what went to the
 * chain is compared with the text without it. Skipping it then took a
 * ShiftTask() call of its own that sent nothing, and the text stood still for
 * a step, so the calls are compared as well - in the middle of the text and at
 * its end. */
TEST_F(MaxMatrixTest, UnknownChars) {
  EXPECT_EQ(matrix_.SetText("a\x01").GetError(), Error::kUnknownChar);

  char known[] = "ab";
  int32_t const frames{ShiftWholeText(known)};
  uint16_t const calls{shift_calls_};

  char in_the_middle[] =
      "a\x01\x02"
      "b";
  EXPECT_EQ(ShiftWholeText(in_the_middle), frames);
  EXPECT_EQ(shift_calls_, calls);
  EXPECT_EQ(matrix_.GetState(), State::kReady);

  char at_the_end[] = "ab\x01";
  EXPECT_EQ(ShiftWholeText(at_the_end), frames);
  EXPECT_EQ(shift_calls_, calls);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}
/* the default font, converted from the table of sprites the library had: every
 * ASCII char from the space to the tilde, no char between the tilde and the
 * degree sign, and 8 rows high */
TEST_F(MaxMatrixTest, DefaultFontHasAsciiAndNothingBetween) {
  EXPECT_EQ(matrix_.GetFont(), kDefaultFont);
  EXPECT_TRUE(Font::CheckFont(kDefaultFont, sizeof(kDefaultFont)).IsOk());
  for (uint16_t code{32}; code <= 126; code++) {
    Result<Glyph> const glyph{
        Font::GetGlyph(kDefaultFont, static_cast<char>(code))};
    EXPECT_EQ(glyph.GetError(), Error::kOk) << code;
    EXPECT_EQ(glyph.GetValue().height, 8) << code;
  }
  for (uint16_t code{127}; code < 0xB0; code++) {
    EXPECT_EQ(Font::GetGlyph(kDefaultFont, static_cast<char>(code)).GetError(),
              Error::kUnknownChar)
        << code;
  }
  EXPECT_EQ(Font::GetGlyph(kDefaultFont, '\x1F').GetError(),
            Error::kUnknownChar);
  EXPECT_EQ(Font::GetGlyph(kDefaultFont, '\xFF').GetError(),
            Error::kUnknownChar);
}

/* a font of MD_MAX72XX that the sketch sets: its chars in its height, which is
 * not the panel's, so dot by dot; a char it lacks, by a width of 0 or beyond
 * its last, fails as one the default font lacks */
TEST_F(MaxMatrixTest, SetFontDrawsInThatFont) {
  std::array<uint8_t, 7> const expected{
      {0x42, 0x7F, 0x00, 0x3E, 0x41, 0x3E, 0x00}};

  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.GetFont(), kDigitFont);
  ASSERT_EQ(matrix_.SetText("10").GetError(), Error::kOk);
  for (size_t column{0}; column < expected.size(); column++) {
    EXPECT_EQ(
        ToInteger(matrix_.GetColumn(static_cast<int16_t>(column)).GetValue()),
        expected[column])
        << "column " << column;
  }
  EXPECT_EQ(matrix_.SetText("2").GetError(), Error::kUnknownChar);
  EXPECT_EQ(matrix_.SetText("A").GetError(), Error::kUnknownChar);

  ASSERT_EQ(matrix_.SetFont(kDefaultFont).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.SetText("A").GetError(), Error::kOk);
}

/* version 2 gives first and last char in two bytes each, so its chars start two
 * bytes later */
TEST_F(MaxMatrixTest, SetFontTakesVersion2) {
  std::array<uint8_t, 6> const expected{{0x3E, 0x41, 0x3E, 0x00, 0x42, 0x7F}};

  ASSERT_EQ(matrix_.SetFont(kDigitFontVersion2).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetText("01").GetError(), Error::kOk);
  for (size_t column{0}; column < expected.size(); column++) {
    EXPECT_EQ(
        ToInteger(matrix_.GetColumn(static_cast<int16_t>(column)).GetValue()),
        expected[column])
        << "column " << column;
  }
}

/* a sprite has five columns at most; a char of a font may have more, standing
 * and shifting in */
TEST_F(MaxMatrixTest, CharWiderThanASpriteIsDrawnWhole) {
  uint16_t steps{0};

  ASSERT_EQ(matrix_.SetFont(kWideFont).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetChar(0, 0, 'W').GetError(), Error::kOk);
  for (uint8_t column{0}; column < 8; column++) {
    EXPECT_EQ(ToInteger(matrix_.GetColumn(column).GetValue()), 1U << column)
        << "standing, column " << static_cast<uint16_t>(column);
  }

  matrix_.Clear();
  ASSERT_EQ(matrix_.SetCharWithShift('W').GetError(), Error::kOk);
  for (; steps < 100 && matrix_.GetState() != State::kReady; steps++) {
    matrix_.ShiftTask();
  }
  EXPECT_EQ(steps, 8 + MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE);
  for (uint8_t column{0}; column < 8; column++) {
    EXPECT_EQ(
        ToInteger(matrix_.GetColumn(kNumberOfColumns - 10 + column).GetValue()),
        1U << column)
        << "shifted, column " << static_cast<uint16_t>(column);
  }
}

/* only what starts as a font is taken, and the font set before stays */
TEST_F(MaxMatrixTest, SetFontRefusesWhatIsNoFont) {
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kVersion3[] PROGMEM{'F', 3, 32, 32, 8, 1, 0};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kNoHeight[] PROGMEM{'F', 1, 32, 32, 0, 1, 0};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kTooHigh[] PROGMEM{'F', 1, 32, 32, 9, 1, 0};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kLastBeforeFirst[] PROGMEM{'F', 1, 33, 32, 8, 1, 0};
  struct BadFont {
    uint8_t const* font;
    size_t size;
  };
  std::array<BadFont, 6> const fonts{{
      {nullptr, 0},
      {kNoFont, sizeof(kNoFont)},
      {kVersion3, sizeof(kVersion3)},
      {kNoHeight, sizeof(kNoHeight)},
      {kTooHigh, sizeof(kTooHigh)},
      {kLastBeforeFirst, sizeof(kLastBeforeFirst)},
  }};

  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  for (auto const& font : fonts) {
    EXPECT_EQ(matrix_.SetFont(font.font, font.size).GetError(),
              Error::kNotAFont)
        << static_cast<void const*>(font.font);
    EXPECT_EQ(matrix_.GetFont(), kDigitFont);
  }
}

/* Bug: only the header of a font was checked, as the format holds no length, so
 * a font whose header promised more chars than its array held was taken, and a
 * char beyond its end was read from whatever lay behind it in flash. Now the
 * chars have to fill the array exactly, from a header that fits into it. */
TEST_F(MaxMatrixTest, SetFontRefusesAFontThatDoesNotFillItsArray) {
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kLastBeyondTheEnd[] PROGMEM{'F', 1, '0', '1',
                                                   7,   1, 0x7F};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kByteAfterTheLast[] PROGMEM{'F', 1, '0',  '0',
                                                   7,   1, 0x7F, 0};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kWidthBeyondTheEnd[] PROGMEM{'F', 1, '0', '0',
                                                    7,   3, 0x7F};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kShortHeader[] PROGMEM{'F', 1, '0', '0'};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kShortHeaderVersion2[] PROGMEM{'F', 2, 0, '0', 0, '0'};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
  static uint8_t const kOneChar[] PROGMEM{'F', 1, '0', '0', 7, 1, 0x7F};

  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.SetFont(kLastBeyondTheEnd).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kByteAfterTheLast).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kWidthBeyondTheEnd).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kShortHeader).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kShortHeaderVersion2).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kDigitFont, sizeof(kDigitFont) - 1).GetError(),
            Error::kNotAFont);
  EXPECT_EQ(matrix_.GetFont(), kDigitFont);

  /* the same chars, filling the array, are taken, by the array and by its
   * address and length */
  EXPECT_EQ(matrix_.SetFont(kOneChar).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.SetFont(kDigitFontVersion2, sizeof(kDigitFontVersion2))
                .GetError(),
            Error::kOk);
  EXPECT_EQ(matrix_.GetFont(), kDigitFontVersion2);
}
