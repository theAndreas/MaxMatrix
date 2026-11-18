// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The facade: what Init() sets, sending at once or in a batch, the pace of a
 * shifting text, and the settings of the MAX7219. */

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Arduino.h"
#include "Max7219.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

TEST_F(MaxMatrixTest, InitConfiguresEveryChip) {
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    SCOPED_TRACE(::testing::Message()
                 << "Module " << static_cast<uint16_t>(module));
    Max7219 const& chip{ChipOfModule(module)};
    EXPECT_EQ(chip.shutdown, max7219::kShutdownModeNormalOperation);
    EXPECT_EQ(chip.display_test, max7219::kDisplayTestNormalOperation);
    EXPECT_EQ(chip.scan_limit, max7219::kScanLimitDisplayDigit0To7);
    EXPECT_EQ(chip.decode_mode, max7219::kDecodeModeNoDecode);
    EXPECT_EQ(chip.intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
  }
}

/* a shift started before Init() had succeeded set its state, so that GetState()
 * no longer said that the chain was not set up, and Init() then ended the shift
 * unseen */
TEST_F(MaxMatrixTest, ShiftNeedsInit) {
  MatrixOf<kNumberOfModules> matrix{DataInPin{kDataInPin},
                                    ChipSelectPin{kChipSelectPin},
                                    ClockPin{kClockPin}};

  Error const char_error{matrix.SetCharWithShift('A').GetError()};
  Error const text_error{matrix.SetTextWithShift("Hi").GetError()};

  EXPECT_EQ(char_error, Error::kNotSetUp);
  EXPECT_EQ(text_error, Error::kNotSetUp);
  EXPECT_EQ(matrix.GetState(), State::kNone);
}

/* a setting sent before Init() had succeeded reported success, or nothing, and
 * Init() then sent its own; the setting was lost unseen */
TEST_F(MaxMatrixTest, SettingsNeedInit) {
  MatrixOf<kNumberOfModules> matrix{DataInPin{kDataInPin},
                                    ChipSelectPin{kChipSelectPin},
                                    ClockPin{kClockPin}};
  int32_t const frames_before{mock::frames};

  Error const intensity_error{matrix.SetIntensity(1).GetError()};
  Error const shutdown_error{matrix.SetShutdown(true).GetError()};
  Error const display_test_error{matrix.SetDisplayTest(true).GetError()};

  EXPECT_EQ(intensity_error, Error::kNotSetUp);
  EXPECT_EQ(shutdown_error, Error::kNotSetUp);
  EXPECT_EQ(display_test_error, Error::kNotSetUp);
  EXPECT_EQ(mock::frames, frames_before);
}

TEST_F(MaxMatrixTest, IntensityReachesEveryChip) {
  EXPECT_EQ(matrix_.SetIntensity(max7219::kIntensityMaxValue).GetError(),
            Error::kOk);
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    EXPECT_EQ(ChipOfModule(module).intensity, max7219::kIntensityMaxValue)
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
        Point{X{column}, Y{static_cast<int16_t>(column % kRowNumberOfModule)}},
        true);
  }
  EXPECT_EQ(mock::frames - frames_before, kNumberOfColumns);
  drawn = Display();

  matrix_.Clear();
  matrix_.SetAutoUpdate(false);
  frames_before = mock::frames;
  for (uint8_t column{0}; column < kNumberOfColumns; column++) {
    matrix_.SetDot(
        Point{X{column}, Y{static_cast<int16_t>(column % kRowNumberOfModule)}},
        true);
  }
  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0))
      << "shown before Update()";

  matrix_.Update();
  EXPECT_EQ(mock::frames - frames_before, kColumnNumberOfModule);
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
    Batch const batch{matrix_};
    for (uint8_t column{0}; column < kNumberOfColumns; column++) {
      matrix_.SetDot(
          Point{X{column},
                Y{static_cast<int16_t>(column % kRowNumberOfModule)}},
          true);
    }
    EXPECT_FALSE(matrix_.GetAutoUpdate());
    EXPECT_EQ(mock::frames, frames_before);
    EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0))
        << "shown inside the block";
  }
  EXPECT_TRUE(matrix_.GetAutoUpdate());
  EXPECT_EQ(mock::frames - frames_before, kColumnNumberOfModule);
}

