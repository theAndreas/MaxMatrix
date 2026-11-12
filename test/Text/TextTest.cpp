// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Text that shifts in: to its end, over again, and held. */

#include <string>
#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::Error;
using max_matrix::State;

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
  uint16_t const pass = shift_calls_ - 1;
  ASSERT_GT(pass, 0);

  matrix_.Clear();
  matrix_.SetDot(0, 0, true);
  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  for (uint16_t step = 0; step < kNumberOfColumns + 3 * pass; step++) {
    matrix_.ShiftTask();
    pictures.push_back(ShownPicture(matrix_));
  }
  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
  EXPECT_NE(pictures.back(), Picture([](int16_t, int16_t) { return false; }));
  for (uint16_t step = kNumberOfColumns; step + pass < pictures.size();
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
  for (byte step = 0; step < 20; step++) matrix_.ShiftTask();
  std::vector<std::string> const stopped = ShownPicture(matrix_);

  matrix_.StopShift();
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  for (byte step = 0; step < 20; step++) matrix_.ShiftTask();
  EXPECT_EQ(ShownPicture(matrix_), stopped);
  matrix_.StopShift();
  EXPECT_EQ(matrix_.GetState(), State::kReady) << "stopped twice";
}
/* Bug: the count of the columns shifted in was a byte, so with the largest
 * space it never reached a char's width and space, and the char shifted for
 * ever */
TEST_F(MaxMatrixTest, CharWithTheLargestSpaceStopsShifting) {
  uint16_t steps = 0;

  matrix_.SetSpaceBetweenChars(255);
  ASSERT_EQ(matrix_.SetCharWithShift('A').GetError(), Error::kOk);
  for (; steps < 1000 && matrix_.GetState() != State::kReady; steps++) {
    matrix_.ShiftTask();
  }
  EXPECT_EQ(steps, 4 + 255);
}
