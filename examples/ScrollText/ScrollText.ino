/*
 * ScrollText
 *
 * Scrolls a text endlessly over the LED matrix without blocking loop().
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 *
 * The number of modules is configured with MAXMATRIX_NUMBER_OF_MODULES in MaxMatrix.h.
 */
#include <MaxMatrix.h>

const byte DATA_IN_PIN = 11;
const byte CHIP_SELECT_PIN = 10;
const byte CLOCK_PIN = 13;
const unsigned long SHIFT_INTERVAL_MS = 50;

MaxMatrix Matrix(DATA_IN_PIN, CHIP_SELECT_PIN, CLOCK_PIN);

/* text must stay valid while it is shifted, so don't use a local variable */
char Text[] = "Hello World!   ";
unsigned long LastShift = 0;

void setup()
{
    Matrix.init();
    /* 270 for the common 4-in-1 FC-16 boards; the README lists the other modules */
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);
    Matrix.setIntensity(2);
    Matrix.setTextWithShift(Text);
}

void loop()
{
    if(millis() - LastShift >= SHIFT_INTERVAL_MS) {
        LastShift = millis();
        Matrix.shiftTask();
        /* restart when the whole text was shifted */
        if(Matrix.getState() == MAXMATRIX_STATE_READY) Matrix.setTextWithShift(Text);
    }

    /* other work can be done here */
}
