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
 *      \details    The page is sent JSON; it sends back "wiring NAME", "upside-down 0|1", "serial LINE", which reaches the
 *                  sketch's Serial as a line, and "hello" when it opens, which asks for the panel.
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
#define SIMULATOR_WEB_COMMAND_SERIAL                                "serial "

/* JSON, RFC 8259: the printable ASCII range; the rest is written as \u00XX */
#define SIMULATOR_WEB_JSON_FIRST_PLAIN                              0x20u
#define SIMULATOR_WEB_JSON_LAST_PLAIN                               0x7Eu


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
stdReturnType SimulatorWeb::begin(uint16_t Port, const char* PagePath)
{
    return Host.begin(Port, PagePath);
} /* begin */


/******************************************************************************************************************************************************
  task()
******************************************************************************************************************************************************/
/*! \brief          follows the sketch and the page
 *  \details        this function logs a new frame and carries out what the page has sent. It is called after every
 *                  frame, so each one gets logged and the page shows them as they pass. What the sketch has printed asks
 *                  for the panel too: an answer such as "ok" changes no LED, and would otherwise wait for the next frame.
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
    if(!Mock::SerialOutput.empty()) Panel.requestDraw();
} /* task */


/******************************************************************************************************************************************************
  send()
******************************************************************************************************************************************************/
/*! \brief          sends the panel to the page
 *  \details        this function sends the panel with the frames logged and what the sketch printed since the last time,
 *                  and starts over with both
 *
 *  \param[in]      Panel           the panel to send
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWeb::send(const SimulatorPanel& Panel)
{
    Host.broadcast(getMessage(Panel));
    FrameLog.clear();
    Mock::SerialOutput.clear();
} /* send */


/******************************************************************************************************************************************************
  convertToJsonString()
******************************************************************************************************************************************************/
/*! \brief          converts text into the inside of a JSON string
 *  \details        this function escapes quotes and backslashes and writes every byte outside printable ASCII as \u00XX.
 *                  A sketch may print anything, latin-1 included, and a WebSocket text frame that is not valid UTF-8 makes
 *                  the browser close the connection; read as latin-1, every byte is a valid char.
 *
 *  \param[in]      Text            the bytes, as the sketch printed them
 *  \return         the text for between the quotes of a JSON string
 *****************************************************************************************************************************************************/
std::string SimulatorWeb::convertToJsonString(const std::string& Text)
{
    std::string Json;
    char Escaped[8];

    for(const char Char : Text) {
        const uint8_t Byte = static_cast<uint8_t>(Char);
        if(Char == '"' || Char == '\\') {
            Json += '\\';
            Json += Char;
        } else if(Byte >= SIMULATOR_WEB_JSON_FIRST_PLAIN && Byte <= SIMULATOR_WEB_JSON_LAST_PLAIN) {
            Json += Char;
        } else {
            snprintf(Escaped, sizeof(Escaped), "\\u%04X", static_cast<uint16_t>(Byte));
            Json += Escaped;
        }
    }
    return Json;
} /* convertToJsonString */


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
 *  \details        this function changes the wiring or the mounting, or hands a line to the sketch's Serial, as a
 *                  control program would send it to a board. Every message asks for the panel, also one that changes
 *                  nothing, which is what "hello" is for; an unknown wiring keeps the one the panel has.
 *
 *  \param[in,out]  Panel           the panel to change
 *  \param[in]      Message         the command
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWeb::command(SimulatorPanel& Panel, const std::string& Message)
{
    const std::string WiringCommand = SIMULATOR_WEB_COMMAND_WIRING;
    const std::string SerialCommand = SIMULATOR_WEB_COMMAND_SERIAL;

    if(Message.compare(0, WiringCommand.size(), WiringCommand) == 0) Panel.setWiring(Message.substr(WiringCommand.size()));
    else if(Message.compare(0, SerialCommand.size(), SerialCommand) == 0) Mock::SerialInput += Message.substr(SerialCommand.size()) + "\n";
    else if(Message == SIMULATOR_WEB_COMMAND_UPSIDE_DOWN) Panel.setUpsideDown(true);
    else if(Message == SIMULATOR_WEB_COMMAND_UPRIGHT) Panel.setUpsideDown(false);
    Panel.requestDraw();
} /* command */


/******************************************************************************************************************************************************
  getMessage()
******************************************************************************************************************************************************/
/*! \brief          the message for the page
 *  \details        this function writes the panel as JSON: the wiring and the mounting, the size, the dots row after row
 *                  as '1' and '0', the brightness of each module from the left, the frame count, the transfer errors,
 *                  the frames since the last message and what the sketch printed since then
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
         << ",\"log\":[" << getLog() << "]"
         << ",\"serial\":\"" << convertToJsonString(Mock::SerialOutput) << "\"}";
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

    for(uint8_t Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        Intensities += (Module > 0 ? "," : "") + std::to_string(Panel.getIntensity(Module));
    }
    return Intensities;
} /* getIntensities */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
