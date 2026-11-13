// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Model of a chain of MAX7219, fed from the wire by the Arduino stand-in.
 *
 * Driven by the bytes the library shifts out and by the rising edge of chip
 * select, not by matrix_buffer_: the buffer only says what the library believes
 * is on the LEDs, and a wrong module, register or frame length leaves it right
 * and the display wrong. Only the logic is modelled - no timing, nothing
 * electrical, and no multiplexing: a digit within the scan limit shows its
 * register for as long as the chip is not shut down. */
#ifndef MAXMATRIX_TEST_MODEL_MAX7219_H_
#define MAXMATRIX_TEST_MODEL_MAX7219_H_

#include <stdint.h>

#include <array>
#include <vector>

#include "MaxMatrixAttributes.h"

class Max7219 {
 public:
  static uint8_t const kNumberOfDigits{8};

  std::array<uint8_t, kNumberOfDigits> digit{};
  uint8_t decode_mode{};
  uint8_t intensity{};
  uint8_t scan_limit{};
  uint8_t shutdown{};
  uint8_t display_test{};

  Max7219() { Reset(); }
  void Reset();
  void Latch(uint8_t address, uint8_t data);
  MAXMATRIX_NODISCARD uint8_t Segments(uint8_t digit_index) const;
};

/* How the LEDs of a module are wired to digits and segments, as the viewer sees
 * them, in the three properties MD_MAX72XX names its hardware types by,
 * DR0CR0RR0_HW to DR1CR1RR1_HW, and with the same meaning, so that a module
 * MD_MAX72XX has tested is the same module here. As there, a module's column is
 * counted from the right and its row from the top: digits_as_rows      a digit
 * drives a row of LEDs and a segment bit a column; otherwise a digit drives a
 * column reversed_columns   the segment bit index is reversed, bit 7 - x for x
 *   reversed_rows      the digit index is reversed, digit 7 - x for x */
struct Max7219Wiring {
  char const* name;
  bool digits_as_rows;
  bool reversed_columns;
  bool reversed_rows;
};

/* the four MD_MAX72XX has names for */
extern Max7219Wiring const kMaX7219WiringFC16;
extern Max7219Wiring const kMaX7219WiringGeneric;
extern Max7219Wiring const kMaX7219WiringParola;
extern Max7219Wiring const kMaX7219WiringIcstation;

/* all eight, DR0CR0RR0 to DR1CR1RR1 */
extern std::array<Max7219Wiring, 8> const kMax7219Wirings;

class Max7219Chain {
 public:
  /* Chips[0] is the chip at DIN, next to the Arduino; a byte reaches the far
   * end last */
  std::vector<Max7219> chips;

  void Reset(uint8_t number_of_chips);
  void Shift(uint8_t value);
  void ShiftBit(bool value);
  void Latch();
  MAXMATRIX_NODISCARD bool IsLit(Max7219Wiring const& wiring, uint16_t x,
                                 uint8_t y, bool upside_down = false) const;
  MAXMATRIX_NODISCARD Max7219 const& ChipAt(uint16_t x,
                                            bool upside_down = false) const;

 private:
  /* two bytes per chip, shift_register_[2 * Chip] its data byte and
   * shift_register_[2 * Chip + 1] its address byte */
  std::vector<uint8_t> shift_register_;
};

#endif  // MAXMATRIX_TEST_MODEL_MAX7219_H_
