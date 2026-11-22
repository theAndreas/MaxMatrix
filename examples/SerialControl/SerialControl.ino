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
 * Wiring, to the board's hardware SPI pins, which SPI needs and bit banging
 * takes as well: DIN -> MOSI, CS -> SS, CLK -> SCK; on the Uno, Nano and
 * Uno R4 11, 10, 13, on the Pico 19, 17, 18, on the ESP32 23, 5, 18.
 */
#include <MaxMatrix.h>
#include <stdint.h>

max_matrix::DataInPin const kDataInPin{MOSI};
max_matrix::ChipSelectPin const kChipSelectPin{SS};
max_matrix::ClockPin const kClockPin{SCK};
uint32_t const kBaudRate{115200};

max_matrix::MatrixOf<4> matrix{kDataInPin, kChipSelectPin, kClockPin};
/* the pace of a shifting text, which speed= sets */
max_matrix::Interval shift_interval{50};
max_matrix::Command command;

void setup() {
  Serial.begin(kBaudRate);
  // A pin the board lacks fails: the LED on the board says so, where it
  // has one, and loop() draws nothing, as the matrix is not set up.
  if (!matrix.Init().IsOk()) {
#ifdef LED_BUILTIN
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    return;
  }
  /* the common 4-in-1 FC-16 boards; the README lists the other modules */
  matrix.SetModuleType(max_matrix::ModuleType::kFc16);
  Serial.println(F("ready"));
}

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  while (Serial.available() > 0) {
    command.Feed(matrix, shift_interval, static_cast<char>(Serial.read()),
                 Serial);
  }
  if (shift_interval.IsDue(millis())) {
    matrix.ShiftStep();
  }
}
