// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Runs an example as the Uno runs it, in simavr, and draws the LED
 *        panel.
 *
 * Build/SimulatorAvr --elf FILE [--wiring FC16] [--upside-down] [--serial
 * LINE]... [--web [--port PORT]] [--tcp PORT] [--for SECONDS [--expect FILE]].
 *
 * The .elf arduino-cli made of an example runs on a simulated ATmega328P,
 * SimulatorAvr, and drives the MAX7219 model through the pins and the SPI
 * output, rather than through the Arduino stand-in. The rest is the simulator
 * of main.cpp: the same options, panel, terminal and page.
 *
 * With --web or --tcp, or without --for, the simulated time keeps to the host's
 * clock, so that the panel moves as it would on the board and the page talks to
 * it in real time. Otherwise it runs as fast as simavr can, and as the time is
 * simulated, a run of the same .elf ends with the same panel every time, also
 * of an example that moves.
 */

#include <chrono>
#include <iostream>
#include <thread>

#include "Arduino.h"
#include "SimulatorAvr.h"
#include "SimulatorFrontEnd.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"

namespace {

/* the pins every example puts the chain on */
constexpr uint8_t kDataInPin{11U};
constexpr uint8_t kChipSelectPin{10U};
constexpr uint8_t kClockPin{13U};
/* the chain every example has, max_matrix::MatrixOf<4> */
constexpr uint8_t kNumberOfModules{4U};

/* exit codes, as in main.cpp */
constexpr int kExitOk{0};
constexpr int kExitFailed{1};
constexpr int kExitUsage{2};

// mock::tick is a plain function pointer, so what Tick() works on has to be
// reachable without an object.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SimulatorOptions options;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SimulatorPanel panel;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SimulatorFrontEnd front_end;
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
SimulatorAvr avr;

// Runs the AVR a millisecond at a time, for `--for` or until Ctrl+C, and the
// front end with it. Returns false if the AVR stopped.
bool RunAvr() {
  /* a page or the control program, and whoever is in front of it, speaks in
   * real time */
  bool const real_time =
      options.IsWeb() || options.GetTcpPort() != 0 || options.GetRunFor() == 0;
  std::chrono::steady_clock::time_point const start =
      std::chrono::steady_clock::now();
  for (uint32_t now = 1; options.GetRunFor() == 0 || now <= options.GetRunFor();
       now++) {
    if (!avr.Run(now)) {
      std::cerr << "the AVR stopped after " << now << " ms\n";
      return false;
    }
    front_end.Follow(panel, now);
    // simavr runs faster than the Uno, so in real time it waits for the host's
    // clock to catch up.
    if (real_time) {
      std::this_thread::sleep_until(start + std::chrono::milliseconds(now));
    }
  }
  return true;
}

}  // namespace

/*!
 * \brief Runs the example.
 *
 * It runs the AVR one simulated millisecond at a time until the run ends.
 *
 * \param[in] argc The number of arguments, the program's name included.
 * \param[in] argv The arguments.
 * \return `kExitOk` if the run with `--for` ended with the panel expected and
 *         no transfer error; `kExitFailed` if the .elf cannot be read, the port
 *         is taken, the AVR stopped, or the run with `--for` ended with another
 *         panel or a transfer error; or `kExitUsage` if the command line is
 *         wrong.
 */
int main(int argc, char** argv) {
  if (!options.Parse(argc, argv) || options.GetElfFile() == nullptr ||
      !panel.SetWiring(options.GetWiringName())) {
    SimulatorOptions::PrintUsage(*argv);
    return kExitUsage;
  }
  panel.SetUpsideDown(options.IsUpsideDown());
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  mock::serial_input = options.GetSerialInput();
  if (!avr.Load(options.GetElfFile())) {
    std::cerr << options.GetElfFile() << " is no .elf for the "
              << kSimulatorAvrMcu << '\n';
    return kExitFailed;
  }
  if (!front_end.Begin(options)) {
    return kExitFailed;
  }
  if (!RunAvr()) {
    return kExitFailed;
  }
  bool const passed =
      SimulatorTerminal::PrintResult(panel, options.GetExpectFile()) &&
      mock::errors == 0;
  return passed ? kExitOk : kExitFailed;
}
