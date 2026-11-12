// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The buffer as wired: which columns go out, and when nothing has to. */

#include <vector>

#include "MaxMatrixTestSupport.h"

/* a changed column of a module goes out in one frame with the same column of
 * every other module */
TEST_F(MaxMatrixTest, ChangedColumnIsSentForEveryModule) {
  byte const module = kNumberOfModules - 2;
  int32_t const frames_before{mock::frames};

  matrix_.panel_.wired_.StoreBit(module * max_matrix::kColumnNumberOfModule + 3,
                                 1, true);
  matrix_.Update();
  EXPECT_EQ(mock::frames - frames_before, 1);
  ASSERT_EQ(mock::last_frame.size(), 2u * kNumberOfModules);
  for (byte i = 0; i < kNumberOfModules; i++) {
    SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(i));
    EXPECT_EQ(mock::last_frame[size_t{2} * i],
              max_matrix::max7219::kDigit3Address);
    EXPECT_EQ(mock::last_frame[size_t{2} * i + 1], i == module ? 0x02 : 0x00);
  }
}

/* Clear() has to reach LEDs the buffer knows nothing of: after power-up the
 * registers hold anything while the buffer is empty, so a clear that sent only
 * what changed in the buffer would send nothing */
TEST_F(MaxMatrixTest, InitClearsWhatThePowerUpLeft) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  for (Max7219& chip : mock::chain.chips) {
    for (uint8_t& digit : chip.digit) {
      digit = 0xA5;
    }
  }
  matrix_.Init();
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));
}

TEST_F(MaxMatrixTest, DrawingWhatIsShownSendsNothing) {
  matrix_.SetText("Hi!");
  int32_t const frames_before{mock::frames};

  matrix_.SetText("Hi!");
  matrix_.SetDot(0, 0, (matrix_.panel_.wired_.GetColumn(0) & 1U) != 0U);
  EXPECT_EQ(mock::frames, frames_before);
}
