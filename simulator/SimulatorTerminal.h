// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in the terminal.
 *
 * Redrawn in place while the sketch runs, or printed once at the end of a run
 * with `--for` and compared with the panel a test expects.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORTERMINAL_H_
#define MAXMATRIX_SIMULATOR_SIMULATORTERMINAL_H_

#include <string>

#include "SimulatorPanel.h"

/*!
 * \brief The simulator's panel in the terminal.
 *
 * It has no state of its own, so every function is static, as in
 * max_matrix::Font.
 */
class SimulatorTerminal {
 private:
  static bool ReadFile(char const* path, std::string* text);

 public:
  /*!
   * \brief Returns the line above the panel.
   *
   * It says which wiring and mounting the panel is drawn for, the intensity of
   * every module and how many frames the sketch has sent. The modules are
   * counted from the left, as on the page.
   *
   * \param[in] panel The panel to describe.
   * \return The line, without a line break.
   */
  static std::string GetStatusLine(SimulatorPanel const& panel);

  /*!
   * \brief Clears the terminal.
   *
   * Call it once before the first draw; every Draw() after it only moves the
   * cursor home.
   */
  static void Clear();

  /*!
   * \brief Draws the panel in place, with the status line above it.
   *
   * It moves the cursor home and writes over what the last draw left.
   *
   * \param[in] panel The panel to draw.
   */
  static void Draw(SimulatorPanel const& panel);

  /*!
   * \brief Prints the panel once at the end of a run, in ASCII, and compares it
   *        with the expected panel if there is one.
   *
   * \param[in] panel The panel to print.
   * \param[in] expect_file The file with the expected panel, or nullptr for
   *                        none.
   * \return True if the panel is as expected or none is expected, or false if
   *         it differs from the file or the file cannot be read.
   */
  static bool PrintResult(SimulatorPanel const& panel, char const* expect_file);
};

#endif  // MAXMATRIX_SIMULATOR_SIMULATORTERMINAL_H_
