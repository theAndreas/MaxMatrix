// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Where the simulator meets a person or a program: the terminal, the
 *        page and the serial port on TCP.
 */

#include "SimulatorFrontEnd.h"

#include <stdio.h>

#include <string>

#include "Arduino.h"
#include "SimulatorTerminal.h"

SimulatorFrontEnd::SimulatorFrontEnd() {
  web_on_ = false;
  terminal_on_ = false;
}

bool SimulatorFrontEnd::Begin(SimulatorOptions const& options) {
  web_on_ = options.IsWeb();
  terminal_on_ = !web_on_ && options.GetRunFor() == 0;

  if (web_on_) {
    if (!web_.Begin(options.GetPort(), SIMULATOR_PAGE)) {
      fprintf(stderr, "port %u is taken, maybe by another simulator\n",
              options.GetPort());
      return false;
    }
    printf("http://localhost:%u/\n", options.GetPort());
  }
  if (options.GetTcpPort() != 0) {
    if (!serial_host_.Begin(options.GetTcpPort())) {
      fprintf(stderr, "port %u is taken, maybe by another simulator\n",
              options.GetTcpPort());
      return false;
    }
    printf("Serial on localhost:%u\n", options.GetTcpPort());
  }
  fflush(stdout);
  if (terminal_on_) SimulatorTerminal::Clear();
  return true;
}

void SimulatorFrontEnd::Follow(SimulatorPanel& panel, uint32_t now) {
  // Taken from the mock once, here, for both: the page sends it with the panel,
  // when that is due, and the control program at once, so neither could clear
  // it for the other.
  std::string const printed = mock::serial_output;
  mock::serial_output.clear();

  if (web_on_) web_.Task(panel, printed);
  serial_host_.Send(printed);
  serial_host_.Task(&mock::serial_input);

  if (panel.IsDrawDue(now)) {
    if (web_on_)
      web_.Send(panel);
    else if (terminal_on_)
      SimulatorTerminal::Draw(panel);
    panel.SetDrawn(now);
  }
}
