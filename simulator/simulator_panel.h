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
/**     \file       simulator_panel.h
 *      \brief      The LED panel of the simulator, as a module of the chosen wiring lights it
 *
 *      \details    Reads the model's LEDs rather than matrix_buffer_, so a sketch with the wrong orientation for its module
 *                  shows it wrong here too - which is the point. Knows nothing of where the panel is drawn.
 *
 *****************************************************************************************************************************************************/
#ifndef MAXMATRIX_SIMULATOR_SIMULATOR_PANEL_H_
#define MAXMATRIX_SIMULATOR_SIMULATOR_PANEL_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include "Arduino.h"
#include "max_matrix_config.h"


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
    std::string wiring_name_;
    Max7219Wiring const* wiring_;
    bool upside_down_;
    int32_t drawn_frames_;
    uint32_t last_draw_;
    bool draw_requested_;

    static Max7219Wiring const* FindWiring(std::string const&);

  public:
    SimulatorPanel();

    // get methods
    std::string const& GetWiringName() const { return wiring_name_; }
    bool IsUpsideDown() const { return upside_down_; }
    std::string GetText(char const*, char const*, char const*) const;
    uint8_t GetIntensity(uint8_t) const;

    // set methods
    bool SetWiring(std::string const&);
    void SetUpsideDown(bool);

    // methods
    void RequestDraw() { draw_requested_ = true; }
    bool IsDrawDue(uint32_t) const;
    void SetDrawn(uint32_t);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
