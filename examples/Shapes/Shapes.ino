// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Shapes
 *
 * Draws a frame, lines, circles and a picture from flash, one after the other,
 * each for a second, without blocking loop(). Each is drawn in a batch, so the
 * matrix goes from one straight to the next.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

uint32_t const kShowMs = 1000;
byte const kNumberOfShapes = 4;

/* a heart, a column per byte, bit 0 the top row */
byte const kHeart[] PROGMEM = {0x06, 0x0F, 0x1F, 0x3E, 0x1F, 0x0F, 0x06};

max_matrix::MatrixOf<4> matrix(11, 10, 13);

byte shape = 0;
uint32_t last_shape = 0;

void DrawShape() {
  int16_t const right = matrix.GetNumberOfColumns() - 1;
  max_matrix::Batch batch(matrix);

  matrix.Clear();
  if (shape == 0) {
    matrix.DrawRect(0, 0, matrix.GetNumberOfColumns(),
                    max_matrix::kRowNumberOfModule, true);
    matrix.FillRect(3, 2, 4, 4, true);
  } else if (shape == 1) {
    matrix.DrawLine(0, 0, right, 7, true);
    matrix.DrawLine(0, 7, right, 0, true);
  } else if (shape == 2) {
    /* the radius grows from 0 to the 3 that fills the height of the matrix */
    for (int16_t radius = 0; radius <= 3; radius++) {
      matrix.DrawCircle(static_cast<int16_t>(4 + 8 * radius), 3, radius, true);
    }
  } else {
    for (int16_t x = 0; x < matrix.GetNumberOfColumns(); x += 8) {
      matrix.DrawBitmap(x, 1, kHeart, sizeof(kHeart));
    }
  }
}

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      max_matrix::ModuleType::kFc16);  // see the README for other modules
  DrawShape();
}

void loop() {
  /* the next shape only every kShowMs; the rest of loop() is free for other
   * work */
  if (millis() - last_shape < kShowMs) return;
  last_shape = millis();

  shape = static_cast<byte>((shape + 1) % kNumberOfShapes);
  DrawShape();
}
