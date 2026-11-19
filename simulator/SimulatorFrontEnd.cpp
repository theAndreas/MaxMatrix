// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Where the simulator meets a person or a program: the terminal, the
 *        page and the serial port on TCP.
 */

#include "SimulatorFrontEnd.h"

#include <stdint.h>

#include <iostream>
#include <string>

#include "Arduino.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"

namespace {

// Says that `port` is taken, which another simulator does most often.
void ReportTaken(uint16_t port) {
  std::cerr << "port " << port << " is taken, maybe by another simulator\n";
}

}  // namespace

SimulatorFrontEnd::SimulatorFrontEnd() = default;

bool SimulatorFrontEnd::Begin(SimulatorOptions const& options) {
  web_on_ = options.IsWeb();
  terminal_on_ = !web_on_ && (options.GetRunFor() == 0);

  if (web_on_) {
    if (!web_.Begin(options.GetPort(), SIMULATOR_PAGE)) {
      ReportTaken(options.GetPort());
      return false;
    }
    std::cout << "http://localhost:" << options.GetPort() << "/\n";
  }
  if (options.GetTcpPort() != 0) {
    if (!serial_host_.Begin(options.GetTcpPort())) {
      ReportTaken(options.GetTcpPort());
      return false;
    }
    std::cout << "Serial on localhost:" << options.GetTcpPort() << '\n';
  }
  std::cout.flush();
  if (terminal_on_) {
    SimulatorTerminal::Clear();
  }
  return true;
}

void SimulatorFrontEnd::Follow(SimulatorPanel& panel, uint32_t now) {
  // Taken from the mock once, here, for both: the page sends it with the panel,
  // when that is due, and the control program at once, so neither could clear
  // it for the other.
  std::string const printed = mock::serial_output;
  mock::serial_output.clear();

  if (web_on_) {
    web_.Task(panel, printed);
  }
  serial_host_.Send(printed);
  mock::serial_input += serial_host_.Task();

  if (panel.IsDrawDue(now)) {
    if (web_on_) {
      web_.Send(panel);
    } else if (terminal_on_) {
      SimulatorTerminal::Draw(panel);
    } else {
      // neither draws while the run lasts: --for prints the panel at its end
    }
    panel.SetDrawn(now);
  }
}
