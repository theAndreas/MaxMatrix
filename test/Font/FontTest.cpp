// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Fonts: the chars beyond ASCII and how a text reaches them, the default
 * font, and a font of a sketch's own. */

#include <avr/pgmspace.h>
#include <binary.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* fonts in the format of MD_MAX72XX for the tests of SetFont(): the digits 0
 * and 1, 7 rows high, with a 2 of width 0 that the font lacks, in version 1 and
 * in version 2; and a char of 8 columns. kNoFont, which the tests of errors
 * use as well, lies in MaxMatrixTestSupport.h */
constexpr auto kDigitFont PROGMEM{make_array<uint8_t>(
    'F', 1, '0', '2', 7, 3, B00111110, B01000001, B00111110,  // 0
    2, B01000010, B01111111,                                  // 1
    0)};  // 2, not in the font
constexpr auto kDigitFontVersion2 PROGMEM{make_array<uint8_t>(
    'F', 2, 0, '0', 0, '1', 7, 3, B00111110, B01000001, B00111110,  // 0
    2, B01000010, B01111111)};                                      // 1
constexpr auto kWideFont PROGMEM{
    make_array<uint8_t>('F', 1, 'W', 'W', 8, 8, 0x01, 0x02, 0x04, 0x08, 0x10,
                        0x20, 0x40, 0x80)};  // W, a diagonal

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class FontTest : public MaxMatrixTest {};

/* the Arduino IDE saves sketches as UTF-8, so an umlaut arrives as two bytes */
TEST_F(FontTest, Utf8TextIsDrawnAsLatin1) {
  ASSERT_EQ(matrix_.SetText("\xC4\xF6\xFC!").GetError(), Error::kOk);
  std::vector<std::string> const latin1{ShownPicture(matrix_)};
  matrix_.Clear();

  ASSERT_EQ(matrix_.SetText("\xC3\x84\xC3\xB6\xC3\xBC!").GetError(),
            Error::kOk);

  EXPECT_EQ(ShownPicture(matrix_), latin1);
}

TEST_F(FontTest, Utf8TextIsShiftedAsLatin1) {
  ASSERT_EQ(matrix_
                .SetTextWithShift("Gr\xFC\xDF"
                                  "e \xC4")
                .GetError(),
            Error::kOk);
  ShiftUntilReady(matrix_);
  std::vector<std::string> const latin1{ShownPicture(matrix_)};
  matrix_.Clear();
  ASSERT_EQ(matrix_
                .SetTextWithShift("Gr\xC3\xBC\xC3\x9F"
                                  "e \xC3\x84")
                .GetError(),
            Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(ShownPicture(matrix_), latin1);
}

/* a char beyond ASCII of the default font, its two bytes of UTF-8 and its
 * latin-1 code */
struct CharBeyondAscii {
  char const* name;
  char const* utf8;
  uint8_t code;
};

constexpr std::array<CharBeyondAscii, 8> kCharsBeyondAscii{{
    {"CapitalAUmlaut", "\xC3\x84", 0xC4},
    {"CapitalOUmlaut", "\xC3\x96", 0xD6},
    {"CapitalUUmlaut", "\xC3\x9C", 0xDC},
    {"SmallAUmlaut", "\xC3\xA4", 0xE4},
    {"SmallOUmlaut", "\xC3\xB6", 0xF6},
    {"SmallUUmlaut", "\xC3\xBC", 0xFC},
    {"SharpS", "\xC3\x9F", 0xDF},
    {"Degree", "\xC2\xB0", 0xB0},
}};

class CharBeyondAsciiTest : public ::testing::TestWithParam<CharBeyondAscii> {};

INSTANTIATE_TEST_SUITE_P(DefaultFont, CharBeyondAsciiTest,
                         ::testing::ValuesIn(kCharsBeyondAscii),
                         CaseName<CharBeyondAscii>);

TEST_P(CharBeyondAsciiTest, DecodesToItsLatin1Code) {
  DecodedChar const decoded{
      Font::DecodeChar(span<char const>{GetParam().utf8, 2})};

  EXPECT_EQ(static_cast<uint8_t>(decoded.c), GetParam().code);
  EXPECT_EQ(decoded.length, 2);
}

TEST_P(CharBeyondAsciiTest, HasAGlyphInTheDefaultFont) {
  Error const error{
      Font::GetGlyph(kDefaultFont, static_cast<char>(GetParam().code))
          .GetError()};

  EXPECT_EQ(error, Error::kOk);
}

/* Bug: the sprite of the U umlaut was a copy of the O, which is 3E 41 41 3E */
TEST_F(FontTest, UUmlautHasAGlyphOfItsOwn) {
  Glyph const u_umlaut{Font::GetGlyph(kDefaultFont, '\xDC').GetValue()};

  EXPECT_EQ(Bytes(u_umlaut.columns),
            (std::vector<uint8_t>{0x3D, 0x40, 0x40, 0x3D}));
}

/* a text of one char beyond ASCII, as UTF-8 or as latin-1, and the columns it
 * draws from the left edge */
struct DrawnChar {
  char const* name;
  char const* text;
  array<uint8_t, 4> columns;
};

/* Bug: the font had no sharp s and no degree sign, so SetText() failed on them
 * and a shifting text lost them without a word; the sharp s is FE 01 49 36,
 * the degree sign 02 05 02 */
constexpr std::array<DrawnChar, 4> kDrawnChars{{
    {"SharpSUtf8", "\xC3\x9F", {{0xFE, 0x01, 0x49, 0x36}}},
    {"SharpSLatin1", "\xDF", {{0xFE, 0x01, 0x49, 0x36}}},
    {"DegreeUtf8", "\xC2\xB0", {{0x02, 0x05, 0x02, 0x00}}},
    {"DegreeLatin1", "\xB0", {{0x02, 0x05, 0x02, 0x00}}},
}};

class DrawnCharTest : public MaxMatrixTest,
                      public ::testing::WithParamInterface<DrawnChar> {};

INSTANTIATE_TEST_SUITE_P(DefaultFont, DrawnCharTest,
                         ::testing::ValuesIn(kDrawnChars), CaseName<DrawnChar>);

/* two chars at most, as the text ends at its null character */
TEST_P(DrawnCharTest, IsDrawnByItsGlyph) {
  ASSERT_EQ(matrix_.SetText(span<char const>{GetParam().text, 2}).GetError(),
            Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 4), Bytes(GetParam().columns));
}

