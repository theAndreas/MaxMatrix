// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Text and sprites: drawn, shifting in to its end, over again, and held. */

#include <avr/pgmspace.h>
#include <gtest/gtest.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Arduino.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class TextTest : public MaxMatrixTest {};

/* the whole panel dark */
std::vector<std::string> Dark() {
  return Picture([](int16_t, int16_t) { return false; });
}

/* A text in flash, as F() and FromFlash() give one, draws and shifts as the
 * same text in RAM (OIL 44): the tests compare the two, as the picture of each
 * is pinned by the tests of the text in RAM. */

TEST_F(TextTest, TextInFlashDrawsAsInRam) {
  ASSERT_EQ(matrix_.SetText("Hi!").GetError(), Error::kOk);
  std::vector<uint8_t> const in_ram{LeftEdge(matrix_, 12)};
  matrix_.Clear();

  ASSERT_EQ(matrix_.SetText(F("Hi!")).GetError(), Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 12), in_ram);
}

/* "\xC3\x84", A umlaut in UTF-8, is one char, read from flash a byte at a
 * time */
TEST_F(TextTest, TextInFlashDecodesUtf8AsInRam) {
  ASSERT_EQ(matrix_.SetText("\xC3\x84").GetError(), Error::kOk);
  std::vector<uint8_t> const in_ram{LeftEdge(matrix_, 6)};
  matrix_.Clear();

  ASSERT_EQ(matrix_.SetText(F("\xC3\x84")).GetError(), Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 6), in_ram);
}

TEST_F(TextTest, TextInFlashWithAnUnknownCharDrawsTheCharsBeforeIt) {
  ASSERT_EQ(matrix_.SetText("I").GetError(), Error::kOk);
  std::vector<uint8_t> const in_ram{LeftEdge(matrix_, 6)};
  matrix_.Clear();

  Error const error{matrix_.SetText(F("I\x01")).GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
  EXPECT_EQ(LeftEdge(matrix_, 6), in_ram);
}

TEST_F(TextTest, TextShiftInFlashShiftsAsInRam) {
  ASSERT_EQ(matrix_.SetTextWithShift("H\xC3\x84llo").GetError(), Error::kOk);
  uint16_t const steps_in_ram{ShiftUntilReady(matrix_)};
  std::vector<uint8_t> const in_ram{RightEdge(matrix_, 24)};
  matrix_.Clear();

  ASSERT_EQ(matrix_.SetTextWithShift(F("H\xC3\x84llo")).GetError(), Error::kOk);
  uint16_t const steps_in_flash{ShiftUntilReady(matrix_)};

  EXPECT_EQ(steps_in_flash, steps_in_ram);
  EXPECT_EQ(RightEdge(matrix_, 24), in_ram);
}

TEST_F(TextTest, RepeatingTextInFlashStartsOverAsInRam) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi", true).GetError(), Error::kOk);
  ShiftSteps(matrix_, 18);
  std::vector<uint8_t> const in_ram{RightEdge(matrix_, 18)};
  matrix_.Clear();

  ASSERT_EQ(matrix_.SetTextWithShift(F("Hi"), true).GetError(), Error::kOk);
  ShiftSteps(matrix_, 18);

  EXPECT_EQ(RightEdge(matrix_, 18), in_ram);
  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
}

/* a text in RAM after one in flash is read from RAM again: as RAM, it ends at
 * the end of its span, "Hi" of "Hix"; read as flash, it went on to its null
 * character, as the mock reads flash where RAM is */
TEST_F(TextTest, TextShiftInRamAfterOneInFlashReadsRam) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi").GetError(), Error::kOk);
  ShiftUntilReady(matrix_);
  std::vector<uint8_t> const in_ram{RightEdge(matrix_, 15)};
  matrix_.Clear();
  ASSERT_EQ(matrix_.SetTextWithShift(F("AB")).GetError(), Error::kOk);
  char const longer[] = "Hix";

  ASSERT_EQ(matrix_.SetTextWithShift(span<char const>{longer, 2}).GetError(),
            Error::kOk);
  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 15), in_ram);
}

/* a table of texts lies in flash with PROGMEM, where F() cannot be written;
 * FromFlash() hands one on as F() does */
