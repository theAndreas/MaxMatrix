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
/**     \file       SimulatorPanel.h
 *      \brief      The LED panel of the simulator, as a module of the chosen wiring lights it
 *
 *      \details    Reads the model's LEDs rather than MatrixBuffer, so a sketch with the wrong orientation for its module
 *                  shows it wrong here too - which is the point. Knows nothing of where the panel is drawn.
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_PANEL_H_
#define _SIMULATOR_PANEL_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include "Arduino.h"
#include "MaxMatrixConfig.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorPanel Parameter */
#define SIMULATOR_PANEL_DEFAULT_WIRING                              "FC16"
/* about 30 pictures a second; neither a terminal nor a page keeps up with every frame */
#define SIMULATOR_PANEL_REDRAW_INTERVAL                             33u


/******************************************************************************************************************************************************
 *  CLASS  SimulatorPanel
 *****************************************************************************************************************************************************/
/* The panel also keeps when it was drawn last, because whether it has to be drawn again depends on what changed on it:
 * a frame, the wiring or the mounting. */
class SimulatorPanel
{
  private:
    std::string WiringName;
    const Max7219WiringType* Wiring;
    bool UpsideDown;
    int DrawnFrames;
    unsigned long LastDraw;
    bool DrawRequested;

    static const Max7219WiringType* findWiring(const std::string&);

  public:
    SimulatorPanel();

    // get methods
    const std::string& getWiringName() const { return WiringName; }
    bool isUpsideDown() const { return UpsideDown; }
    std::string getText(const char*, const char*, const char*) const;
    unsigned getIntensity(unsigned) const;

    // set methods
    stdReturnType setWiring(const std::string&);
    void setUpsideDown(bool);

    // methods
    void requestDraw() { DrawRequested = true; }
    bool isDrawDue(unsigned long) const;
    void setDrawn(unsigned long);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
