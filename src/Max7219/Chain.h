// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Transfer to the chain of MAX7219.
 *
 * Writes registers of the chained MAX7219, by bit banging or by hardware SPI.
 * Knows nothing of LEDs.
 */

#ifndef MAXMATRIX_SRC_MAX7219_CHAIN_H_
#define MAXMATRIX_SRC_MAX7219_CHAIN_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"

namespace max_matrix {

/*!
 * \brief The registers of the MAX7219 and their values, as
 *        Matrix::RegisterWrite() takes them.
 */
namespace max7219 {

/* the addresses of the registers */
/*! No-op: a module that is not addressed in a frame gets it. */
constexpr byte kNoOpAddress{0x00};
/*! Digit 0: the LEDs of one row or column of a module. */
constexpr byte kDigit0Address{0x01};
/*! Digit 1: the LEDs of one row or column of a module. */
constexpr byte kDigit1Address{0x02};
/*! Digit 2: the LEDs of one row or column of a module. */
constexpr byte kDigit2Address{0x03};
/*! Digit 3: the LEDs of one row or column of a module. */
constexpr byte kDigit3Address{0x04};
/*! Digit 4: the LEDs of one row or column of a module. */
constexpr byte kDigit4Address{0x05};
/*! Digit 5: the LEDs of one row or column of a module. */
constexpr byte kDigit5Address{0x06};
/*! Digit 6: the LEDs of one row or column of a module. */
constexpr byte kDigit6Address{0x07};
/*! Digit 7: the LEDs of one row or column of a module. */
constexpr byte kDigit7Address{0x08};
/*! Decode mode: BCD code B per digit, or none. */
constexpr byte kDecodeModeAddress{0x09};
/*! Intensity: the brightness, 0 to 15. */
constexpr byte kIntensityAddress{0x0A};
/*! Scan limit: how many digits, here rows or columns, are lit. */
constexpr byte kScanLimitAddress{0x0B};
/*! Shutdown: off, or normal operation. */
constexpr byte kShutdownAddress{0x0C};
/*! Display test: every LED on, or normal operation. */
constexpr byte kDisplayTestAddress{0x0F};

/* the values of the decode mode register */
/*! No digit decoded: each bit is an LED. */
constexpr byte kDecodeModeNoDecode{0x00};
/*! Code B for digit 0, none for 7 to 1. */
constexpr byte kDecodeModeCodeBDecodeDigit0{0x01};
/*! Code B for digits 3 to 0, none for 7 to 4. */
constexpr byte kDecodeModeCodeBDecodeDigit3To0{0x0F};
/*! Code B for every digit. */
constexpr byte kDecodeModeCodeBDecodeDigit7To0{0xFF};

/* the values of the intensity register */
/*! The lowest brightness, 1/32 duty cycle. */
constexpr byte kIntensityMinValue{0x00};
/*! The highest brightness, 31/32 duty cycle. */
constexpr byte kIntensityMaxValue{0x0F};

/* the values of the scan limit register */
/*! Digit 0 alone is lit. */
constexpr byte kScanLimitDisplayDigit0{0x00};
/*! Digits 0 to 1 are lit. */
constexpr byte kScanLimitDisplayDigit0To1{0x01};
/*! Digits 0 to 2 are lit. */
constexpr byte kScanLimitDisplayDigit0To2{0x02};
/*! Digits 0 to 3 are lit. */
constexpr byte kScanLimitDisplayDigit0To3{0x03};
/*! Digits 0 to 4 are lit. */
constexpr byte kScanLimitDisplayDigit0To4{0x04};
/*! Digits 0 to 5 are lit. */
constexpr byte kScanLimitDisplayDigit0To5{0x05};
/*! Digits 0 to 6 are lit. */
constexpr byte kScanLimitDisplayDigit0To6{0x06};
/*! Digits 0 to 7 are lit. */
constexpr byte kScanLimitDisplayDigit0To7{0x07};

/* the values of the shutdown register */
/*! Shut down: the LEDs off, the registers kept. */
constexpr byte kShutdownModeShutdownMode{0x00};
/*! Normal operation. */
constexpr byte kShutdownModeNormalOperation{0x01};

/* the values of the display test register */
/*! Normal operation. */
constexpr byte kDisplayTestNormalOperation{0x00};
/*! Display test: every LED on at full brightness. */
constexpr byte kDisplayTestDisplayTestMode{0x01};

}  // namespace max7219

namespace internal {

/*!
 * \brief The chain of MAX7219s: the pins, and the frames sent to them.
 *
 * A frame carries one address and value per module. Only this class touches the
 * pins; bit banging or SPI is chosen with `MAXMATRIX_USE_SPI`.
 */
class Chain {
 private:
  byte data_in_pin_;
  byte chip_select_pin_;
  byte clock_pin_;
  byte number_of_modules_;
  /*
   * The port and the bit of each pin, looked up once in Init(): digitalWrite()
   * looks both up in tables in flash on every call, which made a byte bit
   * banged by shiftOut() take about 1500 cycles of the Uno's 16 MHz. The port,
   * not its register: the core reads the register's address from flash with a
   * statement expression, which decltype() cannot take outside a function, so
   * it is read where it is written. Data in and clock only bit bang.
   */
  byte chip_select_port_;
  byte chip_select_mask_;
#if (MAXMATRIX_USE_SPI == STD_OFF)
  byte data_in_port_;
  byte data_in_mask_;
  byte clock_port_;
  byte clock_mask_;
#endif