TEST_F(TextTest, TextFromAFlashTableShiftsAsInRam) {
  static char const kTableText[] PROGMEM = "Hi";
  ASSERT_EQ(matrix_.SetTextWithShift("Hi").GetError(), Error::kOk);
  ShiftUntilReady(matrix_);
  std::vector<uint8_t> const in_ram{RightEdge(matrix_, 9)};
  matrix_.Clear();

  ASSERT_EQ(matrix_.SetTextWithShift(FromFlash(kTableText)).GetError(),
            Error::kOk);
  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 9), in_ram);
}

/* SetTextWithShift() took a char*, though the shift only reads the text, so
 * that a sketch could not make its text const */
TEST_F(TextTest, ConstTextShiftsToItsEnd) {
  char const text[] = "Hello World!";
  ASSERT_EQ(matrix_.SetTextWithShift(text).GetError(), Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* A text that repeats goes on with its first char right after the space behind
 * its last (OIL item 31), with no column more between them: "Hi" is 7F 08 08
 * 7F, the space, 44 7D 40, the space, nine columns, so that two passes are the
 * eighteen columns at the right edge. */
TEST_F(TextTest, RepeatingTextStartsOverAfterTheSpaceBehindItsLastChar) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi", true).GetError(), Error::kOk);

  ShiftSteps(matrix_, 18);

  EXPECT_EQ(RightEdge(matrix_, 18),
            (std::vector<uint8_t>{0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D,
                                  0x40, 0x00, 0x7F, 0x08, 0x08, 0x7F, 0x00,
                                  0x44, 0x7D, 0x40, 0x00}));
}

/* "Hi" alone ends after its nine columns and the call that finds its end */
TEST_F(TextTest, RepeatingTextGoesOnPastItsEnd) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi", true).GetError(), Error::kOk);

  ShiftSteps(matrix_, 30);

  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
}

/* a text that repeats but has no char of the font ends at once, rather than
 * reading itself over and over in one call */
TEST_F(TextTest, RepeatingTextWithoutAKnownCharEnds) {
  ASSERT_EQ(matrix_.SetTextWithShift("\x01\x02", true).GetError(), Error::kOk);

  matrix_.ShiftStep();

  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

TEST_F(TextTest, StopShiftEndsARepeatingText) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hello ", true).GetError(), Error::kOk);
  ShiftSteps(matrix_, 20);

  matrix_.StopShift();

  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* the picture as it stood when the shift was stopped stays, whatever calls of
 * ShiftStep() come after */
TEST_F(TextTest, StopShiftHoldsThePicture) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hello ", true).GetError(), Error::kOk);
  ShiftSteps(matrix_, 20);
  std::vector<std::string> const stopped{ShownPicture(matrix_)};

  matrix_.StopShift();
  ShiftSteps(matrix_, 20);

  EXPECT_EQ(ShownPicture(matrix_), stopped);
}

TEST_F(TextTest, StopShiftEndsAShiftingChar) {
  ASSERT_EQ(matrix_.SetCharWithShift('A').GetError(), Error::kOk);
  matrix_.ShiftStep();

  matrix_.StopShift();

  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* Bug: the count of the columns shifted in was a byte, so with the largest
 * space it never reached a char's width and space, and the char shifted for
 * ever; 'A' is four columns wide */
TEST_F(TextTest, CharWithTheLargestSpaceStopsShifting) {
  matrix_.SetSpaceBetweenChars(255);
  ASSERT_EQ(matrix_.SetCharWithShift('A').GetError(), Error::kOk);

  uint16_t const steps{ShiftUntilReady(matrix_)};

  EXPECT_EQ(steps, 4 + 255);
}

/* a sprite draws as many columns as its width and as many rows as its height,
 * from its top left corner; the bits beyond its height, and the columns beyond
 * its width, stay dark: 03 05 06 two rows high are ## #. .# */
TEST_F(TextTest, SpriteDrawsItsWidthAndHeightFromItsCorner) {
  Sprite const sprite{3, 2, {{0x03, 0x05, 0x06, 0xFF, 0xFF}}};

  matrix_.SetSprite(Point{X{1}, Y{2}}, sprite);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "................................",
                                       "................................",
                                       ".##.............................",
                                       ".#.#............................",
                                       "................................",
                                       "................................",
                                       "................................",
                                       "................................",
                                   }));
}

/* a sprite wider than its columns read past its array; it is drawn as wide as
 * its columns, and the column beside them stays as it is */
