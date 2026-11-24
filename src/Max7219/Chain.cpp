// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Transfer to the chain of MAX7219.
 *
 * Every module in the chain gets one address/value pair per frame, module 0
 * first; a module that is not addressed gets the no-op. The bytes go out
 * through Transfer(), the only place that knows whether they are bit banged or
 * sent by hardware SPI - before, every function that sent a frame had its own
 * loop with its own \#if.
 */

#include "Chain.h"

#include <stdint.h>
#if (MAXMATRIX_USE_PORT_REGISTERS == STD_ON)
#include <util/atomic.h>
#endif

#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "MaxMatrixStandardTypes.h"
#include "Utility/Byte.h"
#include "Utility/span.h"
#if (MAXMATRIX_USE_SPI == STD_ON)
#include "SPI.h"
#endif
#include "Utility/Bit.h"

namespace max_matrix {
namespace internal {

namespace {

// The core looks a pin up in tables of NUM_DIGITAL_PINS entries without a
// check: a pin beyond them takes the bytes that follow for its port, and a
// write goes to whatever address they make.
constexpr bool IsBoardPin(uint8_t pin) { return pin < NUM_DIGITAL_PINS; }

#if (MAXMATRIX_USE_SPI == STD_OFF)
/* the MAX7219 takes a byte most significant bit first */
constexpr uint8_t kFirstBit{0x80};
#endif

}  // namespace

#if (MAXMATRIX_USE_SPI == STD_ON)
Chain::Chain(SPIClass& spi, ChipSelectPin chip_select_pin)
    : chip_select_pin_{chip_select_pin.number}, spi_{spi} {}

// The bus drives data in and clock at pins of its own; the two are taken only
// so that a sketch builds with bit banging and with SPI alike.
Chain::Chain(DataInPin /*data_in_pin*/, ChipSelectPin chip_select_pin,
             ClockPin /*clock_pin*/)
    : Chain{SPI, chip_select_pin} {}
#else
Chain::Chain(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
             ClockPin clock_pin)
    : chip_select_pin_{chip_select_pin.number},
      data_in_pin_{data_in_pin.number},
      clock_pin_{clock_pin.number} {}
#endif

Status Chain::Init(uint8_t number_of_modules) {
  if (!HasBoardPins()) {
    return Error::kOutOfRange;
  }

  pinMode(chip_select_pin_, OUTPUT);
  // A local first: read back from the member, it cost 8 bytes of flash in
  // every example with SPI.
  OutputPin const chip_select{LookUpOutputPin(chip_select_pin_)};
  chip_select_ = chip_select;
  WritePin(chip_select, true);
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the sketch's bus; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  spi_.get().begin();
#else
  pinMode(data_in_pin_, OUTPUT);
  pinMode(clock_pin_, OUTPUT);
  data_in_ = LookUpOutputPin(data_in_pin_);
  clock_ = LookUpOutputPin(clock_pin_);
  /* clock idles low, MAX7219 samples data on the rising edge */
  WritePin(clock_, false);
#endif

  WriteRegisterAll(max7219::Register::kScanLimit,
                   max7219::kScanLimitDisplayDigit0To7,  // display all digits
                   number_of_modules);
  WriteRegisterAll(max7219::Register::kDecodeMode,
                   max7219::kDecodeModeNoDecode,  // an LED matrix, not digits
                   number_of_modules);
  WriteRegisterAll(max7219::Register::kShutdown,
                   max7219::kShutdownModeNormalOperation,  // normal operation
                   number_of_modules);
  WriteRegisterAll(max7219::Register::kDisplayTest,
                   max7219::kDisplayTestNormalOperation,  // no display test
                   number_of_modules);
  return Error::kOk;
}

// FN-16: private; Init() alone calls it, with a named value and the chain's
// length
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void Chain::WriteRegisterAll(max7219::Register address, uint8_t value,
                             uint8_t number_of_modules) {
  if (!IsSetUp()) {
    return;
  }
  StartTransmission();
  for (uint8_t module{0}; module < number_of_modules; module++) {
    Transfer(address, value);
  }
  StopTransmission();
}

void Chain::WriteRegisterEach(max7219::Register address,
                              span<Byte const> buffer, uint8_t column) {
  if (!IsSetUp()) {
    return;
  }
  // A byte, as the buffer of at most kMaxNumberOfModules modules is: the index
  // then steps on in 8 bits, where the Uno would compute a size_t in 16.
  auto const columns{static_cast<uint8_t>(buffer.size())};

  StartTransmission();
  for (uint8_t index{column}; index < columns; index += kColumnNumberOfModule) {
    Transfer(address, buffer[index].ToInteger());
  }
  StopTransmission();
}

// Starts a frame: pulls chip select low, and in SPI mode reserves the bus.
// StopTransmission() has to follow once the frame is transferred. Not const: it
// changes the chain this object stands for, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::StartTransmission() {
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the sketch's bus; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  spi_.get().beginTransaction(
      SPISettings(MAXMATRIX_SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE0));
#endif
  WritePin(chip_select_, false);
}

