/*
 * Animation
 *
 * Bounces a ball off the edges of the matrix without blocking loop(): each step clears the ball
 * where it was and draws it where it is, and both go to the matrix as one picture.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

uint32_t const STEP_INTERVAL_MS = 40;

MaxMatrix Matrix(11, 10, 13);

int16_t BallX = 0;
int16_t BallY = 0;
int8_t StepX = 1;
int8_t StepY = 1;
uint32_t LastStep = 0;

void setup()
{
    Matrix.init();
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);    // FC-16, see the README
    Matrix.setDot(BallX, BallY, true);
}

void loop()
{
    /* a step only every STEP_INTERVAL_MS; the rest of loop() is free for other work */
    if(millis() - LastStep < STEP_INTERVAL_MS) return;
    LastStep = millis();

    /* without the batch the matrix would show the frame without a ball in between, as a flicker */
    MaxMatrixBatch Batch(Matrix);
    Matrix.setDot(BallX, BallY, false);
    /* turn round at an edge rather than leave the matrix */
    if(BallX + StepX < 0 || BallX + StepX >= MAXMATRIX_NUMBER_OF_COLUMNS) StepX = -StepX;
    if(BallY + StepY < 0 || BallY + StepY >= MAXMATRIX_ROW_NUMBER_OF_MODULE) StepY = -StepY;
    BallX += StepX;
    BallY += StepY;
    Matrix.setDot(BallX, BallY, true);
}
