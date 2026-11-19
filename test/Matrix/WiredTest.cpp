// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The buffer as wired: which columns go out, and when nothing has to. */

#include "Matrix/Wired.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include <vector>

#include "Arduino.h"
#include "Max7219.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* internal::Wired itself, set up on the chain of the examples with all it
 * shows sent, so that a test names the buffer column as wired rather than
 * reaching it through an orientation */
class WiredTest : public ::testing::Test {
 protected:
  array<Byte, BufferSize(kNumberOfModules)> buffer_{};
  internal::Wired wired_{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                         ClockPin{kClockPin}, buffer_};

  void SetUp() override {
    mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
    ASSERT_TRUE(wired_.GetChain().Init(kNumberOfModules).IsOk());
    wired_.Send();
  }

  /* every bit went out within a frame */
  void TearDown() override { EXPECT_EQ(mock::errors, 0); }
};

/* column 3 of module 2 */
constexpr uint8_t kColumn{(2 * kColumnNumberOfModule) + 3};

TEST_F(WiredTest, ChangedColumnGoesOutInOneFrame) {
  int32_t const frames_before{mock::frames};
  wired_.StoreBit(kColumn, 1, true);

  wired_.Send();

  EXPECT_EQ(mock::frames - frames_before, 1);
}

/* a changed column of a module goes out with the same column of every other
 * module: digit 3, register 04, for each, 02 for module 2 */
TEST_F(WiredTest, ChangedColumnIsSentForEveryModule) {
  wired_.StoreBit(kColumn, 1, true);

  wired_.Send();

  EXPECT_EQ(mock::last_frame, (std::vector<uint8_t>{0x04, 0x00, 0x04, 0x00,
                                                    0x04, 0x02, 0x04, 0x00}));
}

/* The tests through the matrix, as a sketch sees what Wired sends. */
class WiredMatrixTest : public MaxMatrixTest {};

/* Clear() has to reach LEDs the buffer knows nothing of: after power-up the
 * registers hold anything while the buffer is empty, so a clear that sent only
 * what changed in the buffer would send nothing */
TEST_F(WiredMatrixTest, InitClearsWhatThePowerUpLeft) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  mock::chain.chips[0].digit[0] = 0xA5;
  mock::chain.chips[3].digit[7] = 0xA5;

  ASSERT_EQ(matrix_.Init().GetError(), Error::kOk);

  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns));
}

TEST_F(WiredMatrixTest, DrawingTheSameTextAgainSendsNothing) {
  matrix_.SetText("Hi!");
  int32_t const frames_before{mock::frames};

  matrix_.SetText("Hi!");

  EXPECT_EQ(mock::frames, frames_before);
}

TEST_F(WiredMatrixTest, SettingADotAsItIsSendsNothing) {
  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  int32_t const frames_before{mock::frames};

  matrix_.SetDot(Point{X{0}, Y{0}}, true);

  EXPECT_EQ(mock::frames, frames_before);
}

}  // namespace
}  // namespace max_matrix
