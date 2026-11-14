// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The facade: what Init() sets, sending at once or in a batch, the pace of a
 * shifting text, and the settings of the MAX7219. */

#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::Batch;
using max_matrix::Error;
using max_matrix::MatrixOf;

TEST_F(MaxMatrixTest, InitConfiguresEveryChip) {
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    SCOPED_TRACE(::testing::Message()
                 << "Module " << static_cast<uint16_t>(module));
    Max7219 const& chip{ChipOfModule(module)};
    EXPECT_EQ(chip.shutdown, max_matrix::max7219::kShutdownModeNormalOperation);
    EXPECT_EQ(chip.display_test,
              max_matrix::max7219::kDisplayTestNormalOperation);
    EXPECT_EQ(chip.scan_limit, max_matrix::max7219::kScanLimitDisplayDigit0To7);
    EXPECT_EQ(chip.decode_mode, max_matrix::max7219::kDecodeModeNoDecode);
    EXPECT_EQ(chip.intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
  }
}

TEST_F(MaxMatrixTest, IntensityReachesEveryChip) {
  EXPECT_EQ(
      matrix_.SetIntensity(max_matrix::max7219::kIntensityMaxValue).GetError(),
      Error::kOk);
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    EXPECT_EQ(ChipOfModule(module).intensity,
              max_matrix::max7219::kIntensityMaxValue)
        << "Module " << static_cast<uint16_t>(module);
  }
}
/* the same drawing in both modes: a frame for every call that changed
 * something, or none until Update(), which sends each changed column of a
 * module once */
TEST_F(MaxMatrixTest, DrawingInBothModes) {
  std::vector<uint8_t> drawn;
  int32_t frames_before{mock::frames};

  for (uint8_t column{0}; column < kNumberOfColumns; column++) {
    matrix_.SetDot(
        column, static_cast<int16_t>(column % max_matrix::kRowNumberOfModule),
        true);
  }
  EXPECT_EQ(mock::frames - frames_before, kNumberOfColumns);
  drawn = Display();

  matrix_.Clear();
  matrix_.SetAutoUpdate(false);
  frames_before = mock::frames;
  for (uint8_t column{0}; column < kNumberOfColumns; column++) {
    matrix_.SetDot(
        column, static_cast<int16_t>(column % max_matrix::kRowNumberOfModule),
        true);
  }
  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0))
      << "shown before Update()";

  matrix_.Update();
  EXPECT_EQ(mock::frames - frames_before, max_matrix::kColumnNumberOfModule);
  EXPECT_EQ(Display(), drawn);
}

TEST_F(MaxMatrixTest, AutoUpdateOnSendsWhatWasDrawn) {
  matrix_.SetAutoUpdate(false);
  matrix_.SetText("Hi!");
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));

  matrix_.SetAutoUpdate(true);
  EXPECT_NE(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

TEST_F(MaxMatrixTest, BatchSendsAtTheEndOfTheBlock) {
  int32_t const frames_before{mock::frames};

  {
    Batch batch{matrix_};
    for (uint8_t column{0}; column < kNumberOfColumns; column++) {
      matrix_.SetDot(
          column, static_cast<int16_t>(column % max_matrix::kRowNumberOfModule),
          true);
    }
    EXPECT_FALSE(matrix_.GetAutoUpdate());
    EXPECT_EQ(mock::frames, frames_before);
    EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0))
        << "shown inside the block";
  }
  EXPECT_TRUE(matrix_.GetAutoUpdate());
  EXPECT_EQ(mock::frames - frames_before, max_matrix::kColumnNumberOfModule);
}

/* the inner batch puts back the state it found, which the outer one had turned
 * off */
TEST_F(MaxMatrixTest, NestedBatchesSendAtTheOutermost) {
  int32_t const frames_before{mock::frames};

  {
    Batch outer{matrix_};
    {
      Batch inner{matrix_};
      matrix_.SetDot(1, 1, true);
    }
    EXPECT_EQ(mock::frames, frames_before)
        << "sent at the end of the inner block";
    matrix_.SetDot(2, 2, true);
  }
  EXPECT_GT(mock::frames, frames_before);
}