TEST_F(TextTest, SpriteWiderThanItsColumnsIsDrawnAsWideAsThey) {
  Sprite const too_wide{
      kSpriteMaxWidth + 1, 8, {{0x01, 0x01, 0x01, 0x01, 0x01}}};
  matrix_.SetColumn(X{5}, Byte{0xFF});

  matrix_.SetSprite(Point{X{0}, Y{0}}, too_wide);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "######..........................",
                                       ".....#..........................",
                                       ".....#..........................",
                                       ".....#..........................",
                                       ".....#..........................",
                                       ".....#..........................",
                                       ".....#..........................",
                                       ".....#..........................",
                                   }));
}

/* a sprite higher than a module read bits beyond its bytes, as a bitmap did */
TEST_F(TextTest, SpriteHigherThanAModuleIsDrawnEightHigh) {
  Sprite const too_high{1, 9, {{0x01, 0x00, 0x00, 0x00, 0x00}}};
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.SetSprite(Point{X{0}, Y{-1}}, too_high);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       "################################",
                                   }));
}

TEST_F(TextTest, SpriteOfNoColumnDrawsNothing) {
  Sprite const empty{0, 8, {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}};

  matrix_.SetSprite(Point{X{0}, Y{0}}, empty);

  EXPECT_EQ(ShownPicture(matrix_), Dark());
}

/* a text ends at the end of its span when no null character comes first, so
 * that a buffer without one is not read past its end: the first two chars of
 * "Hi!!" are drawn as "Hi", 7F 08 08 7F, the space, 44 7D 40 */
TEST_F(TextTest, TextEndsAtTheEndOfItsSpanWithoutANullCharacter) {
  char const longer[] = "Hi!!";

  ASSERT_EQ(matrix_.SetText(span<char const>{longer, 2}).GetError(),
            Error::kOk);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "#..#..#.........................",
                                       "#..#............................",
                                       "#..#.##.........................",
                                       "####..#.........................",
                                       "#..#..#.........................",
                                       "#..#..#.........................",
                                       "#..#.###........................",
                                       "................................",
                                   }));
}

/* as SetText(), the shift of a text ends at the end of its span: "Hi" ends
 * with the space behind its i */
TEST_F(TextTest, TextShiftEndsAtTheEndOfItsSpanWithoutANullCharacter) {
  char const longer[] = "Hi!!";
  ASSERT_EQ(matrix_.SetTextWithShift(span<char const>{longer, 2}).GetError(),
            Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 9),
            (std::vector<uint8_t>{0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D,
                                  0x40, 0x00}));
}

/* a text with a char the font lacks draws the chars before it, as SetText()
 * promises: the I of 0x41, 0x7F, 0x41 */
TEST_F(TextTest, TextWithAnUnknownCharDrawsTheCharsBeforeIt) {
  Error const error{matrix_.SetText("I\x01").GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "###.............................",
                                       ".#..............................",
                                       ".#..............................",
                                       ".#..............................",
                                       ".#..............................",
                                       ".#..............................",
                                       "###.............................",
                                       "................................",
                                   }));
}

/* no space between chars sets them side by side: the I of 0x41, 0x7F, 0x41
 * twice */
TEST_F(TextTest, NoSpaceBetweenCharsSetsThemSideBySide) {
  matrix_.SetSpaceBetweenChars(0);

  ASSERT_EQ(matrix_.SetText("II").GetError(), Error::kOk);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "######..........................",
                                       ".#..#...........................",
                                       ".#..#...........................",
                                       ".#..#...........................",
                                       ".#..#...........................",
                                       ".#..#...........................",
                                       "######..........................",
                                       "................................",
                                   }));
}

/* the first step of a char brought two of its columns at once, though the
 * interval is the time per column: '#' brings 0x14 first */
TEST_F(TextTest, CharShiftBringsOneColumnPerStep) {
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);

  matrix_.ShiftStep();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x00, 0x14}));
}

/* a char shifts in with the space behind it, and no more: each char brought
 * one more empty column, as the step that ended it shifted as well; '#' is
 * 0x14, 0x3E, 0x14, 0x3E, 0x14 */
TEST_F(TextTest, CharShiftEndsWithTheSpaceBehindIt) {
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 6),
            (std::vector<uint8_t>{0x14, 0x3E, 0x14, 0x3E, 0x14, 0x00}));
}

/* a text ends with the space behind its last char: the end of a text brought
 * one more empty column; "Hi" is 7F 08 08 7F, the space, 44 7D 40 */
TEST_F(TextTest, TextShiftEndsWithTheSpaceBehindItsLastChar) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi").GetError(), Error::kOk);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 9),
            (std::vector<uint8_t>{0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D,
                                  0x40, 0x00}));
}

}  // namespace
}  // namespace max_matrix
