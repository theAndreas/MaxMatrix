/*
 * SerialControl
 *
 * Takes commands over Serial, one per line, and answers each with "ok" or "error":
 *
 *   text=Hello            shows a text from the left edge
 *   textShift=Hello       shifts a text in from the right
 *   intensity=8           brightness, 0 to 15
 *   speed=50              milliseconds per column of a shifting text
 *   space=1               empty columns between chars
 *   setDot=5,3,1          column, row, 1 for on and 0 for off
 *   clear                 turns every LED off
 *
 * The serial monitor of the Arduino IDE sends them at 115200 baud with "Newline" as line ending; so do the control
 * page and the control program of MaxMatrix.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

const byte DATA_IN_PIN = 11;
const byte CHIP_SELECT_PIN = 10;
const byte CLOCK_PIN = 13;
const uint32_t BAUD_RATE = 115200;

MaxMatrix Matrix(DATA_IN_PIN, CHIP_SELECT_PIN, CLOCK_PIN);
MaxMatrixCommand Command;

void setup()
{
    Serial.begin(BAUD_RATE);
    Matrix.init();
    /* 270 for the common 4-in-1 FC-16 boards; the README lists the other modules */
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);
    Serial.println(F("ready"));
}

void loop()
{
    while(Serial.available() > 0) {
        MaxMatrixCommandResultType Result = Command.feed(Matrix, Serial.read());
        if(Result == MAXMATRIX_COMMAND_DONE) Serial.println(F("ok"));
        else if(Result != MAXMATRIX_COMMAND_PENDING) Serial.println(F("error"));
    }
    Matrix.shiftTask();
}
