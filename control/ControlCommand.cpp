// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The command lines of SerialControl, as the control program sends them,
 *        and the answer to `picture`.
 */

#include "ControlCommand.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "MaxMatrixConfig.h"

namespace {

/* a column of picture is two hex digits, and the first that hex writes as a
 * letter */
constexpr uint8_t kHexBase{16u};
constexpr uint8_t kFirstLetterDigit{10u};

// Whether `c` is a hex digit, in upper or lower case: compared rather than
// asked of isxdigit(), which depends on the locale (MISRA 24.5.1).
bool IsHexDigit(char c) {
  bool const digit{(c >= '0') && (c <= '9')};
  bool const upper_case{(c >= 'A') && (c <= 'F')};
  bool const lower_case{(c >= 'a') && (c <= 'f')};
  return digit || upper_case || lower_case;
}

// The value of a hex digit that IsHexDigit() has taken: the lower case
// letters lie above the upper case ones, which lie above the digits.
uint8_t ConvertHexDigit(char c) {
  uint8_t value{0};

  if (c >= 'a') {
    value = static_cast<uint8_t>((c - 'a') + kFirstLetterDigit);
  } else if (c >= 'A') {
    value = static_cast<uint8_t>((c - 'A') + kFirstLetterDigit);
  } else {
    value = static_cast<uint8_t>(c - '0');
  }
  return value;
}

}  // namespace

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
ControlPicture ControlCommand::DecodePicture(std::string const& line) {
  static std::string const kName = "picture=";
  ControlPicture picture{false, std::vector<uint8_t>{}};

  if (line.compare(0, kName.size(), kName) != 0) {
    return picture;
  }
  picture.answers_picture = true;
  if (line.size() !=
      (kName.size() + (2u * static_cast<size_t>(kControlNumberOfColumns)))) {
    return picture;
  }
  for (size_t i{kName.size()}; i < line.size(); i += 2) {
    char const high{line[i]};
    char const low{line[i + 1]};
    if (!IsHexDigit(high) || !IsHexDigit(low)) {
      picture.columns.clear();
      return picture;
    }
    picture.columns.push_back(static_cast<uint8_t>(
        (ConvertHexDigit(high) * kHexBase) + ConvertHexDigit(low)));
  }
  return picture;
}

bool ControlCommand::Fits(std::string const& line) {
  return (line.size() <= MAXMATRIX_COMMAND_LINE_LENGTH) &&
         (line.find_first_of("\r\n") == std::string::npos);
}

int32_t ControlCommand::GetBytesLeft(std::string const& line) {
  return static_cast<int32_t>(MAXMATRIX_COMMAND_LINE_LENGTH) -
         static_cast<int32_t>(line.size());
}
