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
/**     \file       SimulatorWeb.cpp
 *      \brief      The simulator's panel in a browser
 *
 *      \details    The page is sent JSON; it sends back "wiring NAME", "upside-down 0|1", and "hello" when it opens, which
 *                  asks for the panel.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_WEB_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <sstream>
#include "SimulatorWeb.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* commands of the page */
#define SIMULATOR_WEB_COMMAND_WIRING                                "wiring "
#define SIMULATOR_WEB_COMMAND_UPRIGHT                               "upside-down 0"
#define SIMULATOR_WEB_COMMAND_UPSIDE_DOWN                           "upside-down 1"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorWeb
******************************************************************************************************************************************************/
/*! \brief          SimulatorWeb constructor
 *  \details        Instantiation of a web view that has logged no frame yet
 *
 *  \return         -
 *****************************************************************************************************************************************************/
SimulatorWeb::SimulatorWeb()
{
    LoggedFrames = 0;
} /* SimulatorWeb */


/******************************************************************************************************************************************************
  begin()
******************************************************************************************************************************************************/
/*! \brief          serves the page
 *  \details        this function opens the port on localhost
 *
 *  \param[in]      Port            the port to listen on
 *  \param[in]      PagePath        the page to serve
 *  \return         E_OK
 *                  E_NOT_OK        the port is taken
 *****************************************************************************************************************************************************/
stdReturnType SimulatorWeb::begin(unsigned short Port, const char* PagePath)
{
    return Host.begin(Port, PagePath);
} /* begin */


/******************************************************************************************************************************************************
  task()
******************************************************************************************************************************************************/
/*! \brief          follows the sketch and the page
 *  \details        this function logs a new frame and carries out what the page has sent. It is called after every
 *                  frame, so each one gets logged and the page shows them as they pass.
 *
 *  \param[in,out]  Panel           the panel the page's commands change
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWeb::task(SimulatorPanel& Panel)
{
    std::vector<std::string> Messages;

    if(Mock::Frames != LoggedFrames) logFrame();
    Host.task(&Messages);
    for(const std::string& Message : Messages) command(Panel, Message);
} /* task */


/******************************************************************************************************************************************************
  send()
******************************************************************************************************************************************************/
/*! \brief          sends the panel to the page
 *  \details        this function sends the panel with the frames logged since the last time, and starts a new log
 *
 *  \param[in]      Panel           the panel to send
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWeb::send(const SimulatorPanel& Panel)
{
    Host.broadcast(getMessage(Panel));
    FrameLog.clear();
} /* send */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  logFrame()
******************************************************************************************************************************************************/
/*! \brief          logs the last frame
 *  \details        this function writes the frame as address/value pairs, one pair per module, and keeps no more than
 *                  SIMULATOR_WEB_FRAME_LOG_LENGTH frames
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWeb::logFrame()
{
    std::string Line = "#" + std::to_string(Mock::Frames) + ":";
    char Pair[8];

    for(size_t i = 0; i + 1 < Mock::LastFrame.size(); i += 2) {
        snprintf(Pair, sizeof(Pair), "%02X %02X", Mock::LastFrame[i], Mock::LastFrame[i + 1]);
        Line += i == 0 ? " " : " | ";
        Line += Pair;
    }
    FrameLog.push_back(Line);
    if(FrameLog.size() > SIMULATOR_WEB_FRAME_LOG_LENGTH) FrameLog.erase(FrameLog.begin());
    LoggedFrames = Mock::Frames;
} /* logFrame */


/******************************************************************************************************************************************************
  command()
******************************************************************************************************************************************************/
/*! \brief          carries out a command of the page
 *  \details        this function changes the wiring or the mounting. Every message asks for the panel, also one that
 *                  changes nothing, which is what "hello" is for; an unknown wiring keeps the one the panel has.
 *
 *  \param[in,out]  Panel           the panel to change
 *  \param[in]      Message         the command
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWeb::command(SimulatorPanel& Panel, const std::string& Message)
{
    const std::string WiringCommand = SIMULATOR_WEB_COMMAND_WIRING;

    if(Message.compare(0, WiringCommand.size(), WiringCommand) == 0) Panel.setWiring(Message.substr(WiringCommand.size()));
    else if(Message == SIMULATOR_WEB_COMMAND_UPSIDE_DOWN) Panel.setUpsideDown(true);
    else if(Message == SIMULATOR_WEB_COMMAND_UPRIGHT) Panel.setUpsideDown(false);
    Panel.requestDraw();
} /* command */


/******************************************************************************************************************************************************
  getMessage()
******************************************************************************************************************************************************/
/*! \brief          the message for the page
 *  \details        this function writes the panel as JSON: the wiring and the mounting, the size, the dots row after row
 *                  as '1' and '0', the brightness of each module from the left, the frame count, the transfer errors
 *                  and the frames since the last message
 *
 *  \param[in]      Panel           the panel to send
 *  \return         the JSON text
 *****************************************************************************************************************************************************/
std::string SimulatorWeb::getMessage(const SimulatorPanel& Panel) const
{
    std::ostringstream Json;

    Json << "{\"wiring\":\"" << Panel.getWiringName() << "\",\"upsideDown\":" << (Panel.isUpsideDown() ? "true" : "false")
         << ",\"columns\":" << MAXMATRIX_NUMBER_OF_COLUMNS << ",\"rows\":" << MAXMATRIX_ROW_NUMBER_OF_MODULE
         << ",\"dots\":\"" << Panel.getText("1", "0", "") << "\""
         << ",\"intensity\":[" << getIntensities(Panel) << "]"
         << ",\"frames\":" << Mock::Frames << ",\"errors\":" << Mock::Errors
         << ",\"log\":[" << getLog() << "]}";
    return Json.str();
} /* getMessage */


/******************************************************************************************************************************************************
  getLog()
******************************************************************************************************************************************************/
/*! \brief          the logged frames for the message
 *  \details        this function quotes every logged frame; the frames hold nothing a JSON string would have to escape
 *
 *  \return         the frames as JSON strings, separated by commas
 *****************************************************************************************************************************************************/
std::string SimulatorWeb::getLog() const
{
    std::string Log;

    for(size_t i = 0; i < FrameLog.size(); i++) Log += (i > 0 ? ",\"" : "\"") + FrameLog[i] + "\"";
    return Log;
} /* getLog */


/******************************************************************************************************************************************************
  getIntensities()
******************************************************************************************************************************************************/
/*! \brief          the brightness of every module for the message
 *  \details        this function counts the modules as the viewer sees them, from the left, whatever the mounting
 *
 *  \param[in]      Panel           the panel
 *  \return         the intensities from the left, separated by commas
 *****************************************************************************************************************************************************/
std::string SimulatorWeb::getIntensities(const SimulatorPanel& Panel)
{
    std::string Intensities;

    for(unsigned Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        Intensities += (Module > 0 ? "," : "") + std::to_string(Panel.getIntensity(Module));
    }
    return Intensities;
} /* getIntensities */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
