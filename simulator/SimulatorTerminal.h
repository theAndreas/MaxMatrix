// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in the terminal.
 *
 * Redrawn in place while the sketch runs, or printed once at the end of a run with --for and
 * compared with the panel a test expects.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORTERMINAL_H_
#define MAXMATRIX_SIMULATOR_SIMULATORTERMINAL_H_

#include "SimulatorPanel.h"

/* no state of its own, so every function is static, as in MaxMatrixFont */
class SimulatorTerminal
{
  private:
    static bool ReadFile(char const*, std::string*);

  public:
    // get methods
    static std::string GetStatusLine(SimulatorPanel const&);

    // methods
    static void Clear();
    static void Draw(SimulatorPanel const&);
    static bool PrintResult(SimulatorPanel const&, char const*);
};

#endif
