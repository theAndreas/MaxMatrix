// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The transfer to the chain: the pins it takes, and every bit within a frame.
 */

#include <gtest/gtest.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Arduino.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

uint8_t const kLastPin{NUM_DIGITAL_PINS - 1};
uint8_t const kBeyondPin{NUM_DIGITAL_PINS};

/* the three pins of a chain, one of them at the edge of the board's */
struct PinCase {
  char const* name;
  uint8_t data_in;
  uint8_t chip_select;
  uint8_t clock;
};

class ChainPinTest : public MaxMatrixTest,
                     public ::testing::WithParamInterface<PinCase> {};

class ChainLastPinTest : public ChainPinTest {};

/* the name of a case, after the pin at the edge */
std::string PinCaseName(::testing::TestParamInfo<PinCase> const& param_info) {
  return param_info.param.name;
}

/* chip select was not an output in SPI mode, and the SPI transaction was never
 * ended */
TEST_F(MaxMatrixTest, TransfersAreFramed) {
  char const text[] = "Hello";

  EXPECT_EQ(mock::pin_mode[kChipSelectPin], OUTPUT);
  matrix_.SetText("Hi!");
  matrix_.SetModuleType(ModuleType::kIcstation);
  ShiftWholeText(text);

  EXPECT_GT(mock::frames, 0);
  EXPECT_EQ(mock::errors, 0);
  EXPECT_EQ(mock::pin_level[kChipSelectPin], HIGH);
  EXPECT_FALSE(mock::transaction_open);
  EXPECT_EQ(mock::UsesSpi(), MAXMATRIX_USE_SPI == STD_ON);
}

/* Init() took a pin beyond the board's: the core looks a pin up in tables of
 * NUM_DIGITAL_PINS entries without a check, so that the bit banging wrote to
 * whatever address the bytes past them made, on the Uno pin 20 to address 0,
 * the register r0, and pin 21 to PORTB */
TEST_P(ChainPinTest, InitRefusesAPinBeyondTheBoard) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  MatrixOf<kNumberOfModules> matrix{DataInPin{GetParam().data_in},
                                    ChipSelectPin{GetParam().chip_select},
                                    ClockPin{GetParam().clock}};

  Error const error{matrix.Init().GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(matrix.GetState(), State::kNone);
  EXPECT_EQ(mock::frames, 0);
  EXPECT_EQ(mock::pin_mode[kDataInPin], INPUT);
  EXPECT_EQ(mock::pin_mode[kChipSelectPin], INPUT);
  EXPECT_EQ(mock::pin_mode[kClockPin], INPUT);
}

INSTANTIATE_TEST_SUITE_P(
    EachPin, ChainPinTest,
    ::testing::Values(PinCase{"DataIn", kBeyondPin, kChipSelectPin, kClockPin},
                      PinCase{"ChipSelect", kDataInPin, kBeyondPin, kClockPin},
                      PinCase{"Clock", kDataInPin, kChipSelectPin, kBeyondPin}),
    PinCaseName);

/* the last pin of the board is one of it */
TEST_P(ChainLastPinTest, InitTakesTheLastPinOfTheBoard) {
  mock::Reset(GetParam().data_in, GetParam().chip_select, GetParam().clock,
              kNumberOfModules);
  MatrixOf<kNumberOfModules> matrix{DataInPin{GetParam().data_in},
                                    ChipSelectPin{GetParam().chip_select},
                                    ClockPin{GetParam().clock}};

  Error const error{matrix.Init().GetError()};

  EXPECT_EQ(error, Error::kOk);
  EXPECT_EQ(matrix.GetState(), State::kReady);
  EXPECT_EQ(ChipOfModule(0).intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
}

INSTANTIATE_TEST_SUITE_P(
    EachPin, ChainLastPinTest,
    ::testing::Values(PinCase{"DataIn", kLastPin, kChipSelectPin, kClockPin},
                      PinCase{"ChipSelect", kDataInPin, kLastPin, kClockPin},
                      PinCase{"Clock", kDataInPin, kChipSelectPin, kLastPin}),
    PinCaseName);

/* a failed Init() keeps the pins the chain was set up with, and the matrix
 * draws on them as before */
TEST_F(MaxMatrixTest, FailedInitKeepsThePinsItHad) {
  matrix_.SetDataInPin(kBeyondPin);

  Error const error{matrix_.Init().GetError()};
  matrix_.SetText("Hi!");

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  EXPECT_NE(Display(), std::vector<uint8_t>(kNumberOfColumns, 0));
}

/* a matrix whose Init() failed sent what it drew to a chain that was never set
 * up: with SPI, the Uno waited for ever on a byte before SPI.begin(), and bit
 * banging wrote the register of the core's NOT_A_PORT, address 0, the register
 * r0 */
TEST_F(MaxMatrixTest, NothingIsSentBeforeInitSucceeded) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  MatrixOf<kNumberOfModules> matrix{DataInPin{kBeyondPin},
                                    ChipSelectPin{kChipSelectPin},
                                    ClockPin{kClockPin}};
  Error const error{matrix.Init().GetError()};

  matrix.SetDot(Point{X{0}, Y{0}}, true);
  /* a frame for every module at once, as SetIntensity() sent one before it
   * refused a matrix not set up itself */
  matrix.RegisterWrite(max7219::Register::kIntensity, 1);
  matrix.Update();

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(mock::frames, 0);
  EXPECT_EQ(mock::errors, 0);
}

}  // namespace
}  // namespace max_matrix
