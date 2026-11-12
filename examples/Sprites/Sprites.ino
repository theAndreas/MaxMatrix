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
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

uint16_t const kStepDelayMs{100};
int16_t const kSpriteY{2};

/*   .###.      .###.
 *   #.#.#      #.#.#
 *   #####      #####
 *   .#.#.      .#.#.
 *   #...#      .#.#.  */
max_matrix::Sprite const kInvaderLegsOut{5, 5, 0x16, 0x0D, 0x07, 0x0D, 0x16};
max_matrix::Sprite const kInvaderLegsIn{5, 5, 0x06, 0x1D, 0x07, 0x1D, 0x06};

max_matrix::MatrixOf<4> matrix{11, 10, 13};

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      max_matrix::ModuleType::kFc16);  // see the README for other modules
}

void loop() {
  int16_t const width{kInvaderLegsOut[max_matrix::kSpriteWidth]};

  for (int16_t x{matrix.GetNumberOfColumns()}; x >= -width; x--) {
    {
      /* cleared and drawn again as one picture, so that the column it left goes
       * dark without a flicker */
      max_matrix::Batch const batch{matrix};
      matrix.Clear();
      matrix.SetSprite(x, kSpriteY,
                       x % 2 != 0 ? kInvaderLegsOut : kInvaderLegsIn);
    }
    delay(kStepDelayMs);
  }
}
