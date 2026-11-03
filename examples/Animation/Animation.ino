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

uint32_t const kStepIntervalMs = 40;

MaxMatrix matrix(11, 10, 13);

int16_t ball_x = 0;
int16_t ball_y = 0;
int8_t step_x = 1;
int8_t step_y = 1;
uint32_t last_step = 0;

void setup()
{
    matrix.Init();
    matrix.SetModuleOrientation(MaxMatrixModuleOrientation::k270);    // FC-16, see the README
    matrix.SetDot(ball_x, ball_y, true);
}

void loop()
{
    /* a step only every STEP_INTERVAL_MS; the rest of loop() is free for other work */
    if(millis() - last_step < kStepIntervalMs) return;
    last_step = millis();

    /* without the batch the matrix would show the frame without a ball in between, as a flicker */
    MaxMatrixBatch batch(matrix);
    matrix.SetDot(ball_x, ball_y, false);
    /* turn round at an edge rather than leave the matrix */
    if(ball_x + step_x < 0 || ball_x + step_x >= MAXMATRIX_NUMBER_OF_COLUMNS) step_x = -step_x;
    if(ball_y + step_y < 0 || ball_y + step_y >= MAXMATRIX_ROW_NUMBER_OF_MODULE) step_y = -step_y;
    ball_x += step_x;
    ball_y += step_y;
    matrix.SetDot(ball_x, ball_y, true);
}
