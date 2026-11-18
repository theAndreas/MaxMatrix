// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The buffer as wired: which columns go out, and when nothing has to. */

#include "Matrix/Wired.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "Arduino.h"
#include "Max7219.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* a changed column of a module goes out in one frame with the same column of
 * every other module; a test of internal::Wired itself, which names the buffer
 * column as wired rather than reaching it through an orientation */
TEST(WiredTest, ChangedColumnIsSentForEveryModule) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  array<Byte, BufferSize(kNumberOfModules)> buffer{};
  internal::Wired wired{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}, buffer};
  ASSERT_TRUE(wired.GetChain().Init(kNumberOfModules).IsOk());
  wired.Send();
  uint8_t const module{kNumberOfModules - 2};
  int32_t const frames_before{mock::frames};

  wired.StoreBit((module * kColumnNumberOfModule) + 3, 1, true);
  wired.Send();
  EXPECT_EQ(mock::frames - frames_before, 1);
  ASSERT_EQ(mock::last_frame.size(), 2u * kNumberOfModules);
  for (uint8_t i{0}; i < kNumberOfModules; i++) {
    SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(i));
    EXPECT_EQ(mock::last_frame[size_t{2} * i],
              static_cast<uint8_t>(max7219::Register::kDigit3));
    EXPECT_EQ(mock::last_frame[(size_t{2} * i) + 1],
              (i == module) ? 0x02 : 0x00);
  }
  EXPECT_EQ(mock::errors, 0);
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
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

TEST_F(MaxMatrixTest, DrawingWhatIsShownSendsNothing) {
  matrix_.SetText("Hi!");
  int32_t const frames_before{mock::frames};

  matrix_.SetText("Hi!");
  matrix_.SetDot(Point{X{0}, Y{0}},
                 matrix_.GetDot(Point{X{0}, Y{0}}).GetValue());
  EXPECT_EQ(mock::frames, frames_before);
}

}  // namespace
}  // namespace max_matrix
