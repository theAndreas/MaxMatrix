// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* What the tests share, defined once: see MaxMatrixTestSupport.h. */
#include "MaxMatrixTestSupport.h"

using max_matrix::CommandState;
using max_matrix::Error;
using max_matrix::Matrix;
using max_matrix::ModuleType;
using max_matrix::Result;

#include <string>
#include <vector>

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

std::vector<byte> Bytes(byte const* data, size_t size) {
  // T-20: the library's buffer is a pointer and a length
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return std::vector<byte>(data, data + size);
}

Max7219 const& ChipOfModule(byte module) {
  return mock::chain.chips[mock::chain.chips.size() - 1 - module];
}

std::vector<byte> Display() {
  std::vector<byte> columns;
  for (byte module{0}; module < mock::chain.chips.size(); module++) {
    for (byte column{0}; column < max_matrix::kColumnNumberOfModule; column++) {
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
  for (byte x{0}; x < matrix.GetNumberOfColumns(); x++) {
    for (byte y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      matrix.SetDot(x, y, Pattern(x, y));
    }
  }
}
