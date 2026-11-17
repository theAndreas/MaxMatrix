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

#include "MaxMatrixAttributes.h"

/* SimulatorAvr Parameter */
constexpr char kSimulatorAvrMcu[] = "atmega328p";
constexpr uint32_t kSimulatorAvrFrequency{16000000u};
constexpr uint32_t kSimulatorAvrCyclesPerMillisecond{kSimulatorAvrFrequency /
                                                     1000u};

/*!
 * \brief An ATmega328P in simavr that runs the .elf of an example, wired to the
 *        MAX7219 model.
 *
 * Load() makes the AVR and wires it; Run() runs it on, one stretch of simulated
 * time after the other.
 */
class SimulatorAvr {
 private:
  avr_t* avr_{nullptr};
  elf_firmware_t firmware_{};
  bool data_in_{false};
  bool clock_{false};
  // high, as the pull-up of a real module keeps it until Init() drives the pin
  bool chip_select_{true};
  // The bits of the byte on the wire so far, for the frame the page shows; the
  // model has them one by one already.
  uint8_t bits_{0};
  uint8_t bit_count_{0};
  // simavr's receive buffer has room.
  bool receive_ready_{false};

  void ReceiveSerial();
  void TakeByte(uint8_t value);
  /*
   * simavr reports a pin or a byte through a C function and a pointer it was
   * given, so each of those is a static function that is handed the object. The
   * pins are followed rather than the port registers: digitalWrite() and
   * shiftOut() write the port bit by bit, and only a change is a step on the
   * wire.
   */
  void ConnectPins();
  void ConnectUart();
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
   * \brief Returns the cycles the AVR runs in `milliseconds`, at its 16 MHz.
   *
   * In 64 bits throughout: simavr's own conversion takes the microseconds as
   * 32, which wrap after 71.6 minutes, and a run of the page or the control
   * program lasts until Ctrl+C.
   *
   * \param[in] milliseconds The simulated time.
   * \return The cycles.
   */
  MAXMATRIX_NODISCARD static constexpr avr_cycle_count_t ConvertToCycles(
      uint32_t milliseconds) {
    return static_cast<avr_cycle_count_t>(milliseconds) *
           kSimulatorAvrCyclesPerMillisecond;
  }

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
