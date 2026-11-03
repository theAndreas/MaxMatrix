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
/**     \file       SimulatorPanel.cpp
 *      \brief      The LED panel of the simulator, as a module of the chosen wiring lights it
 *
 *      \details    The wiring by name, the panel as text, the brightness of every module, and whether it is time to draw.
 *
 *****************************************************************************************************************************************************/
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "SimulatorPanel.h"


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
/* the four wirings MD_MAX72XX has names for, beside DR0CR0RR0 ... DR1CR1RR1 from kMax7219Wirings */
static struct {
    char const* name;
    Max7219Wiring const* wiring;
} const kNamedWirings[] = {
    { "FC16", &kMaX7219WiringFC16 },
    { "GENERIC", &kMaX7219WiringGeneric },
    { "PAROLA", &kMaX7219WiringParola },
    { "ICSTATION", &kMaX7219WiringIcstation },
};


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorPanel
******************************************************************************************************************************************************/
/*! \brief          SimulatorPanel constructor
 *  \details        Instantiation of a panel of the default wiring, mounted upright, that has never been drawn
 *****************************************************************************************************************************************************/
SimulatorPanel::SimulatorPanel()
{
    wiring_name_ = SIMULATOR_PANEL_DEFAULT_WIRING;
    wiring_ = FindWiring(wiring_name_);
    upside_down_ = false;
    drawn_frames_ = -1;
    last_draw_ = 0;
    draw_requested_ = true;
} /* SimulatorPanel */


/******************************************************************************************************************************************************
  GetText()
******************************************************************************************************************************************************/
/*! \brief          the panel as text
 *  \details        this function writes the LEDs row after row, as the chosen module lights them
 *
 *  \param[in]      lit             what stands for a lit LED
 *  \param[in]      dark            what stands for a dark LED
 *  \param[in]      end_of_row      what follows every row
 *  \return         the panel
 *****************************************************************************************************************************************************/
std::string SimulatorPanel::GetText(char const* lit, char const* dark, char const* end_of_row) const
{
    std::string text;

    for(uint8_t y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
        for(uint8_t x = 0; x < MAXMATRIX_NUMBER_OF_COLUMNS; x++) text += mock::chain.IsLit(*wiring_, x, y, upside_down_) ? lit : dark;
        text += end_of_row;
    }
    return text;
} /* GetText */


/******************************************************************************************************************************************************
  GetIntensity()
******************************************************************************************************************************************************/
/*! \brief          the brightness of a module
 *  \details        this function asks the chip that lights the module at this place, which depends on the mounting
 *
 *  \param[in]      module          the module's place, counted from the left as the viewer sees the panel
 *  \return         the intensity register of its chip
 *****************************************************************************************************************************************************/
uint8_t SimulatorPanel::GetIntensity(uint8_t module) const
{
    return mock::chain.ChipAt(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, upside_down_).intensity;
} /* GetIntensity */


/******************************************************************************************************************************************************
  SetWiring()
******************************************************************************************************************************************************/
/*! \brief          chooses the wiring of the modules
 *  \details        this function accepts the four names MD_MAX72XX has and DR0CR0RR0 ... DR1CR1RR1
 *
 *  \param[in]      name            the wiring's name
 *  \return         true
 *                  false           no wiring has this name; the panel keeps the one it has
 *****************************************************************************************************************************************************/
bool SimulatorPanel::SetWiring(std::string const& name)
{
    Max7219Wiring const* found = FindWiring(name);

    if(found != nullptr) {
        wiring_ = found;
        wiring_name_ = name;
        draw_requested_ = true;
        return true;
    } else return false;
} /* SetWiring */


/******************************************************************************************************************************************************
  SetUpsideDown()
******************************************************************************************************************************************************/
/*! \brief          chooses how the board is mounted
 *  \details        this function turns the panel, so that the next draw shows it as a board mounted this way lights it
 *
 *  \param[in]      upside_down     true if the board is mounted upside down
 *****************************************************************************************************************************************************/
void SimulatorPanel::SetUpsideDown(bool upside_down)
{
    upside_down_ = upside_down;
    draw_requested_ = true;
} /* SetUpsideDown */


/******************************************************************************************************************************************************
  IsDrawDue()
******************************************************************************************************************************************************/
/*! \brief          says whether the panel has to be drawn
 *  \details        this function says so when a frame arrived or a draw was requested since the last draw, but no more
 *                  often than every SIMULATOR_PANEL_REDRAW_INTERVAL
 *
 *  \param[in]      now             millis()
 *  \return         true if it is time to draw
 *****************************************************************************************************************************************************/
bool SimulatorPanel::IsDrawDue(uint32_t now) const
{
    bool const changed = mock::frames != drawn_frames_ || draw_requested_;

    return changed && now - last_draw_ >= SIMULATOR_PANEL_REDRAW_INTERVAL;
} /* IsDrawDue */


/******************************************************************************************************************************************************
  SetDrawn()
******************************************************************************************************************************************************/
/*! \brief          notes that the panel was drawn
 *  \details        this function remembers the frame count and the time, so that IsDrawDue() waits for the next change
 *
 *  \param[in]      now             millis()
 *****************************************************************************************************************************************************/
void SimulatorPanel::SetDrawn(uint32_t now)
{
    drawn_frames_ = mock::frames;
    last_draw_ = now;
    draw_requested_ = false;
} /* SetDrawn */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  FindWiring()
******************************************************************************************************************************************************/
/*! \brief          finds a wiring by its name
 *  \details        this function looks at the names MD_MAX72XX has first, then at DR0CR0RR0 ... DR1CR1RR1
 *
 *  \param[in]      name            the wiring's name
 *  \return         the wiring, or nullptr if no wiring has this name
 *****************************************************************************************************************************************************/
Max7219Wiring const* SimulatorPanel::FindWiring(std::string const& name)
{
    for(auto const& named : kNamedWirings) {
        if(name == named.name) return named.wiring;
    }
    for(Max7219Wiring const& each : kMax7219Wirings) {
        if(name == each.name) return &each;
    }
    return nullptr;
} /* FindWiring */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
