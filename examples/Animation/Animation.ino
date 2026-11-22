// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Animation
 *
 * Bounces a ball off the edges of the matrix without blocking loop(): each step
 * clears the ball where it was and draws it where it is, and both go to the
 * matrix as one picture.
 *
 * Wiring, to the board's hardware SPI pins, which SPI needs and bit banging
 * takes as well: DIN -> MOSI, CS -> SS, CLK -> SCK; on the Uno, Nano and
 * Uno R4 11, 10, 13, on the Pico 19, 17, 18, on the ESP32 23, 5, 18.
 */
#include <MaxMatrix.h>
#include <stdint.h>

using max_matrix::Point;
using max_matrix::X;
using max_matrix::Y;

max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{MOSI},
                               max_matrix::ChipSelectPin{SS},
                               max_matrix::ClockPin{SCK}};

int16_t ball_x{0};
int16_t ball_y{0};
int8_t step_x{1};
int8_t step_y{1};
max_matrix::Interval step_interval{40};

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
  matrix.SetDot(Point{X{ball_x}, Y{ball_y}}, true);
}

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  /* a step only every 40 ms; the rest of loop() is free for other
   * work */
  if (!step_interval.IsDue(millis())) {
    return;
  }

  /* without the batch the matrix would show the frame without a ball in
   * between, as a flicker */
  max_matrix::Batch const batch{matrix};
  matrix.SetDot(Point{X{ball_x}, Y{ball_y}}, false);
  /* turn round at an edge rather than leave the matrix */
  if (((ball_x + step_x) < 0) ||
      ((ball_x + step_x) >= matrix.GetNumberOfColumns())) {
    step_x = static_cast<int8_t>(-step_x);
  }
  if (((ball_y + step_y) < 0) ||
      ((ball_y + step_y) >= max_matrix::kRowNumberOfModule)) {
    step_y = static_cast<int8_t>(-step_y);
  }
  ball_x += step_x;
  ball_y += step_y;
  matrix.SetDot(Point{X{ball_x}, Y{ball_y}}, true);
}