/* a batch inside a sketch that sends with Update() itself leaves it that way */
TEST_F(MaxMatrixTest, BatchKeepsAutoUpdateOff) {
  matrix_.SetAutoUpdate(false);
  int32_t const frames_before{mock::frames};

  {
    Batch batch{matrix_};
    matrix_.SetDot(1, 1, true);
  }
  EXPECT_FALSE(matrix_.GetAutoUpdate());
  EXPECT_EQ(mock::frames, frames_before);
  matrix_.Update();
}

TEST_F(MaxMatrixTest, ShiftIntervalStartsAtTheConfiguredValue) {
  MatrixOf<kNumberOfModules> const fresh{kDataInPin, kChipSelectPin, kClockPin};

  EXPECT_EQ(fresh.GetShiftInterval(), MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE);
}

/* The tests below count the steps of a shifting char in sprite_shift_counter_,
 * which CharShiftTask() moves on by one per step, and hold the clock, so that a
 * step 50 ms on does not depend on how fast the host is. */

/* the first step comes at once, every further one an interval after the step
 * before */
TEST_F(MaxMatrixTest, ShiftWaitsForTheInterval) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};

  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1)
      << "no first step at once";
  mock::SetMillis(1049);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1)
      << "a step before the interval had passed";
  mock::SetMillis(1050);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
}

/* a call after loop() was held up takes one step, not the ones it missed, and
 * the interval counts from there */
TEST_F(MaxMatrixTest, LateShiftDoesNotCatchUp) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};

  matrix_.ShiftTask();
  mock::SetMillis(1180);
  matrix_.ShiftTask();
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
  mock::SetMillis(1229);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
  mock::SetMillis(1230);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 3);
}

TEST_F(MaxMatrixTest, ShiftIntervalZeroStepsOnEveryCall) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(0);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};

  for (uint8_t call{0}; call < 3; call++) {
    matrix_.ShiftTask();
  }
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 3);
}

/* a new char or text starts at once, however recently the one before took a
 * step */
TEST_F(MaxMatrixTest, NewShiftStartsAtOnce) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  matrix_.ShiftTask();

  mock::SetMillis(1010);
  ASSERT_EQ(matrix_.SetCharWithShift('+').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1);
}
/* shutdown darkens every module and loses nothing: a dot drawn meanwhile shows
 * once it ends, at the intensity set before */
TEST_F(MaxMatrixTest, ShutdownKeepsThePicture) {
  std::vector<uint8_t> const dark(kNumberOfColumns, 0x00);

  ASSERT_TRUE(matrix_.SetIntensity(9).IsOk());
  ASSERT_TRUE(matrix_.SetText("Hi").IsOk());
  matrix_.SetShutdown(true);
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    EXPECT_EQ(ChipOfModule(module).shutdown, 0)
        << "module " << static_cast<uint16_t>(module);
  }
  EXPECT_EQ(Display(), dark);
  matrix_.SetDot(31, 7, true);
  EXPECT_EQ(Display(), dark) << "lit in shutdown";

  matrix_.SetShutdown(false);
  EXPECT_TRUE(matrix_.GetDot(31, 7).GetValue());
  EXPECT_EQ(Display()[31], 0x80) << "the dot drawn in shutdown";
  EXPECT_EQ(ChipOfModule(0).intensity, 9);
}

/* the display test lights every LED of every module at full brightness, even in
 * shutdown, and the picture is back when it ends */
TEST_F(MaxMatrixTest, DisplayTestLightsEveryLed) {
  ASSERT_TRUE(matrix_.SetText("Hi").IsOk());
  std::vector<uint8_t> const shown = Display();

  matrix_.SetShutdown(true);
  matrix_.SetDisplayTest(true);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0xFF));
  matrix_.SetDisplayTest(false);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0x00))
      << "still in shutdown";
  matrix_.SetShutdown(false);
  EXPECT_EQ(Display(), shown);
}
