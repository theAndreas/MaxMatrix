// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The lines between the control program and one SerialControl, on a
 *        board or in the simulator.
 */

#include "ControlLink.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>
#include <vector>

#include "ControlCommand.h"
#include "ControlPort.h"

ControlLink::ControlLink(ControlPort& port) : port_{port} {}

void ControlLink::Begin(bool wait_for_ready) {
  ready_ = !wait_for_ready;
  received_.clear();
  picture_.clear();
  picture_asked_ = false;
  picture_stale_ = false;
  picture_timed_ = false;
}

bool ControlLink::IsReady() const { return ready_ && port_.get().IsOpen(); }

bool ControlLink::Send(std::string const& line) {
  if (!IsReady()) {
    return false;
  }
  picture_stale_ = picture_asked_;
  return port_.get().Write(line + "\n");
}

// The time is compared as a difference, so that a clock that runs past 2^32 ms
// keeps working.
void ControlLink::AskForPicture(uint32_t now) {
  uint32_t const waited{now - picture_asked_at_};

  if (!IsReady()) {
    return;
  }
  if (picture_timed_ &&
      (waited < (picture_asked_ ? kControlLinkPictureTimeoutMs
                                : kControlLinkPictureIntervalMs))) {
    return;
  }
  if (!port_.get().Write(ControlCommand::MakePicture() + "\n")) {
    return;
  }
  picture_asked_ = true;
  picture_stale_ = false;
  picture_timed_ = true;
  picture_asked_at_ = now;
}

ControlLinkLines ControlLink::Task() {
  size_t end{0};
  ControlLinkLines read{false, std::vector<std::string>{}};

  if (!port_.get().IsOpen()) {
    return read;
  }
  read.open = port_.get().Read(received_);

  while (((end = received_.find('\n')) != std::string::npos) ||
         (received_.size() >= kControlLinkMaxLineLength)) {
    size_t const length{(end != std::string::npos) ? end
                                                   : kControlLinkMaxLineLength};
    std::string line = received_.substr(0, length);
    received_.erase(0, (end != std::string::npos) ? (end + 1) : length);
    if (!line.empty() && (line.back() == '\r')) {
      line.pop_back();
    }
    if (Take(line)) {
      read.lines.push_back(std::move(line));
    }
  }
  return read;
}

/*
 * Takes one line from SerialControl, and returns whether it is one to pass on.
 * A board that says ready has started again, by its reset button say, and
 * answers no picture asked for before; it is asked again at the next interval.
 * An answer to picture is never passed on, even one that cannot be decoded, as
 * it would fill the log twenty times a second.
 */
bool ControlLink::Take(std::string const& line) {
  if (line == kControlLinkReady) {
    ready_ = true;
    picture_asked_ = false;
  }
  ControlPicture picture{ControlCommand::DecodePicture(line)};
  if (!picture.answers_picture) {
    return true;
  }
  if (picture_asked_ && !picture_stale_) {
    picture_ = std::move(picture.columns);
  }
  picture_asked_ = false;
  return false;
}
