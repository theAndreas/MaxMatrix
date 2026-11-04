// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in a browser.
 *
 * The page is sent JSON; it sends back "wiring NAME", "upside-down 0|1", "serial LINE", which
 * reaches the sketch's Serial as a line, and "hello" when it opens, which asks for the panel.
 */

#include <stdio.h>
#include <sstream>
#include "SimulatorWeb.h"

/* commands of the page */
#define SIMULATOR_WEB_COMMAND_WIRING                                "wiring "
#define SIMULATOR_WEB_COMMAND_UPRIGHT                               "upside-down 0"
#define SIMULATOR_WEB_COMMAND_UPSIDE_DOWN                           "upside-down 1"
#define SIMULATOR_WEB_COMMAND_SERIAL                                "serial "

/* JSON, RFC 8259: the printable ASCII range; the rest is written as \u00XX */
#define SIMULATOR_WEB_JSON_FIRST_PLAIN                              0x20u
#define SIMULATOR_WEB_JSON_LAST_PLAIN                               0x7Eu

/*! \brief          SimulatorWeb constructor
 *  \details        Instantiation of a web view that has logged no frame yet
 */
SimulatorWeb::SimulatorWeb()
{
    logged_frames_ = 0;
}

/*! \brief          serves the page
 *  \details        this function opens the port on localhost
 *
 *  \param[in]      port            the port to listen on
 *  \param[in]      page_path       the page to serve
 *  \return         true
 *                  false           the port is taken
 */
bool SimulatorWeb::Begin(uint16_t port, char const* page_path)
{
    return host_.Begin(port, page_path);
}

/*! \brief          follows the sketch and the page
 *  \details        this function logs a new frame and carries out what the page has sent. It is called after every
 *                  frame, so each one gets logged and the page shows them as they pass. What the sketch has printed asks
 *                  for the panel too: an answer such as "ok" changes no LED, and would otherwise wait for the next frame.
 *
 *  \param[in,out]  panel           the panel the page's commands change
 */
void SimulatorWeb::Task(SimulatorPanel& panel)
{
    std::vector<std::string> messages;

    if(mock::frames != logged_frames_) LogFrame();
    host_.Task(&messages);
    for(std::string const& message : messages) Command(panel, message);
    if(!mock::serial_output.empty()) panel.RequestDraw();
}

/*! \brief          sends the panel to the page
 *  \details        this function sends the panel with the frames logged and what the sketch printed since the last time,
 *                  and starts over with both
 *
 *  \param[in]      panel           the panel to send
 */
void SimulatorWeb::Send(SimulatorPanel const& panel)
{
    host_.Broadcast(GetMessage(panel));
    frame_log_.clear();
    mock::serial_output.clear();
}

/*! \brief          converts text into the inside of a JSON string
 *  \details        this function escapes quotes and backslashes and writes every byte outside printable ASCII as \u00XX.
 *                  A sketch may print anything, latin-1 included, and a WebSocket text frame that is not valid UTF-8 makes
 *                  the browser close the connection; read as latin-1, every byte is a valid char.
 *
 *  \param[in]      text            the bytes, as the sketch printed them
 *  \return         the text for between the quotes of a JSON string
 */
std::string SimulatorWeb::ConvertToJsonString(std::string const& text)
{
    std::string json;
    char escaped[8];

    for(char const c : text) {
        uint8_t const code = static_cast<uint8_t>(c);
        if(c == '"' || c == '\\') {
            json += '\\';
            json += c;
        } else if(code >= SIMULATOR_WEB_JSON_FIRST_PLAIN && code <= SIMULATOR_WEB_JSON_LAST_PLAIN) {
            json += c;
        } else {
            snprintf(escaped, sizeof(escaped), "\\u%04X", static_cast<uint16_t>(code));
            json += escaped;
        }
    }
    return json;
}

/*! \brief          logs the last frame
 *  \details        this function writes the frame as address/value pairs, one pair per module, and keeps no more than
 *                  SIMULATOR_WEB_FRAME_LOG_LENGTH frames
 */
