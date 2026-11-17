// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief How a run of either simulator starts and ends: the chain every example
 *        has, the command line, and the exit code.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORRUN_H_
#define MAXMATRIX_SIMULATOR_SIMULATORRUN_H_

#include <stdint.h>

#include "MaxMatrixAttributes.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"

/* the pins every example puts the chain on, and the chain it has,
 * max_matrix::MatrixOf<4> */
constexpr uint8_t kSimulatorRunDataInPin{11u};
constexpr uint8_t kSimulatorRunChipSelectPin{10u};
constexpr uint8_t kSimulatorRunClockPin{13u};
constexpr uint8_t kSimulatorRunNumberOfModules{4u};

/* exit codes */
constexpr int kSimulatorRunExitOk{0};
constexpr int kSimulatorRunExitFailed{1};
constexpr int kSimulatorRunExitUsage{2};

/*!
 * \brief How a run of the simulator on the host and of SimulatorAvr starts and
 *        ends.
 *
 * It has no state of its own, so every function is static, as in
 * max_matrix::Font.
 */
class SimulatorRun {
 public:
  /*!
   * \brief Sets a run up from its command line.
   *
   * It reads the options, draws the panel for their wiring and mounting, and
   * gives the mock the chain every example has and the serial input.
   *
   * \param[in] argc The number of arguments, the program's name included.
   * \param[in] argv The arguments.
   * \param[out] options The options read.
   * \param[out] panel The panel, for the wiring and mounting read.
   * \return True, or false if the command line is wrong or names no wiring
   *         there is.
   */
  MAXMATRIX_NODISCARD static bool SetUp(int argc, char** argv,
                                        SimulatorOptions& options,
                                        SimulatorPanel& panel);

  /*!
   * \brief Ends a run with `--for`: prints the panel and says how it went.
   *
   * \param[in] options The options of the run, with the expected panel.
   * \param[in] panel The panel at the end.
   * \return `kSimulatorRunExitOk` if the panel is the one expected, or none is,
   *         and no frame reached the chain broken; else
   *         `kSimulatorRunExitFailed`.
   */
  MAXMATRIX_NODISCARD static int End(SimulatorOptions const& options,
                                     SimulatorPanel const& panel);
};

#endif  // MAXMATRIX_SIMULATOR_SIMULATORRUN_H_
