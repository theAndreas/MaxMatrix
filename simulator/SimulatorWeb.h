// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in a browser.
 *
 * What the page in simulator/index.html is sent - the panel, the brightness of every module, the
 * frames on the wire - and what it sends back: the wiring and the mounting to choose while the
 * sketch runs.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORWEB_H_
#define MAXMATRIX_SIMULATOR_SIMULATORWEB_H_

#include <string>
#include <vector>
#include "SimulatorPanel.h"
#include "SimulatorWebHost.h"

/* SimulatorWeb Parameter */
/* the most frames the page is sent at once; more would scroll past before anyone read them */
#define SIMULATOR_WEB_FRAME_LOG_LENGTH                              16u

/* Every function that needs the panel is handed it, as MaxMatrixText is, rather than the class keeping a reference: the
 * panel belongs to the simulator, and the terminal is handed the same one. */
class SimulatorWeb
{
  private:
    SimulatorWebHost host_;
    /* the frames since the page was last sent the panel, as hex pairs, one string per frame */
    std::vector<std::string> frame_log_;
    int32_t logged_frames_;

    void LogFrame();
    static void Command(SimulatorPanel&, std::string const&);
    std::string GetMessage(SimulatorPanel const&) const;
    std::string GetLog() const;
    static std::string GetIntensities(SimulatorPanel const&);

  public:
    SimulatorWeb();

    // methods
    bool Begin(uint16_t, char const*);
    void Task(SimulatorPanel&);
    void Send(SimulatorPanel const&);
    static std::string ConvertToJsonString(std::string const&);
};

#endif
