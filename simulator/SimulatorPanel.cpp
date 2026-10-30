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
#define _SIMULATOR_PANEL_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "SimulatorPanel.h"


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
/* the four wirings MD_MAX72XX has names for, beside DR0CR0RR0 ... DR1CR1RR1 from Max7219Wirings */
static const struct {
    const char* Name;
    const Max7219WiringType* Wiring;
} NamedWirings[] = {
    { "FC16", &MAX7219_WIRING_FC16 },
    { "GENERIC", &MAX7219_WIRING_GENERIC },
    { "PAROLA", &MAX7219_WIRING_PAROLA },
    { "ICSTATION", &MAX7219_WIRING_ICSTATION },
};


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorPanel
******************************************************************************************************************************************************/
/*! \brief          SimulatorPanel constructor
 *  \details        Instantiation of a panel of the default wiring, mounted upright, that has never been drawn
 *
 *  \return         -
 *****************************************************************************************************************************************************/
SimulatorPanel::SimulatorPanel()
{
    WiringName = SIMULATOR_PANEL_DEFAULT_WIRING;
    Wiring = findWiring(WiringName);
    UpsideDown = false;
    DrawnFrames = -1;
    LastDraw = 0;
    DrawRequested = true;
} /* SimulatorPanel */


/******************************************************************************************************************************************************
  getText()
******************************************************************************************************************************************************/
/*! \brief          the panel as text
 *  \details        this function writes the LEDs row after row, as the chosen module lights them
 *
 *  \param[in]      Lit             what stands for a lit LED
 *  \param[in]      Dark            what stands for a dark LED
 *  \param[in]      EndOfRow        what follows every row
 *  \return         the panel
 *****************************************************************************************************************************************************/
std::string SimulatorPanel::getText(const char* Lit, const char* Dark, const char* EndOfRow) const
{
    std::string Text;

    for(uint8_t Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        for(uint8_t X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) Text += Mock::Chain.isLit(*Wiring, X, Y, UpsideDown) ? Lit : Dark;
        Text += EndOfRow;
    }
    return Text;
} /* getText */


/******************************************************************************************************************************************************
  getIntensity()
******************************************************************************************************************************************************/
/*! \brief          the brightness of a module
 *  \details        this function asks the chip that lights the module at this place, which depends on the mounting
 *
 *  \param[in]      Module          the module's place, counted from the left as the viewer sees the panel
 *  \return         the intensity register of its chip
 *****************************************************************************************************************************************************/
uint8_t SimulatorPanel::getIntensity(uint8_t Module) const
{
    return Mock::Chain.chipAt(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, UpsideDown).Intensity;
} /* getIntensity */


/******************************************************************************************************************************************************
  setWiring()
******************************************************************************************************************************************************/
/*! \brief          chooses the wiring of the modules
 *  \details        this function accepts the four names MD_MAX72XX has and DR0CR0RR0 ... DR1CR1RR1
 *
 *  \param[in]      Name            the wiring's name
 *  \return         E_OK
 *                  E_NOT_OK        no wiring has this name; the panel keeps the one it has
 *****************************************************************************************************************************************************/
stdReturnType SimulatorPanel::setWiring(const std::string& Name)
{
    const Max7219WiringType* Found = findWiring(Name);

    if(Found != nullptr) {
        Wiring = Found;
        WiringName = Name;
        DrawRequested = true;
        return E_OK;
    } else return E_NOT_OK;
} /* setWiring */


/******************************************************************************************************************************************************
  setUpsideDown()
******************************************************************************************************************************************************/
/*! \brief          chooses how the board is mounted
 *  \details        this function turns the panel, so that the next draw shows it as a board mounted this way lights it
 *
 *  \param[in]      sUpsideDown     true if the board is mounted upside down
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorPanel::setUpsideDown(bool sUpsideDown)
{
    UpsideDown = sUpsideDown;
    DrawRequested = true;
} /* setUpsideDown */


/******************************************************************************************************************************************************
  isDrawDue()
******************************************************************************************************************************************************/
/*! \brief          says whether the panel has to be drawn
 *  \details        this function says so when a frame arrived or a draw was requested since the last draw, but no more
 *                  often than every SIMULATOR_PANEL_REDRAW_INTERVAL
 *
 *  \param[in]      Now             millis()
 *  \return         true if it is time to draw
 *****************************************************************************************************************************************************/
bool SimulatorPanel::isDrawDue(uint32_t Now) const
{
    const bool Changed = Mock::Frames != DrawnFrames || DrawRequested;

    return Changed && Now - LastDraw >= SIMULATOR_PANEL_REDRAW_INTERVAL;
} /* isDrawDue */


/******************************************************************************************************************************************************
  setDrawn()
******************************************************************************************************************************************************/
/*! \brief          notes that the panel was drawn
 *  \details        this function remembers the frame count and the time, so that isDrawDue() waits for the next change
 *
 *  \param[in]      Now             millis()
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorPanel::setDrawn(uint32_t Now)
{
    DrawnFrames = Mock::Frames;
    LastDraw = Now;
    DrawRequested = false;
} /* setDrawn */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  findWiring()
******************************************************************************************************************************************************/
/*! \brief          finds a wiring by its name
 *  \details        this function looks at the names MD_MAX72XX has first, then at DR0CR0RR0 ... DR1CR1RR1
 *
 *  \param[in]      Name            the wiring's name
 *  \return         the wiring, or nullptr if no wiring has this name
 *****************************************************************************************************************************************************/
const Max7219WiringType* SimulatorPanel::findWiring(const std::string& Name)
{
    for(const auto& Named : NamedWirings) {
        if(Name == Named.Name) return Named.Wiring;
    }
    for(const Max7219WiringType& Each : Max7219Wirings) {
        if(Name == Each.Name) return &Each;
    }
    return nullptr;
} /* findWiring */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
