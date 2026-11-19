// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The transfer to the chain: the pins it takes, and every bit within a frame.
 */

#include <gtest/gtest.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Arduino.h"
#include "Max7219.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

uint8_t const kLastPin{NUM_DIGITAL_PINS - 1};
uint8_t const kBeyondPin{NUM_DIGITAL_PINS};

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class ChainTest : public MaxMatrixTest {
 protected:
  /* a text drawn, a module type changed, and a text shifted to its end: every
   * kind of transfer the matrix makes */
  void Transfer() {
    matrix_.SetText("Hi!");
    matrix_.SetModuleType(ModuleType::kIcstation);
    ASSERT_EQ(matrix_.SetTextWithShift("Hello").GetError(), Error::kOk);
    ShiftUntilReady(matrix_);
  }
};

TEST_F(ChainTest, ChipSelectIsAnOutput) {
  uint8_t const mode{mock::pin_mode[kChipSelectPin]};

  EXPECT_EQ(mode, OUTPUT);
}

/* chip select was not an output in SPI mode, and the SPI transaction was never
 * ended */
TEST_F(ChainTest, ChipSelectIsHighAfterTheTransfers) {
  Transfer();

  EXPECT_EQ(mock::pin_level[kChipSelectPin], HIGH);
}

TEST_F(ChainTest, NoTransactionIsLeftOpen) {
  Transfer();

  EXPECT_FALSE(mock::transaction_open);
}

TEST_F(ChainTest, TransfersGoThroughSpiWhenItIsConfigured) {
  Transfer();

  EXPECT_EQ(mock::UsesSpi(), MAXMATRIX_USE_SPI == STD_ON);
}

/* the three pins of a chain, one of them at the edge of the board's */
struct PinCase {
  char const* name;
  uint8_t data_in;
  uint8_t chip_select;
  uint8_t clock;
};

/* a matrix on the pins of a case, beside the fixture's, on a chain reset */
class PinTest : public ChainTest,
                public ::testing::WithParamInterface<PinCase> {
 protected:
  MatrixOf<kNumberOfModules> pinned_{DataInPin{GetParam().data_in},
                                     ChipSelectPin{GetParam().chip_select},
                                     ClockPin{GetParam().clock}};
};

/* Init() took a pin beyond the board's: the core looks a pin up in tables of
 * NUM_DIGITAL_PINS entries without a check, so that the bit banging wrote to
 * whatever address the bytes past them made, on the Uno pin 20 to address 0,
 * the register r0, and pin 21 to PORTB */
class PinBeyondTheBoardTest : public PinTest {
 protected:
  void SetUp() override {
    PinTest::SetUp();
    mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  }
};

INSTANTIATE_TEST_SUITE_P(
    EachPin, PinBeyondTheBoardTest,
    ::testing::Values(PinCase{"DataIn", kBeyondPin, kChipSelectPin, kClockPin},
                      PinCase{"ChipSelect", kDataInPin, kBeyondPin, kClockPin},
                      PinCase{"Clock", kDataInPin, kChipSelectPin, kBeyondPin}),
    CaseName<PinCase>);

TEST_P(PinBeyondTheBoardTest, IsRefusedByInit) {
  Error const error{pinned_.Init().GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
}

TEST_P(PinBeyondTheBoardTest, LeavesTheMatrixNotSetUp) {
  pinned_.Init();

  EXPECT_EQ(pinned_.GetState(), State::kNone);
}

TEST_P(PinBeyondTheBoardTest, SendsNothing) {
  pinned_.Init();

  EXPECT_EQ(mock::frames, 0);
}

TEST_P(PinBeyondTheBoardTest, LeavesEveryPinAnInput) {
  pinned_.Init();

  EXPECT_EQ(mock::pin_mode[kDataInPin], INPUT);
  EXPECT_EQ(mock::pin_mode[kChipSelectPin], INPUT);
  EXPECT_EQ(mock::pin_mode[kClockPin], INPUT);
}

/* the last pin of the board is one of it */
class LastPinTest : public PinTest {
 protected:
  void SetUp() override {
    PinTest::SetUp();
    mock::Reset(GetParam().data_in, GetParam().chip_select, GetParam().clock,
                kNumberOfModules);
  }
};

INSTANTIATE_TEST_SUITE_P(
    EachPin, LastPinTest,
    ::testing::Values(PinCase{"DataIn", kLastPin, kChipSelectPin, kClockPin},
                      PinCase{"ChipSelect", kDataInPin, kLastPin, kClockPin},
                      PinCase{"Clock", kDataInPin, kChipSelectPin, kLastPin}),
    CaseName<PinCase>);

TEST_P(LastPinTest, IsTakenByInit) {
  Error const error{pinned_.Init().GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(pinned_.GetState(), State::kReady);
}

/* Init() sends the intensity it starts with, 5, to every chip */
TEST_P(LastPinTest, CarriesTheTransfers) {
  ASSERT_EQ(pinned_.Init().GetError(), Error::kOk);

  EXPECT_EQ(ChipValues(&Max7219::intensity),
            (std::vector<uint8_t>{5, 5, 5, 5}));
}

/* a failed Init() keeps the pins the chain was set up with */
TEST_F(ChainTest, InitAfterADataInPinBeyondTheBoardIsRefused) {
  matrix_.SetDataInPin(kBeyondPin);

  Error const error{matrix_.Init().GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
}

TEST_F(ChainTest, FailedInitLeavesTheMatrixReady) {
  matrix_.SetDataInPin(kBeyondPin);

  matrix_.Init();

  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* "Hi!" is 7F 08 08 7F, the space, 44 7D 40, the space, 5F */
TEST_F(ChainTest, MatrixDrawsOnTheOldPinsAfterAFailedInit) {
  matrix_.SetDataInPin(kBeyondPin);
  matrix_.Init();

  matrix_.SetText("Hi!");

  EXPECT_EQ(LeftEdge(matrix_, 10),
            (std::vector<uint8_t>{0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D,
                                  0x40, 0x00, 0x5F}));
}

/* a matrix whose Init() failed sent what it drew to a chain that was never set
 * up: with SPI, the Uno waited for ever on a byte before SPI.begin(), and bit
 * banging wrote the register of the core's NOT_A_PORT, address 0, the register
 * r0 */
TEST_F(ChainTest, NothingIsSentBeforeInitSucceeded) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  MatrixOf<kNumberOfModules> matrix{DataInPin{kBeyondPin},
                                    ChipSelectPin{kChipSelectPin},
                                    ClockPin{kClockPin}};
  ASSERT_EQ(matrix.Init().GetError(), Error::kOutOfRange);

  matrix.SetDot(Point{X{0}, Y{0}}, true);
  /* a frame for every module at once, as SetIntensity() sent one before it
   * refused a matrix not set up itself */
  matrix.RegisterWrite(max7219::Register::kIntensity, 1);
  matrix.Update();

  EXPECT_EQ(mock::frames, 0);
}

TEST_F(ChainTest, PinsAreMadeWhenCompiling) {
  static_assert(DataInPin{11}.number == 11, "data in");
  static_assert(ChipSelectPin{10}.number == 10, "chip select");
  static_assert(ClockPin{13}.number == 13, "clock");

  EXPECT_EQ(ClockPin{13}.number, 13);
}

}  // namespace
}  // namespace max_matrix
