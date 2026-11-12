// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief An example as the Uno runs it, in simavr, wired to the MAX7219 model.
 *
 * A frame is counted as the Arduino stand-in counts it: from the falling to the
 * rising edge of chip select. A clock edge or an SPI byte outside a frame, and
 * a frame that is not a whole number of bytes, are errors in mock::errors, as a
 * transfer outside a frame is there.
 */

#include "SimulatorAvr.h"

#include <avr_ioport.h>
#include <avr_spi.h>
#include <avr_uart.h>
#include <sim_time.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Arduino.h"

/* the Uno's pins 10, 11 and 13 are PB2, PB3 and PB5 */
#define SIMULATOR_AVR_PORT 'B'
#define SIMULATOR_AVR_CHIP_SELECT_BIT 2u
#define SIMULATOR_AVR_DATA_IN_BIT 3u
#define SIMULATOR_AVR_CLOCK_BIT 5u

/* Serial is the first and only UART of the ATmega328P */
#define SIMULATOR_AVR_UART '0'

/*!
 * \brief Passes on simavr's log, errors only.
 *
 * \param[in] avr The AVR, or `NULL` while the .elf is read.
 * \param[in] level The level of the message; `LOG_ERROR` or less is passed on.
 * \param[in] format As for `printf()`.
 * \param[in] arguments As for `vprintf()`.
 */
__attribute__((format(printf, 3, 0))) static void LogSimavr(avr_t* avr,
                                                            int const level,
                                                            char const* format,
                                                            va_list arguments) {
  (void)avr;
  // simavr also reports every section of the .elf it loads, which would stand
  // between the panel and the command line.
  if (level <= LOG_ERROR) vfprintf(stderr, format, arguments);
}

SimulatorAvr::SimulatorAvr() {
  avr_ = nullptr;
  memset(&firmware_, 0, sizeof(firmware_));
  data_in_ = false;
  clock_ = false;
  // Chip select is high, as the pull-up of a real module keeps it until Init()
  // drives the pin.
  chip_select_ = true;
  bits_ = 0;
  bit_count_ = 0;
  receive_ready_ = false;
}

SimulatorAvr::~SimulatorAvr() {
  if (avr_ != nullptr) avr_terminate(avr_);
}

uint32_t SimulatorAvr::GetMillis() const {
  if (avr_ == nullptr) return 0;
  return static_cast<uint32_t>(avr_cycles_to_usec(avr_, avr_->cycle) / 1000);
}

bool SimulatorAvr::Load(char const* elf_file) {
  avr_global_logger_set(LogSimavr);
  if (elf_read_firmware(elf_file, &firmware_) != 0) return false;
  avr_ = avr_make_mcu_by_name(SIMULATOR_AVR_MCU);
  if (avr_ == nullptr) return false;
  avr_init(avr_);
  avr_->frequency = SIMULATOR_AVR_FREQUENCY;
  avr_load_firmware(avr_, &firmware_);

  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_IOPORT_GETIRQ(SIMULATOR_AVR_PORT),
                    SIMULATOR_AVR_DATA_IN_BIT),
      OnDataIn, this);
  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_IOPORT_GETIRQ(SIMULATOR_AVR_PORT),
                    SIMULATOR_AVR_CLOCK_BIT),
      OnClock, this);
  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_IOPORT_GETIRQ(SIMULATOR_AVR_PORT),
                    SIMULATOR_AVR_CHIP_SELECT_BIT),
      OnChipSelect, this);
  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), OnSpiByte,
      this);

  // The UART would print every line on the console itself, between the panel
  // and the command line.
  uint32_t flags = 0;
  avr_ioctl(avr_, AVR_IOCTL_UART_GET_FLAGS(SIMULATOR_AVR_UART), &flags);
  flags &= ~static_cast<uint32_t>(AVR_UART_FLAG_STDIO);
  avr_ioctl(avr_, AVR_IOCTL_UART_SET_FLAGS(SIMULATOR_AVR_UART), &flags);
  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART),
                    UART_IRQ_OUTPUT),
      OnSerialByte, this);
  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART),
                    UART_IRQ_OUT_XON),
      OnReceiveReady, this);
  avr_irq_register_notify(
      avr_io_getirq(avr_, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART),
                    UART_IRQ_OUT_XOFF),
      OnReceiveFull, this);
  return true;
}

bool SimulatorAvr::Run(uint32_t milliseconds) {
  avr_cycle_count_t const end = avr_usec_to_cycles(avr_, milliseconds * 1000u);

  ReceiveSerial();
  while (avr_->cycle < end) {
    int const state = avr_run(avr_);
    if (state == cpu_Done || state == cpu_Crashed) return false;
  }
  return true;
}

