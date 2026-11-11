// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Shutdown
 *
 * Turns the LEDs off without losing the picture, draws while they are off, and
 * lights every LED for a display test, in turn: each step says in its comment
 * what the matrix shows.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

max_matrix::MatrixOf<4> matrix(11, 10, 13);

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      max_matrix::ModuleType::kFc16);  // see the README for other modules
}

void loop() {
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
