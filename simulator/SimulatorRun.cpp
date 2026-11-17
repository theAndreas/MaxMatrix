// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief How a run of either simulator starts and ends.
 */

#include "SimulatorRun.h"

#include "Arduino.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"

bool SimulatorRun::SetUp(int argc, char** argv, SimulatorOptions& options,
                         SimulatorPanel& panel) {
  if (!options.Parse(argc, argv) || !panel.SetWiring(options.GetWiringName())) {
    return false;
  }
  panel.SetUpsideDown(options.IsUpsideDown());
  mock::Reset(kSimulatorRunDataInPin, kSimulatorRunChipSelectPin,
              kSimulatorRunClockPin, kSimulatorRunNumberOfModules);
  mock::serial_input = options.GetSerialInput();
  return true;
}

int SimulatorRun::End(SimulatorOptions const& options,
                      SimulatorPanel const& panel) {
  bool const as_expected{
      SimulatorTerminal::PrintResult(panel, options.GetExpectFile())};
  bool const passed{as_expected && (mock::errors == 0)};
  return passed ? kSimulatorRunExitOk : kSimulatorRunExitFailed;
}
