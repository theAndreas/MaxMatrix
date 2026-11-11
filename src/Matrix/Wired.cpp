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

namespace max_matrix {
namespace internal {

Wired::Wired(byte data_in_pin, byte chip_select_pin, byte clock_pin,
             byte* buffer, byte number_of_modules)
    : chain_{data_in_pin, chip_select_pin, clock_pin, number_of_modules} {
  matrix_buffer_ = buffer;
  memset(matrix_buffer_, 0, GetNumberOfColumns());
  changed_columns_ = 0;
}

void Wired::StoreBit(byte column, byte bit, bool value) {
  byte column_value = matrix_buffer_[column];

  bitWrite(column_value, bit, value);
  if (column_value != matrix_buffer_[column]) {
    matrix_buffer_[column] = column_value;
    bitSet(changed_columns_, column % kColumnNumberOfModule);
  }
}

void Wired::Clear() {
  memset(matrix_buffer_, 0, GetNumberOfColumns());
  changed_columns_ = 0xFF;
}

// The values of a column lie a module's columns apart in the buffer, and go out
// from there rather than through an array of one per module, which would need
// its length at compile time.
void Wired::Send() {
  for (byte column = 0; column < kColumnNumberOfModule; column++) {
    if (bitRead(changed_columns_, column)) {
      chain_.WriteRegisterEach(max7219::kDigit0Address + column,
                               matrix_buffer_ + column, kColumnNumberOfModule);
    }
  }
  changed_columns_ = 0;
}

}  // namespace internal
}  // namespace max_matrix
