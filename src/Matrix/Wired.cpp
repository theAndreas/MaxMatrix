// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as it is wired.
 *
 * The buffer mirrors what is on the LEDs, one byte per column and one bit per
 * row, columns of all modules in a row, module 0 first. Every write to the
 * hardware updates the buffer as well, because the MAX7219 cannot be read back.
 */

#include "Wired.h"

#include <stdint.h>
#include <string.h>

#include "Max7219/Chain.h"
#include "MaxMatrixConfig.h"
#include "Utility/Byte.h"
#include "Utility/span.h"

namespace max_matrix {
namespace internal {

namespace {

/* a bit for each column of a module, all set */
constexpr Byte kAllColumnsChanged{0xFF};

// The digit register that drives a column of every module: the MAX7219 numbers
// its digits from the register after the no-op.
max7219::Register GetDigitRegister(uint8_t column) {
  auto const digit0{static_cast<uint8_t>(max7219::Register::kDigit0)};
  return static_cast<max7219::Register>(digit0 + column);
}

}  // namespace

Wired::Wired(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
             ClockPin clock_pin, span<Byte> buffer)
    : chain_{data_in_pin, chip_select_pin, clock_pin}, matrix_buffer_{buffer} {
  memset(matrix_buffer_.data(), 0, matrix_buffer_.size());
}

void Wired::StoreBit(uint8_t column, uint8_t bit, bool value) {
  Byte const column_value{WriteBit(matrix_buffer_[column], bit, value)};

  if (column_value != matrix_buffer_[column]) {
    matrix_buffer_[column] = column_value;
    changed_columns_ =
        WriteBit(changed_columns_,
                 static_cast<uint8_t>(column % kColumnNumberOfModule), true);
  }
}

void Wired::Clear() {
  memset(matrix_buffer_.data(), 0, matrix_buffer_.size());
  changed_columns_ = kAllColumnsChanged;
}

// The values of a column go out straight from the buffer rather than through an
// array of one per module, which would need its length at compile time.
void Wired::Send() {
  for (uint8_t column{0}; column < kColumnNumberOfModule; column++) {
    if (ReadBit(changed_columns_, column)) {
      chain_.WriteRegisterEach(GetDigitRegister(column), matrix_buffer_,
                               column);
    }
  }
  changed_columns_ = Byte{};
}

}  // namespace internal
}  // namespace max_matrix
