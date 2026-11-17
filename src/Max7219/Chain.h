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
#include "MaxMatrixAttributes.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "Utility/Byte.h"
#include "Utility/span.h"

namespace max_matrix {

/*!
 * \brief The registers of the MAX7219 and their values, as
 *        Matrix::RegisterWrite() takes them.
 */
namespace max7219 {

/*!
 * \brief The registers, by their addresses.
 *
 * A type of its own rather than a number, so that a value cannot take the place
 * of the address unnoticed (CodingStyle.md FN-16).
 */
enum class Register : uint8_t {
  /*! No-op: a module that is not addressed in a frame gets it. */
  kNoOp = 0x00,
  /*! Digit 0: the LEDs of one row or column of a module. */
  kDigit0 = 0x01,
  /*! Digit 1: the LEDs of one row or column of a module. */
  kDigit1 = 0x02,
  /*! Digit 2: the LEDs of one row or column of a module. */
  kDigit2 = 0x03,
  /*! Digit 3: the LEDs of one row or column of a module. */
  kDigit3 = 0x04,
  /*! Digit 4: the LEDs of one row or column of a module. */
  kDigit4 = 0x05,
  /*! Digit 5: the LEDs of one row or column of a module. */
  kDigit5 = 0x06,
  /*! Digit 6: the LEDs of one row or column of a module. */
  kDigit6 = 0x07,
  /*! Digit 7: the LEDs of one row or column of a module. */
  kDigit7 = 0x08,
  /*! Decode mode: BCD code B per digit, or none. */
  kDecodeMode = 0x09,
  /*! Intensity: the brightness, 0 to 15. */
  kIntensity = 0x0A,
  /*! Scan limit: how many digits, here rows or columns, are lit. */
  kScanLimit = 0x0B,
  /*! Shutdown: off, or normal operation. */
  kShutdown = 0x0C,
  /*! Display test: every LED on, or normal operation. */
  kDisplayTest = 0x0F
};

/* the values of the decode mode register */
/*! No digit decoded: each bit is an LED. */
constexpr uint8_t kDecodeModeNoDecode{0x00};
/*! Code B for digit 0, none for 7 to 1. */
constexpr uint8_t kDecodeModeCodeBDecodeDigit0{0x01};
/*! Code B for digits 3 to 0, none for 7 to 4. */
constexpr uint8_t kDecodeModeCodeBDecodeDigit3To0{0x0F};
/*! Code B for every digit. */
constexpr uint8_t kDecodeModeCodeBDecodeDigit7To0{0xFF};

/* the values of the intensity register */
/*! The lowest brightness, 1/32 duty cycle. */
constexpr uint8_t kIntensityMinValue{0x00};
/*! The highest brightness, 31/32 duty cycle. */
constexpr uint8_t kIntensityMaxValue{0x0F};

/* the values of the scan limit register */
/*! Digit 0 alone is lit. */
constexpr uint8_t kScanLimitDisplayDigit0{0x00};
/*! Digits 0 to 1 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To1{0x01};
/*! Digits 0 to 2 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To2{0x02};
/*! Digits 0 to 3 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To3{0x03};
/*! Digits 0 to 4 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To4{0x04};
/*! Digits 0 to 5 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To5{0x05};
/*! Digits 0 to 6 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To6{0x06};
/*! Digits 0 to 7 are lit. */
constexpr uint8_t kScanLimitDisplayDigit0To7{0x07};

/* the values of the shutdown register */
/*! Shut down: the LEDs off, the registers kept. */
constexpr uint8_t kShutdownModeShutdownMode{0x00};
/*! Normal operation. */
constexpr uint8_t kShutdownModeNormalOperation{0x01};

/* the values of the display test register */
/*! Normal operation. */
constexpr uint8_t kDisplayTestNormalOperation{0x00};
/*! Display test: every LED on at full brightness. */
constexpr uint8_t kDisplayTestDisplayTestMode{0x01};

}  // namespace max7219

/*
 * The three pins of the chain, a type each, so that a sketch cannot swap them
 * unnoticed (CodingStyle.md FN-16). Explicit, as a bare number would be any of
 * them again.
 */

/*!
 * \brief The pin DIN of the chain is connected to: `max_matrix::DataInPin{11}`.
 */
struct DataInPin {
  /*!
   * \brief Names the pin.
   *
   * \param[in] pin The pin's number, as `pinMode()` takes it.
   */
  constexpr explicit DataInPin(uint8_t pin) : number{pin} {}

  uint8_t number; /*!< The pin's number, as `pinMode()` takes it. */
};

/*!
 * \brief The pin CS of the chain is connected to:
 *        `max_matrix::ChipSelectPin{10}`.
 */
struct ChipSelectPin {
  /*!
   * \brief Names the pin.
   *
   * \param[in] pin The pin's number, as `pinMode()` takes it.
   */
  constexpr explicit ChipSelectPin(uint8_t pin) : number{pin} {}

  uint8_t number; /*!< The pin's number, as `pinMode()` takes it. */
};

/*!
 * \brief The pin CLK of the chain is connected to: `max_matrix::ClockPin{13}`.
 */
struct ClockPin {
  /*!
   * \brief Names the pin.
   *
   * \param[in] pin The pin's number, as `pinMode()` takes it.
   */
  constexpr explicit ClockPin(uint8_t pin) : number{pin} {}

