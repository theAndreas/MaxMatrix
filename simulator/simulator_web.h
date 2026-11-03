/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       simulator_web.h
 *      \brief      The simulator's panel in a browser
 *
 *      \details    What the page in simulator/index.html is sent - the panel, the brightness of every module, the frames on
 *                  the wire - and what it sends back: the wiring and the mounting to choose while the sketch runs.
 *
 *****************************************************************************************************************************************************/
#ifndef MAXMATRIX_SIMULATOR_SIMULATOR_WEB_H_
#define MAXMATRIX_SIMULATOR_SIMULATOR_WEB_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include <vector>
#include "simulator_panel.h"
#include "simulator_web_host.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorWeb Parameter */
/* the most frames the page is sent at once; more would scroll past before anyone read them */
#define SIMULATOR_WEB_FRAME_LOG_LENGTH                              16u


/******************************************************************************************************************************************************
 *  CLASS  SimulatorWeb
 *****************************************************************************************************************************************************/
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

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
