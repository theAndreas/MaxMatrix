// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Messages
 *
 * Scrolls several texts one after the other, each at a speed of its own,
 * without blocking loop(). Umlauts can be typed as they are: the library reads
 * the UTF-8 the Arduino IDE saves. This file writes them as their two bytes of
 * UTF-8 only to stay ASCII, as the library's sources do.
 *
 * Wiring, to the board's hardware SPI pins, which SPI needs and bit banging
 * takes as well: DIN -> MOSI, CS -> SS, CLK -> SCK; on the Uno, Nano and
 * Uno R4 11, 10, 13, on the Pico 19, 17, 18, on the ESP32 23, 5, 18.
 */
#include <MaxMatrix.h>
#include <stdint.h>

/* the texts lie in flash with PROGMEM, where they take no RAM; the blanks at
 * the end let one leave the matrix before the next one comes in */
char const kHello[] PROGMEM = "Hello!    ";
/* "Gruene Aepfel, schoene Oefen" with umlauts; an escape takes every hex digit
 * after it, so the text goes on in a string of its own before the f */
char const kUmlauts[] PROGMEM =
    "Gr\xC3\xBCne \xC3\x84pfel, sch\xC3\xB6ne \xC3\x96"
    "fen    ";
char const kFast[] PROGMEM = "fast...    ";
char const kSlow[] PROGMEM = "and slow    ";

/* T-13: made as a whole, {kHello, 50}; a default for its interval would make it
 * no aggregate in C++11, as a sketch is compiled */
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): T-13, see above
struct Message {
  __FlashStringHelper const* text;
  uint16_t shift_interval_ms;
};

/* FromFlash() hands a text of the table on as F() hands on one in quotes */
auto const kMessages{max_matrix::make_array<Message>(
    Message{max_matrix::FromFlash(kHello), 50},
    Message{max_matrix::FromFlash(kUmlauts), 50},
    Message{max_matrix::FromFlash(kFast), 15},
    Message{max_matrix::FromFlash(kSlow), 120})};
uint8_t const kNumberOfMessages{kMessages.size()};

max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{MOSI},
                               max_matrix::ChipSelectPin{SS},
                               max_matrix::ClockPin{SCK}};
/* the pace of the text shown, which each message sets */
max_matrix::Interval shift_interval{50};
uint8_t current{0};

void Show(Message const& message) {
  shift_interval.SetMs(message.shift_interval_ms);
  matrix.SetTextWithShift(message.text);
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
  matrix.SetIntensity(2);
  Show(kMessages[current]);
}

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  /* a step each time the interval of the current text has passed */
  if (shift_interval.IsDue(millis())) {
    matrix.ShiftStep();
  }
  /* the text has been shifted in completely: on to the next one */
  if (matrix.GetState() == max_matrix::State::kReady) {
    current = static_cast<uint8_t>((current + 1) % kNumberOfMessages);
    Show(kMessages[current]);
  }
}
