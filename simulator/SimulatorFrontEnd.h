// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Where the simulator meets a person or a program: the terminal, the
 *        page and the serial port on TCP.
 *
 * Both simulators have the same ends, the one a sketch is built into and
 * SimulatorAvr, so they share this rather than each choosing them on its own.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORFRONTEND_H_
#define MAXMATRIX_SIMULATOR_SIMULATORFRONTEND_H_

#include "Host/SimulatorSerialHost.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "Web/SimulatorWeb.h"

/*!
 * \brief Where the simulator meets a person or a program: the terminal, the
 *        page and the serial port on TCP.
 *
 * The panel goes to the page with `--web`, else to the terminal, unless a run
 * with `--for` prints it only at its end. The sketch's `Serial` goes to the
 * page as well, and with `--tcp` to the control program.
 */
class SimulatorFrontEnd {
 private:
  SimulatorWeb web_;
  SimulatorSerialHost serial_host_;
  bool web_on_;
  bool terminal_on_;

 public:
  /*! \brief Makes a front end that shows nothing until Begin(). */
  SimulatorFrontEnd();

  /*!
   * \brief Opens the ends the options ask for.
   *
   * It opens the port for the page and the one for the control program, and
   * says where they are on stdout, or clears the terminal for a run that draws
   * in it.
   *
   * \param[in] options The command line.
   * \return True once every end is open, or false if a port is taken, which it
   *         says on stderr.
   */
  bool Begin(SimulatorOptions const& options);

  /*!
   * \brief Follows the sketch.
   *
   * It hands the page and the control program what the sketch has printed,
   * carries out what they have sent, and draws the panel when it is due. It is
   * called after every frame and every millisecond, as the page wants every
   * frame logged.
   *
   * \param[in,out] panel The panel to draw, which the page's commands change.
   * \param[in] now The time of the sketch, in milliseconds.
   */
  void Follow(SimulatorPanel& panel, uint32_t now);
};

#endif  // MAXMATRIX_SIMULATOR_SIMULATORFRONTEND_H_
