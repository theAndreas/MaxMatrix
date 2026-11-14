// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Basics
 *
 * Shows text, single dots, rows, a picture drawn in one go and the intensity
 * setting.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

/* a chain of four modules, with the buffer for them; one of eight is
 * max_matrix::MatrixOf<8> */
max_matrix::MatrixOf<4> matrix{11, 10, 13};

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      max_matrix::ModuleType::kFc16);  // see the README for other modules
}

/* text that stands */
void ShowText() {
  matrix.Clear();
  matrix.SetText("Hi!");
  delay(2000);
}

/* a diagonal line, dot by dot */
void DrawDiagonal() {
  matrix.Clear();
  for (uint8_t column{0}; column < matrix.GetNumberOfColumns(); column++) {
    matrix.SetDot(column, column % max_matrix::kRowNumberOfModule, true);
    delay(30);
  }
  delay(1000);
}

/* row 3 on every module */
void FillRow() {
  for (uint8_t module{0}; module < matrix.GetNumberOfModules(); module++) {
    matrix.SetRow(module, 3, 0xFF);
  }
  delay(1000);
}

/* a frame of many dots, shown at once rather than dot by dot */
void DrawFrame() {
  {
    // nothing is sent until the end of this block
    max_matrix::Batch const batch{matrix};
    matrix.Clear();
    for (uint8_t column{0}; column < matrix.GetNumberOfColumns(); column++) {
      matrix.SetDot(column, 0, true);
      matrix.SetDot(column, max_matrix::kRowNumberOfModule - 1, true);
    }
    for (uint8_t row{0}; row < max_matrix::kRowNumberOfModule; row++) {
      matrix.SetDot(0, row, true);
      matrix.SetDot(matrix.GetNumberOfColumns() - 1, row, true);
    }
  }
  delay(1000);
}

/* the intensity, from dark to bright */
void FadeIn() {
  for (uint8_t intensity{0}; intensity <= 15; intensity++) {
    matrix.SetIntensity(intensity);
    delay(100);
  }
  matrix.SetIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
}

void loop() {
  ShowText();
  DrawDiagonal();
  FillRow();
  DrawFrame();
  FadeIn();
}
