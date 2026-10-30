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
/**     \file       SimulatorWeb.h
 *      \brief      The simulator's panel in a browser
 *
 *      \details    What the page in simulator/index.html is sent - the panel, the brightness of every module, the frames on
 *                  the wire - and what it sends back: the wiring and the mounting to choose while the sketch runs.
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_WEB_H_
#define _SIMULATOR_WEB_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include <vector>
#include "MaxMatrixStandardTypes.h"
#include "SimulatorPanel.h"
#include "SimulatorWebHost.h"


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
    SimulatorWebHost Host;
    /* the frames since the page was last sent the panel, as hex pairs, one string per frame */
    std::vector<std::string> FrameLog;
    int32_t LoggedFrames;

    void logFrame();
    void command(SimulatorPanel&, const std::string&);
    std::string getMessage(const SimulatorPanel&) const;
    std::string getLog() const;
    static std::string getIntensities(const SimulatorPanel&);

  public:
    SimulatorWeb();

    // methods
    stdReturnType begin(uint16_t, const char*);
    void task(SimulatorPanel&);
    void send(const SimulatorPanel&);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
