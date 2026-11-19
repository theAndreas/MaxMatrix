// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Max7219.h"

#include <stddef.h>
#include <stdint.h>

#include <array>

#include "Utility/Bit.h"

namespace {

using Bits = max_matrix::Bit<uint8_t>;

/* the fields of the registers, as the datasheet gives them */
constexpr uint8_t kAddressBits{0x0Fu};
constexpr uint8_t kIntensityBits{0x0Fu};
constexpr uint8_t kScanLimitBits{0x07u};
constexpr uint8_t kSwitchBit{0x01u};
constexpr uint8_t kDecimalPoint{0x80u};
constexpr uint8_t kCodeBDigit{0x0Fu};
/* the first bit on the wire, and the one that falls out of a register */
constexpr uint8_t kTopBit{0x80u};
constexpr uint8_t kTopPosition{7u};

}  // namespace

std::array<Max7219Wiring, 8> const kMax7219Wirings{{
    {"DR0CR0RR0", false, false, false},
    {"DR0CR0RR1", false, false, true},
    {"DR0CR1RR0", false, true, false},
    {"DR0CR1RR1", false, true, true},
    {"DR1CR0RR0", true, false, false},
    {"DR1CR0RR1", true, false, true},
    {"DR1CR1RR0", true, true, false},
    {"DR1CR1RR1", true, true, true},
}};

Max7219Wiring const kMax7219WiringGeneric{kMax7219Wirings[2]};
Max7219Wiring const kMax7219WiringFc16{kMax7219Wirings[4]};
Max7219Wiring const kMax7219WiringParola{kMax7219Wirings[6]};
Max7219Wiring const kMax7219WiringIcstation{kMax7219Wirings[7]};

/* the datasheet's power-up state: control registers reset, which means shut
 * down, one digit scanned, no decode and minimum intensity; the digit registers
 * are undefined there and zero here */
void Max7219::Reset() {
  digit.fill(0);
  decode_mode = 0;
  intensity = 0;
  scan_limit = 0;
  shutdown = 0;
  display_test = 0;
}

/* D15 to D12 are don't care, so only the low nibble of the address byte selects
 * the register; 0x0D and 0x0E are no register at all and change nothing, like
 * the no-op */
void Max7219::Latch(uint8_t address, uint8_t data) {
  address = Bits::ReadBitGroup(address, kAddressBits);
  if ((address >= 0x01) && (address <= 0x08)) {
    digit[address - 1] = data;
  } else if (address == 0x09) {
    decode_mode = data;
  } else if (address == 0x0A) {
    intensity = Bits::ReadBitGroup(data, kIntensityBits);
  } else if (address == 0x0B) {
    scan_limit = Bits::ReadBitGroup(data, kScanLimitBits);
  } else if (address == 0x0C) {
    shutdown = Bits::ReadBitGroup(data, kSwitchBit);
  } else if (address == 0x0F) {
    display_test = Bits::ReadBitGroup(data, kSwitchBit);
  } else {
    // the no-op, 0x00, and the addresses the chip has no register at
  }
}

/* lit segments of a digit, laid out as in no-decode mode: bit 7 is DP, bit 6
 * segment A down to bit 0 segment G. On a matrix module a digit is one column
 * (or row) and a segment one LED. Display test lights everything, even in
 * shutdown. */
uint8_t Max7219::Segments(uint8_t digit_index) const {
  /* Code B font, segments A to G in bits 6 to 0: 0-9, '-', 'E', 'H', 'L', 'P',
   * blank */
  static std::array<uint8_t, 16> const kCodeB{
      {0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70, 0x7F, 0x7B, 0x01, 0x4F,
       0x37, 0x0E, 0x67, 0x00}};

  if (display_test != 0u) {
    return 0xFF;
  }
  if ((shutdown == 0u) || (digit_index > scan_limit)) {
    return 0x00;
  }
  if (Bits::ReadBit(decode_mode, digit_index)) {
    uint8_t const point{Bits::ReadBitGroup(digit[digit_index], kDecimalPoint)};
    uint8_t const code{Bits::ReadBitGroup(digit[digit_index], kCodeBDigit)};
    return Bits::Combine(point, kCodeB[code]);
  }
  return digit[digit_index];
}

