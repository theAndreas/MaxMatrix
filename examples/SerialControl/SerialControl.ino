// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * SerialControl
 *
 * Takes commands over Serial, one per line, and answers each with "ok", or with
 * "error: " and the reason, such as "error: out of range" for intensity=16;
 * picture is answered with "picture=" and the columns instead:
 *
 *   text=Hello            shows a text from the left edge
 *   textShift=Hello       shifts a text in from the right
 *   intensity=8           brightness, 0 to 15
 *   speed=50              milliseconds per column of a shifting text
 *   space=1               empty columns between chars
 *   setDot=5,3,1          column, row, 1 for on and 0 for off
 *   clear                 turns every LED off
 *   picture               answers with what the matrix shows, two hex digits
 * per column
 *
 * The serial monitor of the Arduino IDE sends them at 115200 baud with
 * "Newline" as line ending; so do the control page and the control program of
 * MaxMatrix.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

byte const kDataInPin = 11;
byte const kChipSelectPin = 10;
byte const kClockPin = 13;
uint32_t const kBaudRate = 115200;

MaxMatrixOf<4> matrix(kDataInPin, kChipSelectPin, kClockPin);
MaxMatrixCommand command;

void setup() {
  Serial.begin(kBaudRate);
  matrix.Init();
  /* the common 4-in-1 FC-16 boards; the README lists the other modules */
  matrix.SetModuleType(MaxMatrixModuleType::kFc16);
  Serial.println(F("ready"));
}

void loop() {
  while (Serial.available() > 0)
    command.Feed(matrix, static_cast<char>(Serial.read()), Serial);
  matrix.ShiftTask();
}
