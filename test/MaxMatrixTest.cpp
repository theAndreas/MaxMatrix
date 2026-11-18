// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The facade: what Init() sets, sending at once or on Update(), the pace of a
 * shifting text, and the settings of the MAX7219. Batch has a file of its own.
 */

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

/* a matrix on the fixture's pins that no Init() has set up yet */
class MatrixBeforeInitTest : public ::testing::Test {
 protected:
  array<Byte, BufferSize(kNumberOfModules)> buffer_{};
  Matrix matrix_{DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                 ClockPin{kClockPin}, buffer_};

  void SetUp() override {
    mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  }
};

TEST_F(MaxMatrixTest, InitConfiguresEveryChip) {
  EXPECT_EQ(ChipValues(&Max7219::shutdown),
            std::vector<uint8_t>(kNumberOfModules,
                                 max7219::kShutdownModeNormalOperation));
  EXPECT_EQ(ChipValues(&Max7219::display_test),
            std::vector<uint8_t>(kNumberOfModules,
                                 max7219::kDisplayTestNormalOperation));
  EXPECT_EQ(ChipValues(&Max7219::scan_limit),
            std::vector<uint8_t>(kNumberOfModules,
                                 max7219::kScanLimitDisplayDigit0To7));
  EXPECT_EQ(
      ChipValues(&Max7219::decode_mode),
      std::vector<uint8_t>(kNumberOfModules, max7219::kDecodeModeNoDecode));
  EXPECT_EQ(
      ChipValues(&Max7219::intensity),
      std::vector<uint8_t>(kNumberOfModules, MAXMATRIX_INTENSITY_INIT_VALUE));
}

/* a shift started before Init() had succeeded set its state, so that GetState()
 * no longer said that the chain was not set up, and Init() then ended the shift
 * unseen */
TEST_F(MatrixBeforeInitTest, CharShiftNeedsInit) {
  Error const error{matrix_.SetCharWithShift('A').GetError()};

  EXPECT_EQ(error, Error::kNotSetUp);
  EXPECT_EQ(matrix_.GetState(), State::kNone);
}

TEST_F(MatrixBeforeInitTest, TextShiftNeedsInit) {
  Error const error{matrix_.SetTextWithShift("Hi").GetError()};

  EXPECT_EQ(error, Error::kNotSetUp);
  EXPECT_EQ(matrix_.GetState(), State::kNone);
}

/* a setting sent before Init() had succeeded reported success, or nothing, and
 * Init() then sent its own; the setting was lost unseen */
TEST_F(MatrixBeforeInitTest, IntensityNeedsInit) {
  Error const error{matrix_.SetIntensity(1).GetError()};

  EXPECT_EQ(error, Error::kNotSetUp);
  EXPECT_EQ(mock::frames, 0);
}

TEST_F(MatrixBeforeInitTest, ShutdownNeedsInit) {
  Error const error{matrix_.SetShutdown(true).GetError()};

  EXPECT_EQ(error, Error::kNotSetUp);
  EXPECT_EQ(mock::frames, 0);
}

TEST_F(MatrixBeforeInitTest, DisplayTestNeedsInit) {
  Error const error{matrix_.SetDisplayTest(true).GetError()};

  EXPECT_EQ(error, Error::kNotSetUp);
  EXPECT_EQ(mock::frames, 0);
}

TEST_F(MatrixBeforeInitTest, ShiftIntervalStartsAtTheConfiguredValue) {
  uint16_t const interval{matrix_.GetShiftInterval()};

  EXPECT_EQ(interval, MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE);
}