void Max7219Chain::Reset(uint8_t number_of_chips) {
  chips.assign(number_of_chips, Max7219());
  shift_register_.assign(size_t{2} * number_of_chips, 0);
}

/* a byte as the wire carries it, most significant bit first */
void Max7219Chain::Shift(uint8_t value) {
  uint8_t mask{kTopBit};
  while (mask != 0u) {
    ShiftBit(Bits::ReadBitGroup(value, mask) != 0u);
    mask = Bits::ShiftDown(mask, 1u);
  }
}

/* The MAX7219 shifts one bit on every rising clock edge, whatever chip select
 * does - unlike the MAX7221, which needs it low. The bit that falls out of one
 * chip's register goes into the next, so a frame of one pair per chip leaves
 * the first pair sent in the chip at the far end. Bit by bit rather than byte
 * by byte, so that a frame that is not a whole number of bytes, which bit
 * banging can send, latches what the hardware would. */
void Max7219Chain::ShiftBit(bool bit) {
  if (shift_register_.empty()) {
    return;
  }
  for (size_t i{shift_register_.size() - 1}; i > 0; i--) {
    uint8_t const shifted{Bits::ShiftUp(shift_register_[i], 1u)};
    bool const carry{Bits::ReadBit(shift_register_[i - 1], kTopPosition)};
    shift_register_[i] = Bits::WriteBit(shifted, 0, carry);
  }
  uint8_t const shifted{Bits::ShiftUp(shift_register_[0], 1u)};
  shift_register_[0] = Bits::WriteBit(shifted, 0, bit);
}

/* every chip latches whatever its register holds on the rising edge of chip
 * select - after a frame that was too short, that is part of the frame before
 */
void Max7219Chain::Latch() {
  for (size_t chip{0}; chip < chips.size(); chip++) {
    chips[chip].Latch(shift_register_[(2 * chip) + 1],
                      shift_register_[2 * chip]);
  }
}

/* whether the LED at X, Y of the panel is lit, X from the left across all
 * modules and Y from the top. The modules are chained from right to left, as
 * MD_MAX72XX has them, so Chips[0] at DIN is the rightmost. A board mounted
 * upside down shows every LED at the opposite place: its chain starts on the
 * left, each module turned around. */
// FN-16: x before y, as everywhere; a swap reads another LED, which
// MaxMatrixModuleTest reports for every wiring
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
bool Max7219Chain::IsLit(Max7219Wiring const& wiring, uint16_t x, uint8_t y,
                         bool upside_down) const {
  if (upside_down) {
    x = static_cast<uint16_t>((chips.size() * Max7219::kNumberOfDigits) - 1 -
                              x);
    y = Max7219::kNumberOfDigits - 1 - y;
  }

  Max7219 const& chip{ChipAt(x)};
  uint8_t const column{static_cast<uint8_t>(Max7219::kNumberOfDigits - 1 -
                                            (x % Max7219::kNumberOfDigits))};
  uint8_t digit{wiring.digits_as_rows ? y : column};
  uint8_t segment{wiring.digits_as_rows ? column : y};

  if (wiring.reversed_rows) {
    digit = Max7219::kNumberOfDigits - 1 - digit;
  }
  if (wiring.reversed_columns) {
    segment = Max7219::kNumberOfDigits - 1 - segment;
  }
  return Bits::ReadBit(chip.Segments(digit), segment);
}

/* the chip that drives the LEDs in column X of the panel, by the same placement
 * as IsLit() */
Max7219 const& Max7219Chain::ChipAt(uint16_t x, bool upside_down) const {
  if (upside_down) {
    x = static_cast<uint16_t>((chips.size() * Max7219::kNumberOfDigits) - 1 -
                              x);
  }
  size_t const module{static_cast<size_t>(x / Max7219::kNumberOfDigits)};
  return chips[chips.size() - 1 - module];
}
