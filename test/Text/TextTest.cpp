// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Text that shifts in: to its end, over again, and held. */

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Arduino.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* SetTextWithShift() took a char*, though the shift only reads the text, so
 * that a sketch could not make its text const */
TEST_F(MaxMatrixTest, TextShiftFinishes) {
  char const text[] = "Hello World!";

  ShiftWholeText(text);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* A text that repeats goes on with its first char right after its last (OIL
 * item 31): once the picture before it has left the panel, every picture comes
 * again one pass of the text later, with no column between its end and its
 * start, and the shift does not end. A pass is what the text takes when it does
 * not repeat, less the last step, which only shifts the panel once more. */
TEST_F(MaxMatrixTest, RepeatingTextStartsOver) {
  char const text[] = "Hi \xC4\x01 ";
  std::vector<std::vector<std::string>> pictures;

  ShiftWholeText(text);
  uint16_t const pass{static_cast<uint16_t>(shift_calls_ - 1)};
  ASSERT_GT(pass, 0);

  matrix_.Clear();
  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  uint16_t const steps{static_cast<uint16_t>(kNumberOfColumns + (3 * pass))};
  for (uint16_t step{0}; step < steps; step++) {
    matrix_.ShiftTask();
    pictures.push_back(ShownPicture(matrix_));
  }
  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
  EXPECT_NE(pictures.back(), Picture([](int16_t, int16_t) { return false; }));
  for (uint16_t step{kNumberOfColumns}; (step + pass) < pictures.size();
       step++) {
    ASSERT_EQ(pictures[step + pass], pictures[step])
        << "step " << step << " and one pass later";
  }
}

/* a text that repeats but has no char of the font ends at once, rather than
 * reading itself over and over in one call */
TEST_F(MaxMatrixTest, RepeatingTextWithoutAKnownCharEnds) {
  char const text[] = "\x01\x02";

  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* StopShift() ends a text that would repeat for ever, and leaves the picture as
 * it stands */
TEST_F(MaxMatrixTest, StopShiftHoldsThePicture) {
  char const text[] = "Hello ";

  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  for (uint8_t step{0}; step < 20; step++) {
    matrix_.ShiftTask();
  }
  std::vector<std::string> const stopped = ShownPicture(matrix_);

  matrix_.StopShift();
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  for (uint8_t step{0}; step < 20; step++) {
    matrix_.ShiftTask();
  }
  EXPECT_EQ(ShownPicture(matrix_), stopped);
  matrix_.StopShift();
  EXPECT_EQ(matrix_.GetState(), State::kReady) << "stopped twice";
}

/* StopShift() ends a char that shifts in as well as a text */
TEST_F(MaxMatrixTest, StopShiftEndsAShiftingChar) {
  ASSERT_EQ(matrix_.SetCharWithShift('A').GetError(), Error::kOk);
  matrix_.ShiftTask();

  matrix_.StopShift();

  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* Bug: the count of the columns shifted in was a byte, so with the largest
 * space it never reached a char's width and space, and the char shifted for
 * ever */
TEST_F(MaxMatrixTest, CharWithTheLargestSpaceStopsShifting) {
  uint16_t steps{0};

  matrix_.SetSpaceBetweenChars(255);
  ASSERT_EQ(matrix_.SetCharWithShift('A').GetError(), Error::kOk);
  for (; (steps < 1000) && (matrix_.GetState() != State::kReady); steps++) {
    matrix_.ShiftTask();
  }
  EXPECT_EQ(steps, 4 + 255);
}

/* a sprite draws as many columns as its width and as many rows as its height,
 * from its top left corner; the bits beyond its height, and the columns beyond
 * its width, stay dark */
TEST_F(MaxMatrixTest, SpriteDrawsItsWidthAndHeightFromItsCorner) {
  Sprite const sprite{3, 2, {{0x03, 0x05, 0x06, 0xFF, 0xFF}}};
  int16_t const left{1};
  int16_t const top{2};

  matrix_.SetSprite(Point{X{left}, Y{top}}, sprite);

  EXPECT_EQ(ShownPicture(matrix_), Picture([&sprite](int16_t x, int16_t y) {
              int16_t const column{static_cast<int16_t>(x - left)};
              int16_t const row{static_cast<int16_t>(y - top)};
              bool const inside{(column >= 0) && (column < sprite.width) &&
                                (row >= 0) && (row < sprite.height)};
              return inside &&
                     bitRead(sprite.columns[static_cast<size_t>(column)], row);
            }));
}

/* a sprite wider than its columns read past its array; it is drawn as wide as
 * its columns, and the column beside them stays as it is */
TEST_F(MaxMatrixTest, SpriteWiderThanItsColumnsIsDrawnAsWideAsThey) {
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
TEST_F(MaxMatrixTest, SpriteHigherThanAModuleIsDrawnEightHigh) {
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

TEST_F(MaxMatrixTest, SpriteOfNoColumnDrawsNothing) {
  Sprite const empty{0, 8, {{0xFF, 0xFF, 0xFF, 0xFF, 0xFF}}};

  matrix_.SetSprite(Point{X{0}, Y{0}}, empty);

  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }));
}

/* a text ends at the end of its span when no null character comes first, so
 * that a buffer without one is not read past its end: the first two chars of
 * "Hi!!" are drawn and shifted as "Hi" */
TEST_F(MaxMatrixTest, TextEndsAtTheEndOfItsSpanWithoutANullCharacter) {
  char const longer[] = "Hi!!";
  span<char const> const unterminated{longer, 2};

  ASSERT_EQ(matrix_.SetText("Hi").GetError(), Error::kOk);
  std::vector<std::string> const expected{ShownPicture(matrix_)};
  matrix_.Clear();
  ASSERT_EQ(matrix_.SetText(unterminated).GetError(), Error::kOk);
  EXPECT_EQ(ShownPicture(matrix_), expected);

  int32_t const frames{ShiftWholeText("Hi")};
  uint16_t const calls{shift_calls_};
  EXPECT_EQ(ShiftWholeText(unterminated), frames);
  EXPECT_EQ(shift_calls_, calls);
}

}  // namespace
}  // namespace max_matrix
