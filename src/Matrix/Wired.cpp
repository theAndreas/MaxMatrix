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
#include "MaxMatrixStandardTypes.h"
#include "Utility/Byte.h"
#include "Utility/span.h"
#if (MAXMATRIX_USE_SPI == STD_ON)
#include "SPI.h"
#endif

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

// memset() takes no null pointer, not even for no byte, and an empty span,
// which Init() refuses, has one.
void Zero(span<Byte> buffer) {
  if (buffer.size() != 0u) {
    memset(buffer.data(), 0, buffer.size());
  }
}

}  // namespace

Wired::Wired(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
             ClockPin clock_pin, span<Byte> buffer)
    : chain_{data_in_pin, chip_select_pin, clock_pin}, matrix_buffer_{buffer} {
  Zero(matrix_buffer_);
}

#if (MAXMATRIX_USE_SPI == STD_ON)
Wired::Wired(SPIClass& spi, ChipSelectPin chip_select_pin, span<Byte> buffer)
    : chain_{spi, chip_select_pin}, matrix_buffer_{buffer} {
  Zero(matrix_buffer_);
}
#endif

// From the size of the buffer, as the count of its columns wraps beyond a
// byte.
bool Wired::HoldsAChain() const {
  size_t const length{matrix_buffer_.size()};
  bool const whole_modules{(length % kColumnNumberOfModule) == 0u};
  bool const in_range{(length >= BufferSize(1)) &&
                      (length <= BufferSize(kMaxNumberOfModules))};
  return whole_modules && in_range;
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
  Zero(matrix_buffer_);
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
