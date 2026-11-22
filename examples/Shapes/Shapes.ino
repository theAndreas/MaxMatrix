// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Shapes
 *
 * Draws a frame, lines, circles and a picture from flash, one after the other,
 * each for a second, without blocking loop(). Each is drawn in a batch, so the
 * matrix goes from one straight to the next.
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

uint8_t const kNumberOfShapes{4};

/* a heart, a column per byte, bit 0 the top row */
constexpr auto kHeart PROGMEM{
    max_matrix::make_array<uint8_t>(0x06, 0x0F, 0x1F, 0x3E, 0x1F, 0x0F, 0x06)};

max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{MOSI},
                               max_matrix::ChipSelectPin{SS},
                               max_matrix::ClockPin{SCK}};

uint8_t shape{0};
max_matrix::Interval shape_interval{1000};

void DrawShape() {
  X const right{static_cast<int16_t>(matrix.GetNumberOfColumns() - 1)};
  Y const bottom{max_matrix::kRowNumberOfModule - 1};
  max_matrix::Batch const batch{matrix};

  matrix.Clear();
  if (shape == 0) {
    /* a rectangle by two opposite corners, both part of it */
    matrix.DrawRect(Point{X{0}, Y{0}}, Point{right, bottom}, true);
    matrix.FillRect(Point{X{3}, Y{2}}, Point{X{6}, Y{5}}, true);
  } else if (shape == 1) {
    matrix.DrawLine(Point{X{0}, Y{0}}, Point{right, bottom}, true);
    matrix.DrawLine(Point{X{0}, bottom}, Point{right, Y{0}}, true);
  } else if (shape == 2) {
    /* the radius grows from 0 to the 3 that fills the height of the matrix */
    for (int16_t radius{0}; radius <= 3; radius++) {
      X const centre_x{static_cast<int16_t>(4 + (8 * radius))};
      matrix.DrawCircle(Point{centre_x, Y{3}}, radius, true);
    }
  } else {
    for (int16_t x{0}; x < matrix.GetNumberOfColumns(); x += 8) {
      matrix.DrawBitmap(Point{X{x}, Y{1}}, kHeart);
    }
  }
}

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
  DrawShape();
}

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  /* the next shape only every 1000 ms; the rest of loop() is free for other
   * work */
  if (!shape_interval.IsDue(millis())) {
    return;
  }

  shape = static_cast<uint8_t>((shape + 1) % kNumberOfShapes);
  DrawShape();
}