void SimulatorWeb::LogFrame()
{
    std::string line = "#" + std::to_string(mock::frames) + ":";
    char pair[8];

    for(size_t i = 0; i + 1 < mock::last_frame.size(); i += 2) {
        snprintf(pair, sizeof(pair), "%02X %02X", mock::last_frame[i], mock::last_frame[i + 1]);
        line += i == 0 ? " " : " | ";
        line += pair;
    }
    frame_log_.push_back(line);
    if(frame_log_.size() > SIMULATOR_WEB_FRAME_LOG_LENGTH) frame_log_.erase(frame_log_.begin());
    logged_frames_ = mock::frames;
}

/*! \brief          carries out a command of the page
 *  \details        this function changes the wiring or the mounting, or hands a line to the sketch's Serial, as a
 *                  control program would send it to a board. Every message asks for the panel, also one that changes
 *                  nothing, which is what "hello" is for; an unknown wiring keeps the one the panel has.
 *
 *  \param[in,out]  panel           the panel to change
 *  \param[in]      message         the command
 */
void SimulatorWeb::Command(SimulatorPanel& panel, std::string const& message)
{
    std::string const wiring_command = SIMULATOR_WEB_COMMAND_WIRING;
    std::string const serial_command = SIMULATOR_WEB_COMMAND_SERIAL;

    if(message.compare(0, wiring_command.size(), wiring_command) == 0) panel.SetWiring(message.substr(wiring_command.size()));
    else if(message.compare(0, serial_command.size(), serial_command) == 0) mock::serial_input += message.substr(serial_command.size()) + "\n";
    else if(message == SIMULATOR_WEB_COMMAND_UPSIDE_DOWN) panel.SetUpsideDown(true);
    else if(message == SIMULATOR_WEB_COMMAND_UPRIGHT) panel.SetUpsideDown(false);
    panel.RequestDraw();
}

/*! \brief          the message for the page
 *  \details        this function writes the panel as JSON: the wiring and the mounting, the size, the dots row after row
 *                  as '1' and '0', the brightness of each module from the left, the frame count, the transfer errors,
 *                  the frames since the last message and what the sketch printed since then
 *
 *  \param[in]      panel           the panel to send
 *  \return         the JSON text
 */
std::string SimulatorWeb::GetMessage(SimulatorPanel const& panel) const
{
    std::ostringstream json;

    json << "{\"wiring\":\"" << panel.GetWiringName() << "\",\"upsideDown\":" << (panel.IsUpsideDown() ? "true" : "false")
         << ",\"columns\":" << MAXMATRIX_NUMBER_OF_COLUMNS << ",\"rows\":" << MAXMATRIX_ROW_NUMBER_OF_MODULE
         << ",\"dots\":\"" << panel.GetText("1", "0", "") << "\""
         << ",\"intensity\":[" << GetIntensities(panel) << "]"
         << ",\"frames\":" << mock::frames << ",\"errors\":" << mock::errors
         << ",\"log\":[" << GetLog() << "]"
         << ",\"serial\":\"" << ConvertToJsonString(mock::serial_output) << "\"}";
    return json.str();
}

/*! \brief          the logged frames for the message
 *  \details        this function quotes every logged frame; the frames hold nothing a JSON string would have to escape
 *
 *  \return         the frames as JSON strings, separated by commas
 */
std::string SimulatorWeb::GetLog() const
{
    std::string log;

    for(size_t i = 0; i < frame_log_.size(); i++) log += (i > 0 ? ",\"" : "\"") + frame_log_[i] + "\"";
    return log;
}

/*! \brief          the brightness of every module for the message
 *  \details        this function counts the modules as the viewer sees them, from the left, whatever the mounting
 *
 *  \param[in]      panel           the panel
 *  \return         the intensities from the left, separated by commas
 */
std::string SimulatorWeb::GetIntensities(SimulatorPanel const& panel)
{
    std::string intensities;

    for(uint8_t module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) {
        intensities += (module > 0 ? "," : "") + std::to_string(panel.GetIntensity(module));
    }
    return intensities;
}
