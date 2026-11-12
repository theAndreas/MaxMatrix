// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Runs an example sketch from examples/ on the host and draws the LED
 *        panel.
 *
 * Build/Simulator-ScrollText-bitbang [--wiring FC16] [--upside-down] [--serial
 * LINE]... [--web
 * [--port PORT]] [--tcp PORT] [--for SECONDS [--expect FILE]].
 *
 * The sketch is compiled unchanged against the Arduino stand-in in test/mock/
 * and the MAX7219 model in test/model/: setup() runs once, loop() over and
 * over, millis() and delay() on the host clock.
 *
 * Without --for it runs until Ctrl+C and redraws the panel in the terminal.
 * With --for it runs that long, prints the panel once and exits; --expect then
 * compares the printout with a file and fails on a difference.
 *
 * --serial hands the sketch's Serial a line to read, as if it was typed; each
 * one given is read in turn.
 *
 * --web draws in a browser instead, on http://localhost:8080/
 * (simulator/Web/index.html). --port picks another port, so that two
 * simulators, or a simulator and the tests, can run side by side.
 *
 * --tcp opens the sketch's Serial as a TCP port on localhost, for the control
 * program in control/: what it sends, the sketch reads, and what the sketch
 * prints, it gets back.
 */

#include <stdio.h>
#include <stdlib.h>

#include <chrono>
#include <thread>

#include "Arduino.h"
#include "SimulatorFrontEnd.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"

namespace {

/* the pins every example puts the chain on */
constexpr uint8_t kDataInPin{11u};
constexpr uint8_t kChipSelectPin{10u};
constexpr uint8_t kClockPin{13u};
/* the chain every example has, max_matrix::MatrixOf<4> */
constexpr uint8_t kNumberOfModules{4u};

/* exit codes */
constexpr int kExitOk{0};
constexpr int kExitFailed{1};
constexpr int kExitUsage{2};

}  // namespace

/* the sketch's, compiled into the same program */
void setup();  // NOLINT(readability-identifier-naming): Arduino's name
void loop();   // NOLINT(readability-identifier-naming): Arduino's name

namespace {

// mock::tick is a plain function pointer, so what Tick() works on has to be
// reachable without an object.
SimulatorOptions options;
SimulatorPanel panel;
SimulatorFrontEnd front_end;

/*!
 * \brief Ends a run with `--for`.
 *
 * It prints the panel and exits, from within the sketch's loop() or delay(),
 * which never return on their own.
 */
[[noreturn]] void Finish() {
  bool const passed =
      SimulatorTerminal::PrintResult(panel, options.GetExpectFile()) &&
      mock::errors == 0;

  exit(passed ? kExitOk : kExitFailed);
}

/*!
 * \brief Follows the sketch.
 *
 * It is called after every frame and every millisecond of delay(), so that the
 * panel is drawn while a sketch blocks. In a run with `--for` the terminal
 * draws only at the end.
 */
void Tick() {
  uint32_t const now = millis();

  front_end.Follow(panel, now);
  if (options.GetRunFor() != 0 && now >= options.GetRunFor()) Finish();
}

}  // namespace

/*!
 * \brief Runs the sketch.
 *
 * It runs setup() once and loop() until the run ends.
 *
 * \param[in] argc The number of arguments, the program's name included.
 * \param[in] argv The arguments.
 * \return `kExitFailed` if the port is taken, or `kExitUsage` if the command
 *         line is wrong.
 * \note It returns only on these errors: a run with `--for` exits from
 *       Finish(), and one without runs until Ctrl+C.
 */
int main(int argc, char** argv) {
  /* the sketch is built in; an .elf is for SimulatorAvr */
  if (!options.Parse(argc, argv) || options.GetElfFile() != nullptr ||
      !panel.SetWiring(options.GetWiringName())) {
    SimulatorOptions::PrintUsage(argv[0]);
    return kExitUsage;
  }
  panel.SetUpsideDown(options.IsUpsideDown());
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  mock::serial_input = options.GetSerialInput();
  mock::tick = Tick;
  if (!front_end.Begin(options)) return kExitFailed;

  setup();
  for (;;) {
    loop();
    Tick();
    /* a sketch polling millis() would otherwise keep a core busy */
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}
