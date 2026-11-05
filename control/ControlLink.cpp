// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The lines between the control program and one SerialControl, on a board or in the
 *        simulator.
 */

#include "ControlLink.h"

ControlLink::ControlLink(ControlPort& port) : port_(port)
{
    ready_ = false;
}

void ControlLink::Begin(bool wait_for_ready)
{
    ready_ = !wait_for_ready;
    received_.clear();
}

bool ControlLink::IsReady() const
{
    return ready_ && port_.IsOpen();
}

bool ControlLink::Send(std::string const& line)
{
    if(!IsReady()) return false;
    return port_.Write(line + "\n");
}

bool ControlLink::Task(std::vector<std::string>* lines)
{
    size_t end;

    if(!port_.IsOpen()) return false;
    bool const open = port_.Read(&received_);

    while((end = received_.find('\n')) != std::string::npos || received_.size() >= CONTROL_LINK_MAX_LINE_LENGTH) {
        size_t const length = end != std::string::npos ? end : CONTROL_LINK_MAX_LINE_LENGTH;
        std::string line = received_.substr(0, length);
        received_.erase(0, end != std::string::npos ? end + 1 : length);
        if(!line.empty() && line.back() == '\r') line.pop_back();
        if(line == CONTROL_LINK_READY) ready_ = true;
        lines->push_back(line);
    }
    return open;
}
