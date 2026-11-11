// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The lines between the control program and one SerialControl, on a
 *        board or in the simulator.
 */

#include "ControlLink.h"

#include <string>
#include <utility>
#include <vector>

#include "ControlCommand.h"

ControlLink::ControlLink(ControlPort& port) : port_{port} {
  ready_ = false;
  picture_asked_ = false;
  picture_stale_ = false;
  picture_timed_ = false;
  picture_asked_at_ = 0;
}

void ControlLink::Begin(bool wait_for_ready) {
  ready_ = !wait_for_ready;
  received_.clear();
  picture_.clear();
  picture_asked_ = false;
  picture_stale_ = false;
  picture_timed_ = false;
}

bool ControlLink::IsReady() const { return ready_ && port_.IsOpen(); }

bool ControlLink::Send(std::string const& line) {
  if (!IsReady()) return false;
  picture_stale_ = picture_asked_;
  return port_.Write(line + "\n");
}

// The time is compared as a difference, so that a clock that runs past 2^32 ms
// keeps working.
void ControlLink::AskForPicture(uint32_t now) {
  uint32_t const waited = now - picture_asked_at_;

  if (!IsReady()) return;
  if (picture_timed_ &&
      waited < (picture_asked_ ? CONTROL_LINK_PICTURE_TIMEOUT_MS
                               : CONTROL_LINK_PICTURE_INTERVAL_MS))
    return;
  if (!port_.Write(ControlCommand::MakePicture() + "\n")) return;
  picture_asked_ = true;
  picture_stale_ = false;
  picture_timed_ = true;
  picture_asked_at_ = now;
}

bool ControlLink::Task(std::vector<std::string>* lines) {
  size_t end;

  if (!port_.IsOpen()) return false;
  bool const open = port_.Read(&received_);

  while ((end = received_.find('\n')) != std::string::npos ||
         received_.size() >= CONTROL_LINK_MAX_LINE_LENGTH) {
    size_t const length =
        end != std::string::npos ? end : CONTROL_LINK_MAX_LINE_LENGTH;
    std::string line = received_.substr(0, length);
    received_.erase(0, end != std::string::npos ? end + 1 : length);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    Take(std::move(line), lines);
  }
  return open;
}

/*
 * Takes one line from SerialControl. A board that says ready has started again,
 * by its reset button say, and answers no picture asked for before; it is asked
 * again at the next interval. An answer to picture is never passed on, even one
 * that cannot be decoded, as it would fill the log twenty times a second.
 */
void ControlLink::Take(std::string line, std::vector<std::string>* lines) {
  std::vector<uint8_t> picture;

  if (line == CONTROL_LINK_READY) {
    ready_ = true;
    picture_asked_ = false;
  }
  if (!ControlCommand::DecodePicture(line, &picture)) {
    lines->push_back(std::move(line));
    return;
  }
  if (picture_asked_ && !picture_stale_) picture_ = std::move(picture);
  picture_asked_ = false;
}
