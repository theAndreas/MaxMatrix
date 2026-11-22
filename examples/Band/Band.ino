// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Band
 *
 * Runs a heartbeat over the matrix without end, without blocking loop(): every
 * step shifts the picture one column to the left and brings in the next column
 * of the beat at the right. The columns come from flash here, but they may as
 * well come from a sensor or a calculation.
 *
 * Wiring, to the board's hardware SPI pins, which SPI needs and bit banging
 * takes as well: DIN -> MOSI, CS -> SS, CLK -> SCK; on the Uno, Nano and
 * Uno R4 11, 10, 13, on the Pico 19, 17, 18, on the ESP32 23, 5, 18.
 */
#include <MaxMatrix.h>
#include <stdint.h>

/* one beat, a column per byte, bit 0 the top row: a line in row 5 with a spike
 * up and down */
constexpr auto kBeat PROGMEM{max_matrix::make_array<uint8_t>(
    0x20, 0x20, 0x20, 0x10, 0x08, 0x10, 0x20, 0x20, 0x60, 0x3F, 0xE0, 0x20,
    0x20, 0x20, 0x10, 0x08, 0x08, 0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20)};

max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{MOSI},
                               max_matrix::ChipSelectPin{SS},
                               max_matrix::ClockPin{SCK}};

uint8_t next_column{0};
max_matrix::Interval step_interval{60};

void setup() {
  // A pin the board lacks fails: the LED on the board says so, where it
  // has one, and loop() draws nothing, as the matrix is not set up.
  if (!matrix.Init().IsOk()) {
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    return;
  }
  matrix.SetModuleType(
      max_matrix::ModuleType::kFc16);  // see the README for other modules
}

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  /* a step only every 60 ms; the rest of loop() is free for other
   * work */
  if (!step_interval.IsDue(millis())) {
    return;
  }

  /* the column that falls out at the left comes back, for a sketch that passes
   * it on */
  matrix.ShiftLeft(max_matrix::Byte{pgm_read_byte(&kBeat[next_column])});
  next_column = static_cast<uint8_t>((next_column + 1) % kBeat.size());
}
