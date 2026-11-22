// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Shutdown
 *
 * Turns the LEDs off without losing the picture, draws while they are off, and
 * lights every LED for a display test, in turn: each step says in its comment
 * what the matrix shows.
 *
 * Wiring, to the board's hardware SPI pins, which SPI needs and bit banging
 * takes as well: DIN -> MOSI, CS -> SS, CLK -> SCK; on the Uno, Nano and
 * Uno R4 11, 10, 13, on the Pico 19, 17, 18, on the ESP32 23, 5, 18.
 */
#include <MaxMatrix.h>

max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{MOSI},
                               max_matrix::ChipSelectPin{SS},
                               max_matrix::ClockPin{SCK}};

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
  matrix.Clear();
  matrix.SetText("Hi");
  delay(2000);  // "Hi"

  matrix.SetShutdown(true);
  delay(1000);  // every LED off

  /* drawing goes on while the LEDs are off; it shows when they come on again */
  matrix.Clear();
  matrix.SetText("Zz");
  delay(1000);  // still off

  matrix.SetShutdown(false);
  delay(2000);  // "Zz", not "Hi"

  matrix.SetDisplayTest(true);
  delay(1000);  // every LED at full brightness

  matrix.SetDisplayTest(false);
  delay(2000);  // "Zz" again, at the intensity it had
}
