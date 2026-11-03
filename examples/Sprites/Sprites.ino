/*
 * Sprites
 *
 * Walks a sprite of its own across the matrix, from beyond the right edge to beyond the left one,
 * with two frames that alternate as it goes.
 *
 * A sprite is the width, the height and up to five columns, bit 0 the top row, as the chars of
 * the font are. Drawing it partly off the matrix is fine: what lies outside is left out.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

uint16_t const STEP_DELAY_MS = 100;
int16_t const SPRITE_Y = 2;

/*   .###.      .###.
 *   #.#.#      #.#.#
 *   #####      #####
 *   .#.#.      .#.#.
 *   #...#      .#.#.  */
spriteType const InvaderLegsOut = { 5, 5, 0x16, 0x0D, 0x07, 0x0D, 0x16 };
spriteType const InvaderLegsIn = { 5, 5, 0x06, 0x1D, 0x07, 0x1D, 0x06 };

MaxMatrix Matrix(11, 10, 13);

void setup()
{
    Matrix.init();
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);    // FC-16, see the README
}

void loop()
{
    int16_t const Width = InvaderLegsOut[ASCII_TABLE_SPRITE_WIDTH];

    for(int16_t X = MAXMATRIX_NUMBER_OF_COLUMNS; X >= -Width; X--) {
        {
            /* cleared and drawn again as one picture, so that the column it left goes dark without a flicker */
            MaxMatrixBatch Batch(Matrix);
            Matrix.clear();
            Matrix.setSprite(X, SPRITE_Y, (X & 1) ? InvaderLegsOut : InvaderLegsIn);
        }
        delay(STEP_DELAY_MS);
    }
}
