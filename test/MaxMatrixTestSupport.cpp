// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* What the tests share, defined once: see MaxMatrixTestSupport.h. */
#include "MaxMatrixTestSupport.h"

#include <stddef.h>
#include <stdint.h>

#include <ostream>
#include <string>
#include <vector>

#include "Arduino.h"
#include "Max7219.h"

using max_matrix::CommandState;
using max_matrix::Error;
using max_matrix::Matrix;
using max_matrix::ModuleType;
using max_matrix::Result;

namespace max_matrix {

void PrintTo(Error error, std::ostream* out) {
  if (static_cast<uint8_t>(error) < kNumberOfErrors) {
    *out << kErrorNames[static_cast<uint8_t>(error)];
  } else {
    *out << "error " << static_cast<uint16_t>(error);
  }
}

/* what Feed() returns, compared with a state as with an error: a line is done
 * only if it did not fail as well */
bool operator==(Result<CommandState> const& result, CommandState state) {
  return result.IsOk() && result.GetValue() == state;
}

void PrintTo(Result<CommandState> const& result, std::ostream* out) {
  if (!result.IsOk()) {
    PrintTo(result.GetError(), out);
  } else if (result.GetValue() == CommandState::kDone) {
    *out << "CommandState::kDone";
  } else if (result.GetValue() == CommandState::kAnswered) {
    *out << "CommandState::kAnswered";
  } else {
    *out << "CommandState::kPending";
  }
}

}  // namespace max_matrix

std::string Text(__FlashStringHelper const* text) {
  return reinterpret_cast<char const*>(text);
}

std::vector<Report> reports;

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
void max_matrix::ReportError(__FlashStringHelper const* api, Error error) {
  reports.emplace_back(Text(api), error);
}
#endif

std::vector<uint8_t> Bytes(max_matrix::span<uint8_t const> data) {
  std::vector<uint8_t> bytes;
  for (size_t index{0}; index < data.size(); index++) {
    bytes.push_back(data[index]);
  }
  return bytes;
}

std::vector<uint8_t> Bytes(max_matrix::span<max_matrix::Byte const> data) {
  std::vector<uint8_t> bytes;
  for (size_t index{0}; index < data.size(); index++) {
    bytes.push_back(data[index].ToInteger());
  }
  return bytes;
}

Max7219 const& ChipOfModule(size_t module) {
  return mock::chain.chips[mock::chain.chips.size() - 1 - module];
}

std::vector<uint8_t> Display() {
  std::vector<uint8_t> columns;
  for (size_t module{0}; module < mock::chain.chips.size(); module++) {
    for (uint8_t column{0}; column < max_matrix::kColumnNumberOfModule;
         column++) {
      columns.push_back(ChipOfModule(module).Segments(column));
    }
  }
  return columns;
}

std::string ModuleTypeName(ModuleType module_type) {
  return kMax7219Wirings[static_cast<uint8_t>(module_type)].name;
}

bool Pattern(int16_t x, int16_t y) {
  return ((x * x) + (3 * y) + (x * y)) % 3 == 0;
}

std::vector<std::string> ShownPicture(Matrix& matrix) {
  return Picture(
      [&matrix](int16_t x, int16_t y) {
        return matrix.GetDot(x, y).GetValue();
      },
      matrix.GetNumberOfColumns());
}

void DrawPattern(Matrix& matrix) {
  for (uint8_t x{0}; x < matrix.GetNumberOfColumns(); x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      matrix.SetDot(x, y, Pattern(x, y));
    }
  }
}
