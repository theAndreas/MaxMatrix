// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The command lines of SerialControl, as the control program sends them,
 *        and the answer to `picture`.
 */

#include "ControlCommand.h"

#include <ctype.h>
#include <stdlib.h>

#include <array>
#include <string>
#include <vector>

#include "MaxMatrixConfig.h"

std::string ControlCommand::MakeText(std::string const& text, bool shift) {
  return (shift ? "textShift=" : "text=") + text;
}

std::string ControlCommand::MakeIntensity(uint8_t intensity) {
  return "intensity=" + std::to_string(intensity);
}

std::string ControlCommand::MakeSpeed(uint16_t milliseconds) {
  return "speed=" + std::to_string(milliseconds);
}

std::string ControlCommand::MakeSpace(uint8_t columns) {
  return "space=" + std::to_string(columns);
}

std::string ControlCommand::MakeDot(uint8_t column, uint8_t row, bool lit) {
  return "setDot=" + std::to_string(column) + "," + std::to_string(row) + "," +
         (lit ? "1" : "0");
}

std::string ControlCommand::MakeClear() { return "clear"; }

std::string ControlCommand::MakePicture() { return "picture"; }

// Two hex digits per column, upper case as SerialControl prints them; lower
// case is taken as well.
bool ControlCommand::DecodePicture(std::string const& line,
                                   std::vector<uint8_t>* columns) {
  static std::string const kName = "picture=";

  if (line.compare(0, kName.size(), kName) != 0) {
    return false;
  }
  columns->clear();
  if (line.size() !=
      kName.size() + (2U * static_cast<size_t>(kControlNumberOfColumns))) {
    return true;
  }
  for (size_t i = kName.size(); i < line.size(); i += 2) {
    std::array<char, 3> const digits{{line[i], line[i + 1], '\0'}};
    if ((isxdigit(static_cast<unsigned char>(digits[0])) == 0) ||
        (isxdigit(static_cast<unsigned char>(digits[1])) == 0)) {
      columns->clear();
      return true;
    }
    columns->push_back(
        static_cast<uint8_t>(strtoul(digits.data(), nullptr, 16)));
  }
  return true;
}

int32_t ControlCommand::GetBytesLeft(std::string const& line) {
  return static_cast<int32_t>(MAXMATRIX_COMMAND_LINE_LENGTH) -
         static_cast<int32_t>(line.size());
}

bool ControlCommand::Fits(std::string const& line) {
  return line.size() <= MAXMATRIX_COMMAND_LINE_LENGTH &&
         line.find_first_of("\r\n") == std::string::npos;
}