TEST_F(FontTest, DecodesAsciiAsItself) {
  DecodedChar const decoded{Font::DecodeChar("A")};

  EXPECT_EQ(decoded.c, 'A');
  EXPECT_EQ(decoded.length, 1);
}

/* a lead byte of UTF-8 at the end of a text has no continuation to read */
TEST_F(FontTest, TakesALeadByteAtTheEndAsItself) {
  DecodedChar const decoded{Font::DecodeChar("\xC3")};

  EXPECT_EQ(decoded.c, '\xC3');
  EXPECT_EQ(decoded.length, 1);
}

/* the a of "a" and a lead byte, 20 54 54 78, and the space behind it */
TEST_F(FontTest, TextShiftEndsAtALeadByteAtItsEnd) {
  ASSERT_EQ(matrix_.SetTextWithShift("a\xC3").GetError(), Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(matrix_.GetState(), State::kReady);
  EXPECT_EQ(RightEdge(matrix_, 5),
            (std::vector<uint8_t>{0x20, 0x54, 0x54, 0x78, 0x00}));
}

/* "ab" as it ends on the panel: a, the space, b, the space */
std::vector<uint8_t> Ab() {
  return {0x20, 0x54, 0x54, 0x78, 0x00, 0x7F, 0x44, 0x44, 0x38, 0x00};
}

/* Bug: an unknown char was shifted in all the same, with whatever sprite was
 * left over */
TEST_F(FontTest, TextShiftLeavesOutUnknownCharsInTheMiddle) {
  ASSERT_EQ(matrix_
                .SetTextWithShift("a\x01\x02"
                                  "b")
                .GetError(),
            Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 10), Ab());
}

TEST_F(FontTest, TextShiftLeavesOutAnUnknownCharAtTheEnd) {
  ASSERT_EQ(matrix_.SetTextWithShift("ab\x01").GetError(), Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 10), Ab());
}

/* Bug: skipping an unknown char took a ShiftStep() call of its own that sent
 * nothing, and the text stood still for a step; "ab" takes a step for each of
 * its ten columns and one that finds its end */
TEST_F(FontTest, TextShiftSkipsUnknownCharsInTheMiddleWithoutAStep) {
  ASSERT_EQ(matrix_
                .SetTextWithShift("a\x01\x02"
                                  "b")
                .GetError(),
            Error::kOk);

  uint16_t const steps{ShiftUntilReady(matrix_)};

  EXPECT_EQ(steps, 11);
}

TEST_F(FontTest, TextShiftSkipsAnUnknownCharAtTheEndWithoutAStep) {
  ASSERT_EQ(matrix_.SetTextWithShift("ab\x01").GetError(), Error::kOk);

  uint16_t const steps{ShiftUntilReady(matrix_)};

  EXPECT_EQ(steps, 11);
}

TEST_F(FontTest, MatrixStartsWithTheDefaultFont) {
  uint8_t const* const font{matrix_.GetFont().data()};

  EXPECT_EQ(font, kDefaultFont.data());
}

/* the default font was converted from the table of sprites the library had */
TEST_F(FontTest, DefaultFontPassesItsCheck) {
  Status const status{Font::CheckFont(kDefaultFont)};

  EXPECT_TRUE(status.IsOk());
}