/* the inner batch puts back the state it found, which the outer one had turned
 * off */
TEST_F(MaxMatrixTest, NestedBatchesSendAtTheOutermost) {
  int32_t const frames_before{mock::frames};

  {
    Batch const outer{matrix_};
    {
      Batch const inner{matrix_};
      matrix_.SetDot(Point{X{1}, Y{1}}, true);
    }
    EXPECT_EQ(mock::frames, frames_before)
        << "sent at the end of the inner block";
    matrix_.SetDot(Point{X{2}, Y{2}}, true);
  }
  EXPECT_GT(mock::frames, frames_before);
}

/* a batch inside a sketch that sends with Update() itself leaves it that way */
TEST_F(MaxMatrixTest, BatchKeepsAutoUpdateOff) {
  matrix_.SetAutoUpdate(false);
  int32_t const frames_before{mock::frames};

  {
    Batch const batch{matrix_};
    matrix_.SetDot(Point{X{1}, Y{1}}, true);
  }
  EXPECT_FALSE(matrix_.GetAutoUpdate());
  EXPECT_EQ(mock::frames, frames_before);
  matrix_.Update();
}

TEST_F(MaxMatrixTest, ShiftIntervalStartsAtTheConfiguredValue) {
  MatrixOf<kNumberOfModules> const fresh{DataInPin{kDataInPin},
                                         ChipSelectPin{kChipSelectPin},
                                         ClockPin{kClockPin}};

  EXPECT_EQ(fresh.GetShiftInterval(), MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE);
}

/* The tests below tell the steps of a shifting char by the three columns at
 * the right edge of the panel, where '#' brings 0x14, 0x3E, 0x14 and 0x3E, the
 * first when the shift starts and one more per step, and hold the clock, so
 * that a step 50 ms on does not depend on how fast the host is. Up to three
 * steps, each leaves the edge another picture. */
std::vector<uint8_t> RightEdge(Matrix const& matrix, uint8_t width = 3) {
  std::vector<uint8_t> columns;
  for (auto x{static_cast<int16_t>(kNumberOfColumns - width)};
       x < kNumberOfColumns; x++) {
    columns.push_back(matrix.GetColumn(X{x}).GetValue().ToInteger());
  }
  return columns;
}

/* the first step comes at once, every further one an interval after the step
 * before */
TEST_F(MaxMatrixTest, ShiftWaitsForTheInterval) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);

  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}))
      << "no first step at once";
  mock::SetMillis(1049);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}))
      << "a step before the interval had passed";
  mock::SetMillis(1050);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x14, 0x3E, 0x14}));
}

/* a call after loop() was held up takes one step, not the ones it missed, and
 * the interval counts from there */
TEST_F(MaxMatrixTest, LateShiftDoesNotCatchUp) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);

  matrix_.ShiftTask();
  mock::SetMillis(1180);
  matrix_.ShiftTask();
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x14, 0x3E, 0x14}));
  mock::SetMillis(1229);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x14, 0x3E, 0x14}));
  mock::SetMillis(1230);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x3E, 0x14, 0x3E}));
}

TEST_F(MaxMatrixTest, ShiftIntervalZeroStepsOnEveryCall) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(0);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);

  matrix_.ShiftTask();
  matrix_.ShiftTask();
  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x3E, 0x14, 0x3E}));
}

/* a new char or text starts at once, however recently the one before took a
 * step: '+' brings 0x08, 0x08 and 0x3E */
