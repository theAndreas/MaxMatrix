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

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

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

Chain::Chain(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
             ClockPin clock_pin)
    : data_in_pin_{data_in_pin.number},
      chip_select_pin_{chip_select_pin.number},
      clock_pin_{clock_pin.number} {}

Status Chain::Init(uint8_t number_of_modules) {
  if (!HasBoardPins()) {
    return Error::kOutOfRange;
  }

  pinMode(chip_select_pin_, OUTPUT);
  // A local first: read back from the member, it cost 8 bytes of flash in
  // every example with SPI.
  PortBit const chip_select{LookUpPortBit(chip_select_pin_)};
  chip_select_ = chip_select;
  WritePin(chip_select, true);
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the core's SPI object; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  SPI.begin();
#else
  pinMode(data_in_pin_, OUTPUT);
  pinMode(clock_pin_, OUTPUT);
  data_in_ = LookUpPortBit(data_in_pin_);
  clock_ = LookUpPortBit(clock_pin_);
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
  // the core's SPI object; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  SPI.beginTransaction(
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
  // the core's SPI object; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  SPI.endTransaction();
#endif
}

// Sends the address and value of one module within a frame; StartTransmission()
// has to come first. Neither const nor static: it changes the chain, and bit
// banging needs the pins, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const,readability-convert-member-functions-to-static)
void Chain::Transfer(max7219::Register address, uint8_t value) {
  auto const address_byte{static_cast<uint8_t>(address)};
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the core's SPI object; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  SPI.transfer(address_byte);
  // the core's SPI object; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  SPI.transfer(value);
#else
  ShiftOut(address_byte);
  ShiftOut(value);
#endif
}

#if (MAXMATRIX_USE_SPI == STD_OFF)
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
   * from flash, a pointer, and SREG is the status register at its fixed
   * address: clang-tidy reports both on the Uno, where there is no other way to
   * a port. T-8: auto, as the register is a volatile uint8_t on the board and
   * the mock's object of its own on the host.
   */
  auto* const data_in_register{portOutputRegister(
      data_in_.port)}; /* NOLINT(performance-no-int-to-ptr) */
  auto* const clock_register{
      portOutputRegister(clock_.port)}; /* NOLINT(performance-no-int-to-ptr) */
  uint8_t const status_register{
      SREG}; /* NOLINT(clang-analyzer-core.FixedAddressDereference) */

  cli();
  for (uint8_t bit{kFirstBit}; bit != 0u;
       bit = Bit<uint8_t>::ShiftDown(bit, 1u)) {
    if (Bit<uint8_t>::ReadBitGroup(value, bit) != 0u) {
      Bit<uint8_t>::SetBits(*data_in_register, data_in_.mask);
    } else {
      Bit<uint8_t>::ClearBits(*data_in_register, data_in_.mask);
    }
    Bit<uint8_t>::SetBits(*clock_register, clock_.mask);
    Bit<uint8_t>::ClearBits(*clock_register, clock_.mask);
  }
  SREG = status_register;
}
#endif

// Looks up the port of `pin` and its bit there, as digitalWrite() does on every
// call.
Chain::PortBit Chain::LookUpPortBit(uint8_t pin) {
  return PortBit{digitalPinToPort(pin), digitalPinToBitMask(pin)};
}

// Sets `pin` high or low, with the interrupts held off, as ShiftOut() does.
void Chain::WritePin(PortBit pin, bool high) {
  // The NOLINTs are those of ShiftOut(), for the same reason.
  auto* const port_register{
      portOutputRegister(pin.port)}; /* NOLINT(performance-no-int-to-ptr) */
  uint8_t const status_register{
      SREG}; /* NOLINT(clang-analyzer-core.FixedAddressDereference) */

  cli();
  if (high) {
    Bit<uint8_t>::SetBits(*port_register, pin.mask);
  } else {
    Bit<uint8_t>::ClearBits(*port_register, pin.mask);
  }
  SREG = status_register;
}

// All three, though SPI drives data in and clock by pins of its own: a sketch
// names the same pins for both, and one the board lacks is a mistake in either.
bool Chain::HasBoardPins() const {
  bool const data_in_on_board{IsBoardPin(data_in_pin_)};
  bool const chip_select_on_board{IsBoardPin(chip_select_pin_)};
  bool const clock_on_board{IsBoardPin(clock_pin_)};
  return data_in_on_board && chip_select_on_board && clock_on_board;
}

}  // namespace internal
}  // namespace max_matrix
