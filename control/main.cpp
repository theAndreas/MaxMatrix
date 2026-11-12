// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The control program of MaxMatrix: SerialControl on a board or in the
 *        simulator, from a window.
 *
 * build/MaxMatrixControl
 *
 * The control of the simulator's page as a program of its own, for a browser
 * without Web Serial and for whoever wants a program rather than a page; OIL
 * item 26.
 */

#include <wx/app.h>

#include "ControlWindow.h"

/*!
 * \brief The control program: one window.
 */
class ControlApp : public wxApp {  // NOLINT(misc-use-internal-linkage): the
  // wxGetApp() of wxIMPLEMENT_APP would go unused in an anonymous namespace.

 public:
  /*!
   * \brief Opens the window.
   *
   * \return True, as there is nothing that could keep the program from
   *         starting.
   */
  bool OnInit() override {
    (new ControlWindow())->Show();
    return true;
  }
};

wxIMPLEMENT_APP(ControlApp);
