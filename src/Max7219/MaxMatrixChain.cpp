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

#include "MaxMatrixChain.h"
#if (MAXMATRIX_USE_SPI == STD_ON)
#include "SPI.h"
#endif

namespace max_matrix {
namespace internal {

Chain::Chain(byte data_in_pin, byte chip_select_pin, byte clock_pin,
             byte number_of_modules) {
  data_in_pin_ = data_in_pin;
  chip_select_pin_ = chip_select_pin;
  clock_pin_ = clock_pin;
  number_of_modules_ = number_of_modules;
  chip_select_port_ = 0;
  chip_select_mask_ = 0;
#if (MAXMATRIX_USE_SPI == STD_OFF)
  data_in_port_ = 0;
  data_in_mask_ = 0;
  clock_port_ = 0;
  clock_mask_ = 0;
#endif
}

void Chain::Init() {
  pinMode(chip_select_pin_, OUTPUT);
  chip_select_port_ = digitalPinToPort(chip_select_pin_);
  chip_select_mask_ = digitalPinToBitMask(chip_select_pin_);
  WritePin(chip_select_port_, chip_select_mask_, true);
#if (MAXMATRIX_USE_SPI == STD_ON)
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

  WriteRegisterAll(
      MAX7219_REG_SCAN_LIMIT_ADDRESS,
      MAX7219_REG_SCAN_LIMIT_DISPLAY_DIGIT_0_TO_7);  // display all digits
  WriteRegisterAll(
      MAX7219_REG_DECODE_MODE_ADDRESS,
      MAX7219_REG_DECODE_MODE_NO_DECODE);  // using an led matrix (not digits)
  WriteRegisterAll(
      MAX7219_REG_SHUTDOWN_ADDRESS,
      MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);  // normal operation mode
  WriteRegisterAll(
      MAX7219_REG_DISPLAY_TEST_ADDRESS,
      MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);  // no display test
}

void Chain::WriteRegisterAll(byte register_address, byte register_value) {
  StartTransmission();
  for (byte module = 0; module < number_of_modules_; module++)
    Transfer(register_address, register_value);
  StopTransmission();
}

void Chain::WriteRegisterEach(byte register_address,
                              byte const* register_values, byte stride) {
  StartTransmission();
  for (byte module = 0; module < number_of_modules_;
       module++, register_values += stride)
    Transfer(register_address, *register_values);
  StopTransmission();
}

// Starts a frame: pulls chip select low, and in SPI mode reserves the bus.
// StopTransmission() has to follow once the frame is transferred. Not const: it
// changes the chain this object stands for, see MaxMatrixChain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::StartTransmission() {
#if (MAXMATRIX_USE_SPI == STD_ON)
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
// Not const: it changes the chain this object stands for, see MaxMatrixChain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::StopTransmission() {
  WritePin(chip_select_port_, chip_select_mask_, true);
#if (MAXMATRIX_USE_SPI == STD_ON)
  SPI.endTransaction();
#endif
}

// Sends the address and value of one module within a frame; StartTransmission()
// has to come first. Neither const nor static: it changes the chain, and bit
// banging needs the pins, see MaxMatrixChain.h.
// NOLINTNEXTLINE(readability-make-member-function-const,readability-convert-member-functions-to-static)
void Chain::Transfer(byte register_address, byte register_value) {
#if (MAXMATRIX_USE_SPI == STD_ON)
  SPI.transfer(register_address);
  SPI.transfer(register_value);
#else
  ShiftOut(register_address);
  ShiftOut(register_value);
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
// Not const: it changes the chain this object stands for, see MaxMatrixChain.h.
// NOLINTNEXTLINE(readability-make-member-function-const)
void Chain::ShiftOut(byte value) {
  /*
   * The core's portOutputRegister() makes the register's address, a number read
   * from flash, a pointer, and SREG is the status register at its fixed
   * address: clang-tidy reports both on the Uno, where there is no other way to
   * a port.
   */
  auto const data_in_register =
      portOutputRegister(data_in_port_); /* NOLINT(performance-no-int-to-ptr) */
  auto const clock_register =
      portOutputRegister(clock_port_); /* NOLINT(performance-no-int-to-ptr) */
  byte const status_register =
      SREG; /* NOLINT(clang-analyzer-core.FixedAddressDereference) */

  cli();
  for (byte bit = 0x80; bit != 0; bit >>= 1) {
    if ((value & bit) != 0)
      *data_in_register |= data_in_mask_;
    else
      *data_in_register &= static_cast<byte>(~data_in_mask_);
    *clock_register |= clock_mask_;
    *clock_register &= static_cast<byte>(~clock_mask_);
  }
  SREG = status_register;
}
#endif

// Sets the pin of `mask` in `port` high or low, with the interrupts held off,
// as ShiftOut() does.
void Chain::WritePin(byte port, byte mask, bool high) {
  // The NOLINTs are those of ShiftOut(), for the same reason.
  auto const port_register =
      portOutputRegister(port); /* NOLINT(performance-no-int-to-ptr) */
  byte const status_register =
      SREG; /* NOLINT(clang-analyzer-core.FixedAddressDereference) */

  cli();
  if (high)
    *port_register |= mask;
  else
    *port_register &= static_cast<byte>(~mask);
  SREG = status_register;
}

}  // namespace internal
}  // namespace max_matrix
