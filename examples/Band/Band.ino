/*
 * Band
 *
 * Runs a heartbeat over the matrix without end, without blocking loop(): every step shifts the
 * picture one column to the left and brings in the next column of the beat at the right. The
 * columns come from flash here, but they may as well come from a sensor or a calculation.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

uint32_t const kStepIntervalMs = 60;

/* one beat, a column per byte, bit 0 the top row: a line in row 5 with a spike up and down */
byte const kBeat[] PROGMEM = {
    0x20, 0x20, 0x20, 0x10, 0x08, 0x10, 0x20, 0x20, 0x60, 0x3F, 0xE0, 0x20,
    0x20, 0x20, 0x10, 0x08, 0x08, 0x10, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};

MaxMatrixOf<4> matrix(11, 10, 13);

byte next_column = 0;
uint32_t last_step = 0;

void setup()
{
    matrix.Init();
    matrix.SetModuleType(MaxMatrixModuleType::kFc16);    // see the README for other modules
}

void loop()
{
    /* a step only every kStepIntervalMs; the rest of loop() is free for other work */
    if(millis() - last_step < kStepIntervalMs) return;
    last_step = millis();

    /* the column that falls out at the left comes back, for a sketch that passes it on */
    matrix.ShiftLeft(pgm_read_byte(kBeat + next_column));
    next_column = (next_column + 1) % sizeof(kBeat);
}