// Hands the UART what Serial has to read, one byte after the other until the
// UART says its buffer is full; the rest waits for the next time it has room.
void SimulatorAvr::ReceiveSerial() {
  avr_irq_t* const input = avr_io_getirq(
      avr_, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART), UART_IRQ_INPUT);

  while (receive_ready_ && !mock::serial_input.empty()) {
    uint8_t const received = static_cast<uint8_t>(mock::serial_input[0]);
    mock::serial_input.erase(0, 1);
    avr_raise_irq(input, received);
  }
}

// Adds `value`, the byte on the wire, to the frame as the page shows it; a byte
// outside a frame is an error, as in the Arduino stand-in.
void SimulatorAvr::TakeByte(uint8_t value) const {
  if (chip_select_)
    mock::errors++;
  else
    mock::frame.push_back(value);
}

// DIN changed: `irq` is the pin, `value` its level, `object` the SimulatorAvr.
void SimulatorAvr::OnDataIn(avr_irq_t* irq, uint32_t value, void* object) {
  (void)irq;
  static_cast<SimulatorAvr*>(object)->data_in_ = value != 0;
}

/*
 * CLK changed: `irq` is the pin, `value` its level, `object` the SimulatorAvr.
 * On a rising edge, DIN is shifted into the chain, as the MAX7219 does, and the
 * bits are collected into the byte the page shows. Only bit banging moves the
 * pin; the SPI peripheral of simavr reports its byte without it.
 */
void SimulatorAvr::OnClock(avr_irq_t* irq, uint32_t value, void* object) {
  SimulatorAvr* const simulator = static_cast<SimulatorAvr*>(object);
  bool const rising = value != 0 && !simulator->clock_;

  (void)irq;
  simulator->clock_ = value != 0;
  if (!rising) return;
  mock::chain.ShiftBit(simulator->data_in_);
  uint8_t const shifted{static_cast<uint8_t>(simulator->bits_ << 1U)};
  simulator->bits_ =
      static_cast<uint8_t>(shifted | (simulator->data_in_ ? 1U : 0U));
  if (++simulator->bit_count_ == 8) {
    simulator->TakeByte(simulator->bits_);
    simulator->bit_count_ = 0;
  }
}

/*
 * CS changed: `irq` is the pin, `value` its level, `object` the SimulatorAvr. A
 * falling edge starts a frame and a rising one latches it, as every MAX7219 of
 * the chain does; a frame that ends within a byte is an error.
 */
void SimulatorAvr::OnChipSelect(avr_irq_t* irq, uint32_t value, void* object) {
  SimulatorAvr* const simulator = static_cast<SimulatorAvr*>(object);
  bool const high = value != 0;

  (void)irq;
  if (simulator->chip_select_ && !high) {
    mock::frame.clear();
  } else if (!simulator->chip_select_ && high) {
    if (simulator->bit_count_ != 0) mock::errors++;
    simulator->bit_count_ = 0;
    mock::chain.Latch();
    mock::last_frame = mock::frame;
    mock::frames++;
  }
  simulator->chip_select_ = high;
}

// The SPI peripheral sent a byte: `irq` is the SPI output, `value` the byte,
// `object` the SimulatorAvr.
void SimulatorAvr::OnSpiByte(avr_irq_t* irq, uint32_t value, void* object) {
  uint8_t const data = static_cast<uint8_t>(value);

  (void)irq;
  mock::chain.Shift(data);
  static_cast<SimulatorAvr*>(object)->TakeByte(data);
}

// The sketch printed a byte on Serial: `irq` is the UART output, `value` the
// byte, `object` the SimulatorAvr.
void SimulatorAvr::OnSerialByte(avr_irq_t* irq, uint32_t value, void* object) {
  (void)irq;
  (void)object;
  mock::serial_output += static_cast<char>(value);
}

/*
 * The UART has room for a byte: `irq` is XON of the UART, `value` unused,
 * `object` the SimulatorAvr. What Serial has to read is sent at once, as simavr
 * asks: it says so again and again while there is room, and XOFF, from within
 * the sending, when there is none.
 */
void SimulatorAvr::OnReceiveReady(avr_irq_t* irq, uint32_t value,
                                  void* object) {
  SimulatorAvr* const simulator = static_cast<SimulatorAvr*>(object);

  (void)irq;
  (void)value;
  simulator->receive_ready_ = true;
  simulator->ReceiveSerial();
}

// The UART's buffer is full: `irq` is XOFF of the UART, `value` unused,
// `object` the SimulatorAvr.
void SimulatorAvr::OnReceiveFull(avr_irq_t* irq, uint32_t value, void* object) {
  (void)irq;
  (void)value;
  static_cast<SimulatorAvr*>(object)->receive_ready_ = false;
}