TEST_F(MaxMatrixTest, IntensityReachesEveryChip) {
  Error const error{
      matrix_.SetIntensity(max7219::kIntensityMaxValue).GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(ChipValues(&Max7219::intensity),
            std::vector<uint8_t>(kNumberOfModules, 15));
}

TEST_F(MaxMatrixTest, AutoUpdateSendsAFrameForEveryCallThatDraws) {
  int32_t const frames_before{mock::frames};

  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  matrix_.SetDot(Point{X{9}, Y{1}}, true);

  EXPECT_EQ(mock::frames - frames_before, 2);
}

TEST_F(MaxMatrixTest, AutoUpdateOffSendsNothing) {
  matrix_.SetAutoUpdate(false);
  int32_t const frames_before{mock::frames};

  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  matrix_.SetDot(Point{X{9}, Y{1}}, true);

  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
  // the check after every test compares the LEDs with the buffer
  matrix_.Update();
}

/* Update() sends each changed column of a module once, in one frame for every
 * module: columns 0 and 3 here, column 0 changed in two modules */
TEST_F(MaxMatrixTest, UpdateSendsEachChangedColumnOfAModuleOnce) {
  matrix_.SetAutoUpdate(false);
  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  matrix_.SetDot(Point{X{3}, Y{2}}, true);
  matrix_.SetDot(Point{X{8}, Y{1}}, true);
  int32_t const frames_before{mock::frames};

  matrix_.Update();

  EXPECT_EQ(mock::frames - frames_before, 2);
  EXPECT_EQ(Display(), (std::vector<uint8_t>{
                           0x01, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,  //
                           0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST_F(MaxMatrixTest, AutoUpdateOnSendsWhatWasDrawn) {
  matrix_.SetAutoUpdate(false);
  matrix_.SetDot(Point{X{3}, Y{2}}, true);

  matrix_.SetAutoUpdate(true);

  EXPECT_EQ(Display(), (std::vector<uint8_t>{
                           0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

/* The tests below tell the steps of a shifting char by the three columns at
 * the right edge of the panel, where '#' brings 0x14, 0x3E and 0x14, one per
 * step, and hold the clock, so that a step 50 ms on does not depend on how
 * fast the host is. Up to three steps, each leaves the edge another picture. */
/* a char shifting in at 50 ms per step, its shift started at `start` */
class ShiftPaceTest : public MaxMatrixTest {
 protected:
  void StartShift(uint32_t start) {
    mock::SetMillis(start);
    matrix_.SetShiftInterval(50);
    ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  }
};

TEST_F(ShiftPaceTest, FirstStepComesAtOnce) {
  StartShift(1000);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x00, 0x14}));
}

TEST_F(ShiftPaceTest, NoStepBeforeTheIntervalHasPassed) {
  StartShift(1000);
  matrix_.ShiftTask();
  mock::SetMillis(1049);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x00, 0x14}));
}

TEST_F(ShiftPaceTest, StepOnceTheIntervalHasPassed) {
  StartShift(1000);
  matrix_.ShiftTask();
  mock::SetMillis(1050);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}));
}

/* a call after loop() was held up takes one step, not the ones it missed */
TEST_F(ShiftPaceTest, LateCallTakesOneStep) {
  StartShift(1000);
  matrix_.ShiftTask();
  mock::SetMillis(1180);

  matrix_.ShiftTask();
  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}));
}

/* the interval counts from a late step, not from when it was due */
TEST_F(ShiftPaceTest, NoStepAnIntervalAfterTheMissedOne) {
  StartShift(1000);
  matrix_.ShiftTask();
  mock::SetMillis(1180);
  matrix_.ShiftTask();
  mock::SetMillis(1229);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}));
}

TEST_F(ShiftPaceTest, StepAnIntervalAfterTheLateOne) {
  StartShift(1000);
  matrix_.ShiftTask();
  mock::SetMillis(1180);
  matrix_.ShiftTask();
  mock::SetMillis(1230);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x14, 0x3E, 0x14}));
}

TEST_F(ShiftPaceTest, IntervalZeroStepsOnEveryCall) {
  StartShift(1000);
  matrix_.SetShiftInterval(0);

  matrix_.ShiftTask();
  matrix_.ShiftTask();
  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x14, 0x3E, 0x14}));
}

/* a new char or text starts at once, however recently the one before took a
 * step: '+' brings 0x08 first, beside the 0x14 of '#' */
TEST_F(ShiftPaceTest, NewShiftStartsAtOnce) {
  StartShift(1000);
  matrix_.ShiftTask();
  mock::SetMillis(1010);
  ASSERT_EQ(matrix_.SetCharWithShift('+').GetError(), Error::kOk);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_, 2), (std::vector<uint8_t>{0x14, 0x08}));
}

/* the interval is counted across the overflow of millis() after 49.7 days: a
 * step 50 ms after 0xFFFFFFF0 comes at 0x22, and none at 0xFFFFFFFF, where a
 * comparison with the sum, which wraps first, takes one (H-2) */
TEST_F(ShiftPaceTest, NoStepJustBeforeTheClockWraps) {
  StartShift(0xFFFFFFF0u);
  matrix_.ShiftTask();
  mock::SetMillis(0xFFFFFFFFu);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x00, 0x14}));
}

TEST_F(ShiftPaceTest, NoStepBeforeTheIntervalHasPassedAcrossTheWrap) {
  StartShift(0xFFFFFFF0u);
  matrix_.ShiftTask();
  mock::SetMillis(0x21u);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x00, 0x14}));
}