/*
 * Ends a frame: pulls chip select high, so that every MAX7219 latches what it
 * was sent, and in SPI mode releases the bus for other devices.
 * StartTransmission() has to come first.
 */
// Not const: it changes the chain this object stands for, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::StopTransmission() {
  WritePin(chip_select_, true);
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the sketch's bus; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  spi_.get().endTransaction();
#endif
}

// Sends the address and value of one module within a frame; StartTransmission()
// has to come first. Not const: it changes the chain, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::Transfer(max7219::Register address, uint8_t value) {
  auto const address_byte{static_cast<uint8_t>(address)};
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the sketch's bus; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  spi_.get().transfer(address_byte);
  // the sketch's bus; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  spi_.get().transfer(value);
#else
  ShiftOut(address_byte);
  ShiftOut(value);
#endif
}

#if (MAXMATRIX_USE_SPI == STD_OFF)
#if (MAXMATRIX_USE_PORT_REGISTERS == STD_ON)
/*
 * Bit bangs `value`, most significant bit first: data in, then a rising clock,
 * on which the MAX7219 takes the bit, and the clock low again. The interrupts
 * are held off for the byte, as digitalWrite() holds them off for a pin: an
 * interrupt that wrote another pin of the same port between the read and the
 * write of |= would be undone. A byte takes about 320 cycles, 20 us, in simavr,
 * against about 1500 through shiftOut().
 */
// Not const: it changes the chain this object stands for, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::ShiftOut(uint8_t value) {
  /*
   * The core's portOutputRegister() makes the register's address, a number read
   * from flash, a pointer: clang-tidy reports it on the Uno, where there is no
   * other way to a port. T-8: auto, as the register is a volatile uint8_t on
   * the board and the mock's object of its own on the host.
   */
  auto* const data_in_register{portOutputRegister(
      data_in_.port)}; /* NOLINT(performance-no-int-to-ptr) */
  auto* const clock_register{
      portOutputRegister(clock_.port)}; /* NOLINT(performance-no-int-to-ptr) */

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    uint8_t bit{kFirstBit};
    while (bit != 0u) {
      if (Bit<uint8_t>::ReadBitGroup(value, bit) != 0u) {
        Bit<uint8_t>::SetBits(*data_in_register, data_in_.mask);
      } else {
        Bit<uint8_t>::ClearBits(*data_in_register, data_in_.mask);
      }
      Bit<uint8_t>::SetBits(*clock_register, clock_.mask);
      Bit<uint8_t>::ClearBits(*clock_register, clock_.mask);
      bit = Bit<uint8_t>::ShiftDown(bit, 1u);
    }
  }
}
#else
// Bit bangs `value` as above, a pin at a time through the core, which holds
// the interrupts off itself where its board needs it.
// Not const: it changes the chain this object stands for, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::ShiftOut(uint8_t value) {
  uint8_t bit{kFirstBit};
  while (bit != 0u) {
    bool const high{Bit<uint8_t>::ReadBitGroup(value, bit) != 0u};
    WritePin(data_in_, high);
    WritePin(clock_, true);
    WritePin(clock_, false);
    bit = Bit<uint8_t>::ShiftDown(bit, 1u);
  }
}
#endif
#endif

#if (MAXMATRIX_USE_PORT_REGISTERS == STD_ON)
// Looks up the port of `pin` and its bit there, as digitalWrite() does on every
// call.
Chain::OutputPin Chain::LookUpOutputPin(uint8_t pin) {
  return OutputPin{digitalPinToPort(pin), digitalPinToBitMask(pin)};
}

// Sets `pin` high or low, with the interrupts held off, as ShiftOut() does.
void Chain::WritePin(OutputPin pin, bool high) {
  // The NOLINT is that of ShiftOut(), for the same reason.
  auto* const port_register{
      portOutputRegister(pin.port)}; /* NOLINT(performance-no-int-to-ptr) */

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
    if (high) {
      Bit<uint8_t>::SetBits(*port_register, pin.mask);
    } else {
      Bit<uint8_t>::ClearBits(*port_register, pin.mask);
    }
  }
}
#else
// Takes the pin as it is: digitalWrite() looks its port up itself.
Chain::OutputPin Chain::LookUpOutputPin(uint8_t pin) { return OutputPin{pin}; }

// Sets `pin` high or low through the core, which holds the interrupts off
// itself where its board needs it.
void Chain::WritePin(OutputPin pin, bool high) {
  digitalWrite(pin.pin, high ? HIGH : LOW);
}
#endif

// With SPI chip select alone: the bus drives data in and clock at pins of its
// own, which a sketch on another bus does not name.
bool Chain::HasBoardPins() const {
#if (MAXMATRIX_USE_SPI == STD_ON)
  return IsBoardPin(chip_select_pin_);
#else
  bool const data_in_on_board{IsBoardPin(data_in_pin_)};
  bool const chip_select_on_board{IsBoardPin(chip_select_pin_)};
  bool const clock_on_board{IsBoardPin(clock_pin_)};
  return data_in_on_board && chip_select_on_board && clock_on_board;
#endif
}

}  // namespace internal
}  // namespace max_matrix
