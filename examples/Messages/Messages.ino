// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Messages
 *
 * Scrolls several texts one after the other, each at a speed of its own,
 * without blocking loop(). Umlauts can be typed as they are: the library reads
 * the UTF-8 the Arduino IDE saves.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

/* a text must stay valid while it is shifted, so none of them is a local
 * variable; the blanks at the end let it leave the matrix before the next one
 * comes in */
char hello[] = "Hello!    ";
char umlauts[] = "Grüne Äpfel, schöne Öfen    ";
char fast[] = "fast...    ";
char slow[] = "and slow    ";

struct Message {
  char* text;
  uint16_t shift_interval_ms;
};

Message const kMessages[] = {
    {hello, 50},
    {umlauts, 50},
    {fast, 15},
    {slow, 120},
};
byte const kNumberOfMessages = sizeof(kMessages) / sizeof(kMessages[0]);

MaxMatrixOf<4> matrix(11, 10, 13);
byte current = 0;

void Show(Message const& message) {
  matrix.SetShiftInterval(message.shift_interval_ms);
  matrix.SetTextWithShift(message.text);
}

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      MaxMatrixModuleType::kFc16);  // see the README for other modules
  matrix.SetIntensity(2);
  Show(kMessages[current]);
}

void loop() {
  /* takes a step only when the interval of the current text has passed */
  matrix.ShiftTask();
  /* the text has been shifted in completely: on to the next one */
  if (matrix.GetState() == MaxMatrixState::kReady) {
    current = (current + 1) % kNumberOfMessages;
    Show(kMessages[current]);
  }
}