std::string CodeName(::testing::TestParamInfo<uint16_t> const& info) {
  return "Code" + std::to_string(info.param);
}

/* every ASCII char from the space to the tilde */
class AsciiCharTest : public ::testing::TestWithParam<uint16_t> {};

INSTANTIATE_TEST_SUITE_P(DefaultFont, AsciiCharTest,
                         ::testing::Range(uint16_t{32}, uint16_t{127}),
                         CodeName);

TEST_P(AsciiCharTest, HasAGlyphEightRowsHigh) {
  Result<Glyph> const glyph{
      Font::GetGlyph(kDefaultFont, static_cast<char>(GetParam()))};

  EXPECT_EQ(glyph.GetError(), Error::kOk);
  EXPECT_EQ(glyph.GetValue().height, 8);
}

/* no char between the tilde and the degree sign */
class CharBetweenTildeAndDegreeTest
    : public ::testing::TestWithParam<uint16_t> {};

INSTANTIATE_TEST_SUITE_P(DefaultFont, CharBetweenTildeAndDegreeTest,
                         ::testing::Range(uint16_t{127}, uint16_t{0xB0}),
                         CodeName);

TEST_P(CharBetweenTildeAndDegreeTest, HasNoGlyph) {
  Error const error{
      Font::GetGlyph(kDefaultFont, static_cast<char>(GetParam())).GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
}

TEST_F(FontTest, CharBelowTheSpaceHasNoGlyph) {
  Error const error{Font::GetGlyph(kDefaultFont, '\x1F').GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
}

TEST_F(FontTest, CharAboveTheLastHasNoGlyph) {
  Error const error{Font::GetGlyph(kDefaultFont, '\xFF').GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
}

TEST_F(FontTest, SetFontMakesItTheFont) {
  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);

  uint8_t const* const font{matrix_.GetFont().data()};

  EXPECT_EQ(font, kDigitFont.data());
}

/* a font of MD_MAX72XX that the sketch sets: its chars in its height, which is
 * not the panel's; "10" is 42 7F, the space, 3E 41 3E, the space */
TEST_F(FontTest, SetFontDrawsInThatFont) {
  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetText("10").GetError(), Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 7),
            (std::vector<uint8_t>{0x42, 0x7F, 0x00, 0x3E, 0x41, 0x3E, 0x00}));
}

/* a char a font lacks, by a width of 0 or beyond its last, fails as one the
 * default font lacks */
TEST_F(FontTest, CharOfWidthZeroIsUnknown) {
  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);

  Error const error{matrix_.SetText("2").GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
}

TEST_F(FontTest, CharBeyondTheLastIsUnknown) {
  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);

  Error const error{matrix_.SetText("A").GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
}

TEST_F(FontTest, DefaultFontSetAgainDrawsItsChars) {
  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetFont(kDefaultFont).GetError(), Error::kOk);

  Error const error{matrix_.SetText("A").GetError()};

  EXPECT_EQ(error, Error::kOk);
}

/* version 2 gives first and last char in two bytes each, so its chars start two
 * bytes later: "01" is 3E 41 3E, the space, 42 7F */
TEST_F(FontTest, SetFontTakesVersion2) {
  ASSERT_EQ(matrix_.SetFont(kDigitFontVersion2).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetText("01").GetError(), Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 6),
            (std::vector<uint8_t>{0x3E, 0x41, 0x3E, 0x00, 0x42, 0x7F}));
}

/* a sprite has five columns at most; a char of a font may have more, as the
 * diagonal W of eight */
TEST_F(FontTest, CharWiderThanASpriteStandsWhole) {
  ASSERT_EQ(matrix_.SetFont(kWideFont).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetChar(Point{X{0}, Y{0}}, 'W').GetError(), Error::kOk);

  EXPECT_EQ(
      LeftEdge(matrix_, 8),
      (std::vector<uint8_t>{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}));
}

/* the W and the space behind it */
TEST_F(FontTest, CharWiderThanASpriteShiftsInWhole) {
  ASSERT_EQ(matrix_.SetFont(kWideFont).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetCharWithShift('W').GetError(), Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 9),
            (std::vector<uint8_t>{0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40,
                                  0x80, 0x00}));
}

/* a step for each of its eight columns and the space */
TEST_F(FontTest, CharWiderThanASpriteTakesAStepPerColumn) {
  ASSERT_EQ(matrix_.SetFont(kWideFont).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetCharWithShift('W').GetError(), Error::kOk);

  uint16_t const steps{ShiftUntilReady(matrix_)};

  EXPECT_EQ(steps, 8 + 1);
}

/* what SetFont() refuses, and why */
struct BadFont {
  char const* name;
  uint8_t const* font;
  size_t size;
};

