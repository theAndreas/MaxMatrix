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

max_matrix::DataInPin const kDataInPin{11};
max_matrix::ChipSelectPin const kChipSelectPin{10};
max_matrix::ClockPin const kClockPin{13};

max_matrix::MatrixOf<4> matrix{kDataInPin, kChipSelectPin, kClockPin};
/* the pace of the text: a column every 50 ms */
max_matrix::Interval shift_interval{50};

/* Text must stay valid while it is shifted, so don't use a local variable; the
 * spaces at its end keep the end apart from the start */
char const kText[] = "Hello World!   ";

void setup() {
  // A pin the board lacks fails: the LED on the board says so, and loop()
  // draws nothing, as the matrix is not set up.
  if (!matrix.Init().IsOk()) {
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    return;
  }
  /* the common 4-in-1 FC-16 boards; the README lists the other modules */
  matrix.SetModuleType(max_matrix::ModuleType::kFc16);
  matrix.SetIntensity(2);
  matrix.SetTextWithShift(kText, true);  // over and over
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