  /*
   * Not const, though they change no member: they change the chain this object
   * stands for, and a const Chain that still sends would be a lie.
   * transfer() is not static either, though with SPI it uses no member: bit
   * banging needs the pins. clang-tidy reports all three, and each definition
   * answers it with a NOLINT.
   */
  void StartTransmission();
  void StopTransmission();
  void Transfer(byte register_address, byte register_value);
#if (MAXMATRIX_USE_SPI == STD_OFF)
  void ShiftOut(byte value);
#endif
  static void WritePin(byte port, byte mask, bool high);

 public:
  /*!
   * \brief Makes the chain at these pins; nothing is sent until Init().
   *
   * \param[in] data_in_pin The pin DIN of the chain is connected to.
   * \param[in] chip_select_pin The pin CS of the chain is connected to.
   * \param[in] clock_pin The pin CLK of the chain is connected to.
   * \param[in] number_of_modules The MAX7219s in the chain, 1 to
   *                              `kMaxNumberOfModules`.
   */
  Chain(byte data_in_pin, byte chip_select_pin, byte clock_pin,
        byte number_of_modules);

  /*! \brief Returns the MAX7219s in the chain. */
  byte GetNumberOfModules() const { return number_of_modules_; }

  /*! \brief Returns the pin DIN of the chain is connected to. */
  byte GetDataInPin() const { return data_in_pin_; }

  /*! \brief Returns the pin CS of the chain is connected to. */
  byte GetChipSelectPin() const { return chip_select_pin_; }

  /*! \brief Returns the pin CLK of the chain is connected to. */
  byte GetClockPin() const { return clock_pin_; }

  /*!
   * \brief Sets the pin DIN of the chain is connected to.
   *
   * \param[in] data_in_pin The pin; the next Init() makes it an output and
   *                        takes it.
   */
  void SetDataInPin(byte data_in_pin) { data_in_pin_ = data_in_pin; }

  /*!
   * \brief Sets the pin CS of the chain is connected to.
   *
   * \param[in] chip_select_pin The pin; the next Init() makes it an output and
   *                            takes it.
   */
  void SetChipSelectPin(byte chip_select_pin) {
    chip_select_pin_ = chip_select_pin;
  }

  /*!
   * \brief Sets the pin CLK of the chain is connected to.
   *
   * \param[in] clock_pin The pin; the next Init() makes it an output and takes
   *                      it.
   */
  void SetClockPin(byte clock_pin) { clock_pin_ = clock_pin; }

  /*!
   * \brief Sets the pins up, and every MAX7219 to drive an 8x8 matrix.
   *
   * All eight digits, no decoding, normal operation, no display test.
   */
  void Init();

  /*!
   * \brief Writes the same register of every MAX7219, with the same value.
   *
   * One frame, with the same address and value for every module.
   *
   * \param[in] register_address The address of the register, one of
   *                             `MAX7219_REG_..._ADDRESS`.
   * \param[in] register_value The value.
   */
  void WriteRegisterAll(byte register_address, byte register_value);

  /*!
   * \brief Writes the same register of every MAX7219, each with its own value.
   *
   * One frame, with the same address and one value per module.
   *
   * \param[in] register_address The address of the register, one of
   *                             `MAX7219_REG_..._ADDRESS`.
   * \param[in] register_values One value per module, module 0 first.
   * \param[in] stride The bytes from one module's value to the next: 1 for an
   *                   array of values, or the columns of a module to take one
   *                   column of every module straight from the matrix buffer.
   */
  void WriteRegisterEach(byte register_address, byte const* register_values,
                         byte stride);
};

}  // namespace internal
}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAX7219_CHAIN_H_