constexpr auto kVersion3 PROGMEM{make_array<uint8_t>('F', 3, 32, 32, 8, 1, 0)};
constexpr auto kNoHeight PROGMEM{make_array<uint8_t>('F', 1, 32, 32, 0, 1, 0)};
constexpr auto kTooHigh PROGMEM{make_array<uint8_t>('F', 1, 32, 32, 9, 1, 0)};
constexpr auto kLastBeforeFirst PROGMEM{
    make_array<uint8_t>('F', 1, 33, 32, 8, 1, 0)};
/* Bug: only the header of a font was checked, as the format holds no length, so
 * a font whose header promised more chars than its array held was taken, and a
 * char beyond its end was read from whatever lay behind it in flash. Now the
 * chars have to fill the array exactly, from a header that fits into it. */
constexpr auto kLastBeyondTheEnd PROGMEM{
    make_array<uint8_t>('F', 1, '0', '1', 7, 1, 0x7F)};
constexpr auto kByteAfterTheLast PROGMEM{
    make_array<uint8_t>('F', 1, '0', '0', 7, 1, 0x7F, 0)};
constexpr auto kWidthBeyondTheEnd PROGMEM{
    make_array<uint8_t>('F', 1, '0', '0', 7, 3, 0x7F)};
constexpr auto kShortHeader PROGMEM{make_array<uint8_t>('F', 1, '0', '0')};
constexpr auto kShortHeaderVersion2 PROGMEM{
    make_array<uint8_t>('F', 2, 0, '0', 0, '0')};

constexpr std::array<BadFont, 12> kBadFonts{{
    {"Nothing", nullptr, 0},
    {"NoFont", kNoFont.data(), kNoFont.size()},
    {"Version3", kVersion3.data(), kVersion3.size()},
    {"NoHeight", kNoHeight.data(), kNoHeight.size()},
    {"TooHigh", kTooHigh.data(), kTooHigh.size()},
    {"LastBeforeFirst", kLastBeforeFirst.data(), kLastBeforeFirst.size()},
    {"LastBeyondTheEnd", kLastBeyondTheEnd.data(), kLastBeyondTheEnd.size()},
    {"ByteAfterTheLast", kByteAfterTheLast.data(), kByteAfterTheLast.size()},
    {"WidthBeyondTheEnd", kWidthBeyondTheEnd.data(), kWidthBeyondTheEnd.size()},
    {"ShortHeader", kShortHeader.data(), kShortHeader.size()},
    {"ShortHeaderVersion2", kShortHeaderVersion2.data(),
     kShortHeaderVersion2.size()},
    {"ArrayOneShort", kDigitFont.data(), kDigitFont.size() - 1},
}};

/* only what starts as a font and fills its array is taken */
class BadFontTest : public MaxMatrixTest,
                    public ::testing::WithParamInterface<BadFont> {
 protected:
  void SetUp() override {
    MaxMatrixTest::SetUp();
    ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  }
};

INSTANTIATE_TEST_SUITE_P(SetFont, BadFontTest, ::testing::ValuesIn(kBadFonts),
                         CaseName<BadFont>);

TEST_P(BadFontTest, IsRefused) {
  Error const error{
      matrix_.SetFont({GetParam().font, GetParam().size}).GetError()};

  EXPECT_EQ(error, Error::kNotAFont);
}

TEST_P(BadFontTest, LeavesTheFontSetBefore) {
  matrix_.SetFont({GetParam().font, GetParam().size});

  EXPECT_EQ(matrix_.GetFont().data(), kDigitFont.data());
}

/* the same char as kLastBeyondTheEnd, filling its array */
TEST_F(FontTest, SetFontTakesAFontThatFillsItsArray) {
  static constexpr auto kOneChar PROGMEM{
      make_array<uint8_t>('F', 1, '0', '0', 7, 1, 0x7F)};

  Error const error{matrix_.SetFont(kOneChar).GetError()};

  EXPECT_EQ(error, Error::kOk);
}

TEST_F(FontTest, SetFontTakesAFontByItsAddressAndLength) {
  Error const error{
      matrix_.SetFont({kDigitFontVersion2.data(), kDigitFontVersion2.size()})
          .GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(matrix_.GetFont().data(), kDigitFontVersion2.data());
}

constexpr char kHi[] = "Hi";

TEST_F(FontTest, EndOfTextIsFoundWhenCompiling) {
  static_assert(!internal::IsEndOfText(kHi, 1), "within the text");
  static_assert(internal::IsEndOfText(kHi, 2), "at its null character");
  static_assert(internal::IsEndOfText(span<char const>{kHi, 1}, 1),
                "at the end of its span");

  EXPECT_TRUE(internal::IsEndOfText(kHi, 2));
}

}  // namespace
}  // namespace max_matrix
