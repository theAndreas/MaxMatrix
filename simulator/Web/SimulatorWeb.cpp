// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in a browser.
 *
 * The page is sent JSON; it sends back "wiring NAME", "upside-down 0|1",
 * "serial LINE", which reaches the sketch's Serial as a line, and "hello" when
 * it opens, which asks for the panel.
 */

#include "SimulatorWeb.h"

#include <stdio.h>

#include <sstream>
#include <string>
#include <vector>

namespace {

/* commands of the page */
constexpr char kCommandWiring[] = "wiring ";
constexpr char kCommandUpright[] = "upside-down 0";
constexpr char kCommandUpsideDown[] = "upside-down 1";
constexpr char kCommandSerial[] = "serial ";

/* JSON, RFC 8259: the printable ASCII range; the rest is written as \u00XX */
constexpr uint8_t kJsonFirstPlain{0x20u};
constexpr uint8_t kJsonLastPlain{0x7Eu};

}  // namespace

SimulatorWeb::SimulatorWeb() { logged_frames_ = 0; }

bool SimulatorWeb::Begin(uint16_t port, char const* page_path) {
  return host_.Begin(port, page_path);
}

void SimulatorWeb::Task(SimulatorPanel& panel, std::string const& printed) {
  std::vector<std::string> messages;

  if (mock::frames != logged_frames_) LogFrame();
  host_.Task(&messages);
  for (std::string const& message : messages) Command(panel, message);
  serial_output_ += printed;
  if (!printed.empty()) panel.RequestDraw();
}

void SimulatorWeb::Send(SimulatorPanel const& panel) {
  host_.Broadcast(GetMessage(panel));
  frame_log_.clear();
  serial_output_.clear();
}

/*
 * Every byte outside printable ASCII is escaped: a sketch may print anything,
 * latin-1 included, and a WebSocket text frame that is not valid UTF-8 makes
 * the browser close the connection; read as latin-1, every byte is a valid
 * char.
 */
std::string SimulatorWeb::ConvertToJsonString(std::string const& text) {
  std::string json;
  char escaped[8];

  for (char const c : text) {
    uint8_t const code = static_cast<uint8_t>(c);
    if (c == '"' || c == '\\') {
      json += '\\';
      json += c;
    } else if (code >= kJsonFirstPlain && code <= kJsonLastPlain) {
      json += c;
    } else {
      snprintf(escaped, sizeof(escaped), "\\u%04X",
               static_cast<uint16_t>(code));
      json += escaped;
    }
  }
  return json;
}

// Logs the last frame as address/value pairs, one pair per module, and keeps no
// more than kSimulatorWebFrameLogLength frames.
void SimulatorWeb::LogFrame() {
  std::string line = "#" + std::to_string(mock::frames) + ":";
  char pair[8];

  for (size_t i = 0; i + 1 < mock::last_frame.size(); i += 2) {
    snprintf(pair, sizeof(pair), "%02X %02X", mock::last_frame[i],
             mock::last_frame[i + 1]);
    line += i == 0 ? " " : " | ";
    line += pair;
  }
  frame_log_.push_back(line);
  if (frame_log_.size() > kSimulatorWebFrameLogLength) {
    frame_log_.erase(frame_log_.begin());
  }
  logged_frames_ = mock::frames;
}

/*
 * Carries out `message`, a command of the page, on `panel`: changes the wiring
 * or the mounting, or hands a line to the sketch's Serial, as a control program
 * would send it to a board. Every message asks for the panel, also one that
 * changes nothing, which is what "hello" is for; an unknown wiring keeps the
 * one the panel has.
 */
void SimulatorWeb::Command(SimulatorPanel& panel, std::string const& message) {
  std::string const wiring_command = kCommandWiring;
  std::string const serial_command = kCommandSerial;

  if (message.compare(0, wiring_command.size(), wiring_command) == 0) {
    panel.SetWiring(message.substr(wiring_command.size()));
  } else if (message.compare(0, serial_command.size(), serial_command) == 0) {
    mock::serial_input += message.substr(serial_command.size()) + "\n";
  } else if (message == kCommandUpsideDown) {
    panel.SetUpsideDown(true);
  } else if (message == kCommandUpright) {
    panel.SetUpsideDown(false);
  }
  panel.RequestDraw();
}

/*
 * Returns the message for the page: `panel` as JSON, with the wiring and the
 * mounting, the size, the dots row after row as '1' and '0', the brightness of
 * each module from the left, the frame count, the transfer errors, the frames
 * since the last message and what the sketch printed since then.
 */
std::string SimulatorWeb::GetMessage(SimulatorPanel const& panel) const {
  std::ostringstream json;

  json << "{\"wiring\":\"" << panel.GetWiringName()
       << "\",\"upsideDown\":" << (panel.IsUpsideDown() ? "true" : "false")
       << ",\"columns\":" << static_cast<uint16_t>(panel.GetNumberOfColumns())
       << ",\"rows\":" << static_cast<uint16_t>(max_matrix::kRowNumberOfModule)
       << ",\"dots\":\"" << panel.GetText("1", "0", "") << "\""
       << ",\"intensity\":[" << GetIntensities(panel) << "]"
       << ",\"frames\":" << mock::frames << ",\"errors\":" << mock::errors
       << ",\"log\":[" << GetLog() << "]"
       << ",\"serial\":\"" << ConvertToJsonString(serial_output_) << "\"}";
  return json.str();
}

// Returns the logged frames for the message, as JSON strings separated by
// commas. Every frame is quoted as it is: the frames hold nothing a JSON string
// would have to escape.
std::string SimulatorWeb::GetLog() const {
  std::string log;

  for (size_t i = 0; i < frame_log_.size(); i++) {
    log += (i > 0 ? ",\"" : "\"") + frame_log_[i] + "\"";
  }
  return log;
}

// Returns the brightness of every module of `panel` for the message, separated
// by commas. The modules count as the viewer sees them, from the left, whatever
// the mounting.
std::string SimulatorWeb::GetIntensities(SimulatorPanel const& panel) {
  std::string intensities;

  for (uint8_t module = 0; module < panel.GetNumberOfModules(); module++) {
    intensities +=
        (module > 0 ? "," : "") + std::to_string(panel.GetIntensity(module));
  }
  return intensities;
}
