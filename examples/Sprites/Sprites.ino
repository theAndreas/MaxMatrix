// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*
 * Sprites
 *
 * Walks a sprite of its own across the matrix, from beyond the right edge to
 * beyond the left one, with two frames that alternate as it goes.
 *
 * A sprite is the width, the height and up to five columns, bit 0 the top row,
 * as the chars of the font are. Drawing it partly off the matrix is fine: what
 * lies outside is left out.
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

uint16_t const kStepDelayMs{100};
Y const kSpriteY{2};

/*   .###.      .###.
 *   #.#.#      #.#.#
 *   #####      #####
 *   .#.#.      .#.#.
 *   #...#      .#.#.  */
max_matrix::Sprite const kInvaderLegsOut{
    5, 5, {{0x16, 0x0D, 0x07, 0x0D, 0x16}}};
max_matrix::Sprite const kInvaderLegsIn{5, 5, {{0x06, 0x1D, 0x07, 0x1D, 0x06}}};

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

void loop() {
  if (matrix.GetState() == max_matrix::State::kNone) {
    return;
  }
  int16_t const width{kInvaderLegsOut.width};

  for (int16_t x{matrix.GetNumberOfColumns()}; x >= -width; x--) {
    {
      /* cleared and drawn again as one picture, so that the column it left goes
       * dark without a flicker */
      max_matrix::Batch const batch{matrix};
      matrix.Clear();
      matrix.SetSprite(Point{X{x}, kSpriteY},
                       ((x % 2) != 0) ? kInvaderLegsOut : kInvaderLegsIn);
    }
    delay(kStepDelayMs);
  }
}
