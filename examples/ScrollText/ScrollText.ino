// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * ScrollText
 *
 * Scrolls a text endlessly over the LED matrix without blocking loop(): the
 * library starts it over after its last char.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 *
 * A chain of four modules, as max_matrix::MatrixOf<4> says below; one of eight
 * is max_matrix::MatrixOf<8>.
 */
#include <MaxMatrix.h>

byte const kDataInPin = 11;
byte const kChipSelectPin = 10;
byte const kClockPin = 13;
uint16_t const kShiftIntervalMs = 50;

max_matrix::MatrixOf<4> matrix(kDataInPin, kChipSelectPin, kClockPin);

/* Text must stay valid while it is shifted, so don't use a local variable; the
 * spaces at its end keep the end apart from the start */
char const kText[] = "Hello World!   ";

void setup() {
  matrix.Init();
  /* the common 4-in-1 FC-16 boards; the README lists the other modules */
  matrix.SetModuleType(max_matrix::ModuleType::kFc16);
  matrix.SetIntensity(2);
  matrix.SetShiftInterval(kShiftIntervalMs);
  matrix.SetTextWithShift(kText, true);  // over and over
}

void loop() {
  /* takes a step only every kShiftIntervalMs, so it can be called on every
   * loop() */
  matrix.ShiftTask();

  /* other work can be done here */
}
