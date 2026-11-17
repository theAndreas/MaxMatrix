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
#include "MaxMatrixStandardTypes.h"
#include "Utility/Byte.h"
#include "Utility/span.h"
#if (MAXMATRIX_USE_SPI == STD_ON)
#include "SPI.h"
#endif
#include "Utility/Bit.h"

namespace max_matrix {
namespace internal {

#if (MAXMATRIX_USE_SPI == STD_OFF)
namespace {

/* the MAX7219 takes a byte most significant bit first */
constexpr uint8_t kFirstBit{0x80};

}  // namespace
#endif

Chain::Chain(uint8_t data_in_pin, uint8_t chip_select_pin, uint8_t clock_pin)
    : data_in_pin_{data_in_pin},
      chip_select_pin_{chip_select_pin},
      clock_pin_{clock_pin} {}

void Chain::Init(uint8_t number_of_modules) {
  pinMode(chip_select_pin_, OUTPUT);
  chip_select_port_ = digitalPinToPort(chip_select_pin_);
  chip_select_mask_ = digitalPinToBitMask(chip_select_pin_);
  WritePin(chip_select_port_, chip_select_mask_, true);
#if (MAXMATRIX_USE_SPI == STD_ON)
  // the core's SPI object; its members are static on the AVR alone
  // NOLINTNEXTLINE(readability-static-accessed-through-instance)
  SPI.begin();
#else
  pinMode(data_in_pin_, OUTPUT);
  pinMode(clock_pin_, OUTPUT);
  data_in_port_ = digitalPinToPort(data_in_pin_);
  data_in_mask_ = digitalPinToBitMask(data_in_pin_);
  clock_port_ = digitalPinToPort(clock_pin_);
  clock_mask_ = digitalPinToBitMask(clock_pin_);
  /* clock idles low, MAX7219 samples data on the rising edge */
  WritePin(clock_port_, clock_mask_, false);
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
}

void Chain::WriteRegisterAll(max7219::Register address, uint8_t value,
                             uint8_t number_of_modules) {
  StartTransmission();
  for (uint8_t module{0}; module < number_of_modules; module++) {
    Transfer(address, value);
  }
  StopTransmission();
}

void Chain::WriteRegisterEach(max7219::Register address,
                              span<Byte const> buffer, uint8_t column) {
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
  WritePin(chip_select_port_, chip_select_mask_, false);
}

/*
 * Ends a frame: pulls chip select high, so that every MAX7219 latches what it
 * was sent, and in SPI mode releases the bus for other devices.
 * StartTransmission() has to come first.
 */
// Not const: it changes the chain this object stands for, see Chain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::StopTransmission() {
  WritePin(chip_select_port_, chip_select_mask_, true);
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
   * a port.
   */
  auto* const data_in_register{portOutputRegister(
      data_in_port_)}; /* NOLINT(performance-no-int-to-ptr) */
  auto* const clock_register{
      portOutputRegister(clock_port_)}; /* NOLINT(performance-no-int-to-ptr) */
  uint8_t const status_register{
      SREG}; /* NOLINT(clang-analyzer-core.FixedAddressDereference) */

  cli();
  for (uint8_t bit{kFirstBit}; bit != 0u;
       bit = Bit<uint8_t>::ShiftDown(bit, 1u)) {
    if (Bit<uint8_t>::ReadBitGroup(value, bit) != 0u) {
      Bit<uint8_t>::SetBits(*data_in_register, data_in_mask_);
    } else {
      Bit<uint8_t>::ClearBits(*data_in_register, data_in_mask_);
    }
    Bit<uint8_t>::SetBits(*clock_register, clock_mask_);
    Bit<uint8_t>::ClearBits(*clock_register, clock_mask_);
  }
  SREG = status_register;
}
#endif

// Sets the pin of `mask` in `port` high or low, with the interrupts held off,
// as ShiftOut() does.
void Chain::WritePin(uint8_t port, uint8_t mask, bool high) {
  // The NOLINTs are those of ShiftOut(), for the same reason.
  auto* const port_register{
      portOutputRegister(port)}; /* NOLINT(performance-no-int-to-ptr) */
  uint8_t const status_register{
      SREG}; /* NOLINT(clang-analyzer-core.FixedAddressDereference) */

  cli();
  if (high) {
    Bit<uint8_t>::SetBits(*port_register, mask);
  } else {
    Bit<uint8_t>::ClearBits(*port_register, mask);
  }
  SREG = status_register;
}

}  // namespace internal
}  // namespace max_matrix
