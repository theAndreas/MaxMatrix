/*
 * ScrollText
 *
 * Scrolls a text endlessly over the LED matrix without blocking loop().
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 *
 * A chain of four modules, as MaxMatrixOf<4> says below; one of eight is MaxMatrixOf<8>.
 */
#include <MaxMatrix.h>

byte const kDataInPin = 11;
byte const kChipSelectPin = 10;
byte const kClockPin = 13;
uint16_t const kShiftIntervalMs = 50;

MaxMatrixOf<4> matrix(kDataInPin, kChipSelectPin, kClockPin);

/* Text must stay valid while it is shifted, so don't use a local variable */
char text[] = "Hello World!   ";

void setup()
{
    matrix.Init();
    /* the common 4-in-1 FC-16 boards; the README lists the other modules */
    matrix.SetModuleType(MaxMatrixModuleType::kFc16);
    matrix.SetIntensity(2);
    matrix.SetShiftInterval(kShiftIntervalMs);
    matrix.SetTextWithShift(text);
}

void loop()
{
    /* takes a step only every kShiftIntervalMs, so it can be called on every loop() */
    matrix.ShiftTask();
    /* restart when the whole text was shifted */
    if(matrix.GetState() == MaxMatrixState::kReady) matrix.SetTextWithShift(text);

    /* other work can be done here */
}
