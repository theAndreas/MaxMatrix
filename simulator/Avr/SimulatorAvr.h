// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief An example as the Uno runs it, in simavr, wired to the MAX7219 model.
 *
 * The .elf avr-gcc made runs on a simulated ATmega328P at 16 MHz. Pins 10, 11
 * and 13 - CS, DIN and CLK, which are SS, MOSI and SCK as well - and the SPI
 * output drive mock::chain, and the UART reads mock::serial_input and writes
 * mock::serial_output: the variables the Arduino stand-in drives for the
 * simulator a sketch is built into, so that the panel, the terminal and the
 * page work on either.
 */

#ifndef MAXMATRIX_SIMULATOR_AVR_SIMULATORAVR_H_
#define MAXMATRIX_SIMULATOR_AVR_SIMULATORAVR_H_

#include <sim_avr.h>
#include <sim_elf.h>
#include <stdint.h>

/* SimulatorAvr Parameter */
#define SIMULATOR_AVR_MCU "atmega328p"
#define SIMULATOR_AVR_FREQUENCY 16000000u

/*!
 * \brief An ATmega328P in simavr that runs the .elf of an example, wired to the
 *        MAX7219 model.
 *
 * Load() makes the AVR and wires it; Run() runs it on, one stretch of simulated
 * time after the other.
 */
class SimulatorAvr {
 private:
  avr_t* avr_;
  elf_firmware_t firmware_;
  bool data_in_;
  bool clock_;
  bool chip_select_;
  // The bits of the byte on the wire so far, for the frame the page shows; the
  // model has them one by one already.
  uint8_t bits_;
  uint8_t bit_count_;
  // simavr's receive buffer has room.
  bool receive_ready_;

  void ReceiveSerial();
  void TakeByte(uint8_t value) const;
  /*
   * simavr reports a pin or a byte through a C function and a pointer it was
   * given, so each of those is a static function that is handed the object. The
   * pins are followed rather than the port registers: digitalWrite() and
   * shiftOut() write the port bit by bit, and only a change is a step on the
   * wire.
   */
  static void OnDataIn(avr_irq_t* irq, uint32_t value, void* object);
  static void OnClock(avr_irq_t* irq, uint32_t value, void* object);
  static void OnChipSelect(avr_irq_t* irq, uint32_t value, void* object);
  static void OnSpiByte(avr_irq_t* irq, uint32_t value, void* object);
  static void OnSerialByte(avr_irq_t* irq, uint32_t value, void* object);
  static void OnReceiveReady(avr_irq_t* irq, uint32_t value, void* object);
  static void OnReceiveFull(avr_irq_t* irq, uint32_t value, void* object);

 public:
  /*! \brief Makes a simulator without an AVR; Load() makes one. */
  SimulatorAvr();

  /*! \brief Releases the AVR simavr made. */
  ~SimulatorAvr();

  /*!
   * \brief Not copyable.
   *
   * The simulated AVR is the object's own, and simavr holds a pointer to the
   * object.
   */
  SimulatorAvr(SimulatorAvr const&) = delete;

  /*! \brief Not assignable, as it is not copyable. */
  SimulatorAvr& operator=(SimulatorAvr const&) = delete;

  /*!
   * \brief Returns the simulated time.
   *
   * It counts the cycles the AVR has run, at its 16 MHz, not the host's clock.
   *
   * \return The milliseconds since the AVR started, or 0 before Load().
   */
  uint32_t GetMillis() const;

  /*!
   * \brief Makes the AVR and loads an example into it.
   *
   * The pins, the SPI output and the UART are wired to the MAX7219 model and to
   * `Serial`. It is called once.
   *
   * \param[in] elf_file The .elf arduino-cli made of the example.
   * \return True once the example is loaded, or false if the file is no .elf
   *         simavr can read.
   * \pre mock::Reset() has made the chain.
   */
  bool Load(char const* elf_file);

  /*!
   * \brief Runs the AVR on until the simulated time reaches `milliseconds`.
   *
   * On the way, it passes on what `Serial` has to read, when the UART has room
   * for it.
   *
   * \param[in] milliseconds The simulated time to stop at.
   * \return True once the time is reached, or false if the AVR stopped or
   *         crashed: the example ran into a sleep with every interrupt off, or
   *         the watchdog fired.
   * \pre Load() has succeeded.
   */
  bool Run(uint32_t milliseconds);
};

#endif  // MAXMATRIX_SIMULATOR_AVR_SIMULATORAVR_H_
