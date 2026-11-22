// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * ScrollText
 *
 * Scrolls a text endlessly over the LED matrix without blocking loop(): the
 * library starts it over after its last char.
 *
 * Wiring, to the board's hardware SPI pins, which SPI needs and bit banging
 * takes as well: DIN -> MOSI, CS -> SS, CLK -> SCK; on the Uno, Nano and
 * Uno R4 11, 10, 13, on the Pico 19, 17, 18, on the ESP32 23, 5, 18.
 *
 * A chain of four modules, as max_matrix::MatrixOf<4> says below; one of eight
 * is max_matrix::MatrixOf<8>.
 */
#include <MaxMatrix.h>

max_matrix::DataInPin const kDataInPin{MOSI};
max_matrix::ChipSelectPin const kChipSelectPin{SS};
max_matrix::ClockPin const kClockPin{SCK};

max_matrix::MatrixOf<4> matrix{kDataInPin, kChipSelectPin, kClockPin};
/* the pace of the text: a column every 50 ms */
max_matrix::Interval shift_interval{50};

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
  /* the common 4-in-1 FC-16 boards; the README lists the other modules */
  matrix.SetModuleType(max_matrix::ModuleType::kFc16);
  matrix.SetIntensity(2);
  /* F() keeps the text in flash, where it takes no RAM; the spaces at its end
   * keep the end apart from the start */
  matrix.SetTextWithShift(F("Hello World!   "), true);  // over and over
}

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  /* a step each time the interval has passed, so loop() never waits */
  if (shift_interval.IsDue(millis())) {
    matrix.ShiftStep();
  }

  /* other work can be done here */
}