TEST_F(MaxMatrixTest, NewShiftStartsAtOnce) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  matrix_.ShiftTask();
  mock::SetMillis(1010);
  ASSERT_EQ(matrix_.SetCharWithShift('+').GetError(), Error::kOk);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_, 2), (std::vector<uint8_t>{0x08, 0x08}));
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
  matrix_.SetDot(Point{X{31}, Y{7}}, true);
  EXPECT_EQ(Display(), dark) << "lit in shutdown";

  matrix_.SetShutdown(false);
  EXPECT_TRUE(matrix_.GetDot(Point{X{31}, Y{7}}).GetValue());
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

/* a buffer that is no chain of 1 to kMaxNumberOfModules modules: more than 255
 * bytes wrapped the count of the columns, a length that is no multiple of eight
 * left columns no module has, and none was a chain of no module */
struct BufferCase {
  char const* name;
  size_t length;
};

class MatrixBufferTest : public ::testing::TestWithParam<BufferCase> {};

std::string BufferCaseName(::testing::TestParamInfo<BufferCase> const& info) {
  return info.param.name;
}

TEST_P(MatrixBufferTest, InitRefusesABufferThatIsNoChain) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  std::vector<Byte> buffer(GetParam().length);
  Matrix matrix{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                ClockPin{kClockPin}, span<Byte>{buffer.data(), buffer.size()}};

  Error const error{matrix.Init().GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(matrix.GetState(), State::kNone);
  EXPECT_EQ(mock::frames, 0);
  EXPECT_EQ(mock::pin_mode[kChipSelectPin], INPUT);
}

INSTANTIATE_TEST_SUITE_P(
    Lengths, MatrixBufferTest,
    ::testing::Values(BufferCase{"Empty", 0},
                      BufferCase{"LessThanAModule", kColumnNumberOfModule - 1},
                      BufferCase{"OneColumnMore", kColumnNumberOfModule + 1},
                      BufferCase{"OneModuleTooMany",
                                 BufferSize(kMaxNumberOfModules + 1)}),
    BufferCaseName);

/* an empty buffer, which Init() refuses, was cleared by a memset() of a null
 * pointer, which C does not define even for no byte */
TEST(MatrixTest, ClearOfAnEmptyBufferSendsNothing) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  Matrix matrix{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                ClockPin{kClockPin}, span<Byte>{}};

  matrix.Clear();

  EXPECT_EQ(mock::frames, 0);
  EXPECT_EQ(mock::errors, 0);
}