TEST_F(ShiftPaceTest, StepOnceTheIntervalHasPassedAcrossTheWrap) {
  StartShift(0xFFFFFFF0u);
  matrix_.ShiftTask();
  mock::SetMillis(0x22u);

  matrix_.ShiftTask();

  EXPECT_EQ(RightEdge(matrix_), (std::vector<uint8_t>{0x00, 0x14, 0x3E}));
}

TEST_F(MaxMatrixTest, ShutdownDarkensEveryModule) {
  ASSERT_EQ(matrix_.SetText("Hi").GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetShutdown(true).GetError(), Error::kOk);

  EXPECT_EQ(ChipValues(&Max7219::shutdown),
            std::vector<uint8_t>(kNumberOfModules,
                                 max7219::kShutdownModeShutdownMode));
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
  // the check after every test compares the LEDs with the buffer
  ASSERT_EQ(matrix_.SetShutdown(false).GetError(), Error::kOk);
}

TEST_F(MaxMatrixTest, DotDrawnInShutdownStaysDark) {
  ASSERT_EQ(matrix_.SetShutdown(true).GetError(), Error::kOk);

  matrix_.SetDot(Point{X{31}, Y{7}}, true);

  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
  // the check after every test compares the LEDs with the buffer
  ASSERT_EQ(matrix_.SetShutdown(false).GetError(), Error::kOk);
}

/* shutdown loses nothing: a dot drawn meanwhile shows once it ends */
TEST_F(MaxMatrixTest, DotDrawnInShutdownShowsWhenItEnds) {
  ASSERT_EQ(matrix_.SetShutdown(true).GetError(), Error::kOk);
  matrix_.SetDot(Point{X{31}, Y{7}}, true);

  ASSERT_EQ(matrix_.SetShutdown(false).GetError(), Error::kOk);

  EXPECT_EQ(Display(), (std::vector<uint8_t>{
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80}));
}

TEST_F(MaxMatrixTest, ShutdownKeepsTheIntensity) {
  ASSERT_EQ(matrix_.SetIntensity(9).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetShutdown(true).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetShutdown(false).GetError(), Error::kOk);

  EXPECT_EQ(ChipValues(&Max7219::intensity),
            std::vector<uint8_t>(kNumberOfModules, 9));
}

/* the display test lights every LED of every module at full brightness, even in
 * shutdown */
TEST_F(MaxMatrixTest, DisplayTestLightsEveryLedEvenInShutdown) {
  ASSERT_EQ(matrix_.SetShutdown(true).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetDisplayTest(true).GetError(), Error::kOk);

  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0xFF));
  // the check after every test compares the LEDs with the buffer
  ASSERT_EQ(matrix_.SetDisplayTest(false).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetShutdown(false).GetError(), Error::kOk);
}

TEST_F(MaxMatrixTest, EndOfTheDisplayTestLeavesAShutdownDark) {
  ASSERT_EQ(matrix_.SetText("Hi").GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetShutdown(true).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetDisplayTest(true).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetDisplayTest(false).GetError(), Error::kOk);

  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
  // the check after every test compares the LEDs with the buffer
  ASSERT_EQ(matrix_.SetShutdown(false).GetError(), Error::kOk);
}

TEST_F(MaxMatrixTest, EndOfTheDisplayTestShowsThePictureAgain) {
  ASSERT_EQ(matrix_.SetText("Hi").GetError(), Error::kOk);
  std::vector<uint8_t> const shown{Display()};
  ASSERT_EQ(matrix_.SetDisplayTest(true).GetError(), Error::kOk);

  ASSERT_EQ(matrix_.SetDisplayTest(false).GetError(), Error::kOk);

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
TEST_F(MatrixBeforeInitTest, InitClearsWhatWasDrawnBefore) {
  matrix_.SetDot(Point{X{3}, Y{2}}, true);

  Error const error{matrix_.Init().GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_FALSE(matrix_.GetDot(Point{X{3}, Y{2}}).GetValue());
  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

TEST_F(MatrixBeforeInitTest, ShiftTaskBeforeInitDoesNothing) {
  matrix_.ShiftTask();

  EXPECT_EQ(matrix_.GetState(), State::kNone);
  EXPECT_EQ(mock::frames, 0);
}

TEST_F(MatrixBeforeInitTest, StopShiftBeforeInitLeavesTheMatrixNotSetUp) {
  matrix_.StopShift();

  EXPECT_EQ(matrix_.GetState(), State::kNone);
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
