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
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

/* a text must stay valid while it is shifted, so none of them is a local
 * variable; the blanks at the end let it leave the matrix before the next one
 * comes in */
char const kHello[] = "Hello!    ";
/* "Gruene Aepfel, schoene Oefen" with umlauts; an escape takes every hex digit
 * after it, so the text goes on in a string of its own before the f */
char const kUmlauts[] =
    "Gr\xC3\xBCne \xC3\x84pfel, sch\xC3\xB6ne \xC3\x96"
    "fen    ";
char const kFast[] = "fast...    ";
char const kSlow[] = "and slow    ";

struct Message {
  char const* text;
  uint16_t shift_interval_ms;
};

Message const kMessages[]{
    {kHello, 50},
    {kUmlauts, 50},
    {kFast, 15},
    {kSlow, 120},
};
uint8_t const kNumberOfMessages{sizeof(kMessages) / sizeof(kMessages[0])};

max_matrix::MatrixOf<4> matrix{11, 10, 13};
uint8_t current{0};

void Show(Message const& message) {
  matrix.SetShiftInterval(message.shift_interval_ms);
  matrix.SetTextWithShift(message.text);
}

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      max_matrix::ModuleType::kFc16);  // see the README for other modules
  matrix.SetIntensity(2);
  Show(kMessages[current]);
}

void loop() {
  /* takes a step only when the interval of the current text has passed */
  matrix.ShiftTask();
  /* the text has been shifted in completely: on to the next one */
  if (matrix.GetState() == max_matrix::State::kReady) {
    current = static_cast<uint8_t>((current + 1) % kNumberOfMessages);
    Show(kMessages[current]);
  }
}
