// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Basics
 *
 * Shows text, single dots, rows, a picture drawn in one go and the intensity
 * setting.
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

/* a chain of four modules, with the buffer for them; one of eight is
 * max_matrix::MatrixOf<8> */
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
    auto const row{
        static_cast<int16_t>(column % max_matrix::kRowNumberOfModule)};
    matrix.SetDot(Point{X{column}, Y{row}}, true);
    delay(30);
  }
  delay(1000);
}

/* row 3 on every module */
void FillRow() {
  for (uint8_t module{0}; module < matrix.GetNumberOfModules(); module++) {
    matrix.SetRow(max_matrix::Module{module}, 3, max_matrix::Byte{0xFF});
  }
  delay(1000);
}

/* a frame of many dots, shown at once rather than dot by dot */
void DrawFrame() {
  {
    // nothing is sent until the end of this block
    max_matrix::Batch const batch{matrix};
    X const right{static_cast<int16_t>(matrix.GetNumberOfColumns() - 1)};
    Y const bottom{max_matrix::kRowNumberOfModule - 1};

    matrix.Clear();
    for (uint8_t column{0}; column < matrix.GetNumberOfColumns(); column++) {
      matrix.SetDot(Point{X{column}, Y{0}}, true);
      matrix.SetDot(Point{X{column}, bottom}, true);
    }
    for (uint8_t row{0}; row < max_matrix::kRowNumberOfModule; row++) {
      matrix.SetDot(Point{X{0}, Y{row}}, true);
      matrix.SetDot(Point{right, Y{row}}, true);
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
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  ShowText();
  DrawDiagonal();
  FillRow();
  DrawFrame();
  FadeIn();
}