  uint8_t number; /*!< The pin's number, as `pinMode()` takes it. */
};

namespace internal {

/*!
 * \brief The chain of MAX7219s: the pins, and the frames sent to them.
 *
 * A frame carries one address and value per module. Only this class touches the
 * pins; bit banging or SPI is chosen with `MAXMATRIX_USE_SPI`.
 */
class Chain {
 public:
  /*!
   * \brief Makes the chain at these pins; nothing is sent until Init().
   *
   * \param[in] data_in_pin The pin DIN of the chain is connected to.
   * \param[in] chip_select_pin The pin CS of the chain is connected to.
   * \param[in] clock_pin The pin CLK of the chain is connected to.
   */
  Chain(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
        ClockPin clock_pin);

  /*! \brief Returns the pin DIN of the chain is connected to. */
  MAXMATRIX_NODISCARD uint8_t GetDataInPin() const { return data_in_pin_; }

  /*! \brief Returns the pin CS of the chain is connected to. */
  MAXMATRIX_NODISCARD uint8_t GetChipSelectPin() const {
    return chip_select_pin_;
  }

  /*! \brief Returns the pin CLK of the chain is connected to. */
  MAXMATRIX_NODISCARD uint8_t GetClockPin() const { return clock_pin_; }

  /*!
   * \brief Sets the pin DIN of the chain is connected to.
   *
   * \param[in] data_in_pin The pin; the next Init() makes it an output and
   *                        takes it.
   */
  void SetDataInPin(uint8_t data_in_pin) { data_in_pin_ = data_in_pin; }

  /*!
   * \brief Sets the pin CS of the chain is connected to.
   *
   * \param[in] chip_select_pin The pin; the next Init() makes it an output and
   *                            takes it.
   */
  void SetChipSelectPin(uint8_t chip_select_pin) {
    chip_select_pin_ = chip_select_pin;
  }

  /*!
   * \brief Sets the pin CLK of the chain is connected to.
   *
   * \param[in] clock_pin The pin; the next Init() makes it an output and takes
   *                      it.
   */
  void SetClockPin(uint8_t clock_pin) { clock_pin_ = clock_pin; }

  /*!
   * \brief Sets the pins up, and every MAX7219 to drive an 8x8 matrix.
   *
   * All eight digits, no decoding, normal operation, no display test.
   *
   * \param[in] number_of_modules The MAX7219s in the chain.
   * \return Error::kOk, or Error::kOutOfRange if a pin is none of the board's,
   *         `NUM_DIGITAL_PINS` or above; then nothing is set up, and the chain
   *         drives the pins of the last Init() that succeeded.
   */
  MAXMATRIX_NODISCARD Status Init(uint8_t number_of_modules);

  /*!
   * \brief Writes the same register of every MAX7219, with the same value.
   *
   * One frame, with the same address and value for every module.
   *
   * \param[in] address The register.
   * \param[in] value The value.
   * \param[in] number_of_modules The MAX7219s in the chain.
   */
  void WriteRegisterAll(max7219::Register address, uint8_t value,
                        uint8_t number_of_modules);

  /*!
   * \brief Writes the same register of every MAX7219, each with its own value:
   *        one column of every module, straight from the matrix buffer.
   *
   * One frame, with the same address and one value per module. The values of
   * a column lie a module's columns apart in the buffer.
   *
   * \param[in] address The register.
   * \param[in] buffer The matrix buffer, `BufferSize()` of the modules in the
   *                   chain, module 0 first.
   * \param[in] column The column of a module to write, 0 to 7.
   */
  void WriteRegisterEach(max7219::Register address, span<Byte const> buffer,
                         uint8_t column);

 private:
  /* a pin's port and its bit there, as one, so that the two cannot be mixed up
   * between pins or swapped in a call (FN-16) */
  struct PortBit {
    uint8_t port;
    uint8_t mask;
  };

  /*
   * Not const, though they change no member: they change the chain this object
   * stands for, and a const Chain that still sends would be a lie.
   * transfer() is not static either, though with SPI it uses no member: bit
   * banging needs the pins. clang-tidy reports all three, and each definition
   * answers it with a NOLINT.
   */
  void StartTransmission();
  void StopTransmission();
  void Transfer(max7219::Register address, uint8_t value);
#if (MAXMATRIX_USE_SPI == STD_OFF)
  void ShiftOut(uint8_t value);
#endif
  static PortBit LookUpPortBit(uint8_t pin);
  static void WritePin(PortBit pin, bool high);
  // Whether each of the three pins is one of the board's, which Init() checks
  // before it takes them.
  MAXMATRIX_NODISCARD bool HasBoardPins() const;

  uint8_t data_in_pin_;
  uint8_t chip_select_pin_;
  uint8_t clock_pin_;
  /*
   * The port and the bit of each pin, looked up once in Init(): digitalWrite()
   * looks both up in tables in flash on every call, which made a byte bit
   * banged by shiftOut() take about 1500 cycles of the Uno's 16 MHz. The port,
   * not its register: the core reads the register's address from flash with a
   * statement expression, which decltype() cannot take outside a function, so
   * it is read where it is written. Data in and clock only bit bang.
   */
  PortBit chip_select_{};
#if (MAXMATRIX_USE_SPI == STD_OFF)
  PortBit data_in_{};
  PortBit clock_{};
#endif
};

}  // namespace internal
}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAX7219_CHAIN_H_