/* the longest chain, kMaxNumberOfModules modules, is one */
TEST(MatrixTest, InitTakesTheLongestChain) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kMaxNumberOfModules);
  array<Byte, BufferSize(kMaxNumberOfModules)> buffer{};
  Matrix matrix{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                ClockPin{kClockPin}, buffer};

  Error const error{matrix.Init().GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(matrix.GetNumberOfModules(), kMaxNumberOfModules);
  EXPECT_EQ(mock::chain.chips.back().intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
  EXPECT_EQ(mock::errors, 0);
}

/* Init() again clears what was drawn, as the first clears what the power-up
 * left (TS-26) */
TEST_F(MaxMatrixTest, InitAgainClearsThePanel) {
  matrix_.SetDot(Point{X{3}, Y{2}}, true);

  Error const error{matrix_.Init().GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

/* Init() during a shift ends it, as it clears the panel the shift draws on */
TEST_F(MaxMatrixTest, InitEndsARunningShift) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi", true).GetError(), Error::kOk);
  matrix_.ShiftTask();

  Error const error{matrix_.Init().GetError()};
  matrix_.ShiftTask();

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

/* what is drawn before Init() only changes the buffer, which Init() clears */
TEST(MatrixTest, InitClearsWhatWasDrawnBefore) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  array<Byte, BufferSize(kNumberOfModules)> buffer{};
  Matrix matrix{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                ClockPin{kClockPin}, buffer};
  matrix.SetDot(Point{X{3}, Y{2}}, true);

  Error const error{matrix.Init().GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_FALSE(matrix.GetDot(Point{X{3}, Y{2}}).GetValue());
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

TEST(MatrixTest, ShiftTaskBeforeInitDoesNothing) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  array<Byte, BufferSize(kNumberOfModules)> buffer{};
  Matrix matrix{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                ClockPin{kClockPin}, buffer};

  matrix.ShiftTask();

  EXPECT_EQ(matrix.GetState(), State::kNone);
  EXPECT_EQ(mock::frames, 0);
}

TEST(MatrixTest, StopShiftBeforeInitLeavesTheMatrixNotSetUp) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  array<Byte, BufferSize(kNumberOfModules)> buffer{};
  Matrix matrix{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                ClockPin{kClockPin}, buffer};

  matrix.StopShift();

  EXPECT_EQ(matrix.GetState(), State::kNone);
}

TEST_F(MaxMatrixTest, ShiftTaskWithoutAShiftSendsNothing) {
  matrix_.SetText("Hi");
  int32_t const frames_before{mock::frames};

  matrix_.ShiftTask();

  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

TEST_F(MaxMatrixTest, StopShiftWithoutAShiftDoesNothing) {
  matrix_.SetText("Hi");
  int32_t const frames_before{mock::frames};

  matrix_.StopShift();

  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* with auto update on, every call has sent its result already */
TEST_F(MaxMatrixTest, UpdateWithNothingChangedSendsNothing) {
  matrix_.SetText("Hi");
  int32_t const frames_before{mock::frames};

  matrix_.Update();

  EXPECT_EQ(mock::frames, frames_before);
}

TEST_F(MaxMatrixTest, CharShiftTakesOverFromARunningText) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi", true).GetError(), Error::kOk);
  matrix_.ShiftTask();

  Error const error{matrix_.SetCharWithShift('#').GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(matrix_.GetState(), State::kCharShift);
}

TEST_F(MaxMatrixTest, TextShiftTakesOverFromARunningChar) {
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  matrix_.ShiftTask();

  Error const error{matrix_.SetTextWithShift("Hi").GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
}

/* a shift that fails to start leaves the one that runs, as nothing starts on
 * an error */
TEST_F(MaxMatrixTest, CharShiftThatFailsLeavesTheRunningText) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi", true).GetError(), Error::kOk);
  matrix_.ShiftTask();

  Error const error{matrix_.SetCharWithShift('\x01').GetError()};

  EXPECT_EQ(error, Error::kUnknownChar);
  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
}

TEST_F(MaxMatrixTest, TextShiftThatFailsLeavesTheRunningChar) {
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  matrix_.ShiftTask();

  Error const error{matrix_.SetTextWithShift({}).GetError()};

  EXPECT_EQ(error, Error::kNoText);
  EXPECT_EQ(matrix_.GetState(), State::kCharShift);
}

/* the interval is counted across the overflow of millis() after 49.7 days: a
 * step 50 ms after 0xFFFFFFF0 comes at 0x22, and none at 0xFFFFFFFF, where a
 * comparison with the sum, which wraps first, takes one (H-2) */
TEST_F(MaxMatrixTest, ShiftIntervalSpansTheOverflowOfTheClock) {
  mock::SetMillis(0xFFFFFFF0u);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  matrix_.ShiftTask();

  mock::SetMillis(0xFFFFFFFFu);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}))
      << "a step before the clock wrapped";
  mock::SetMillis(0x21u);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}))
      << "a step before the interval had passed";
  mock::SetMillis(0x22u);
  matrix_.ShiftTask();
  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x14, 0x3E, 0x14}));
}

TEST_F(MaxMatrixTest, IntensityTakesItsLeastValue) {
  Error const error{matrix_.SetIntensity(0).GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(ChipOfModule(0).intensity, 0);
}

TEST(MatrixTest, BufferSizeIsMadeWhenCompiling) {
  static_assert(BufferSize(1) == 8, "a byte per column");
  static_assert(BufferSize(kMaxNumberOfModules) == 248, "of the longest chain");

  EXPECT_EQ(BufferSize(4), 32u);
}

}  // namespace
}  // namespace max_matrix
