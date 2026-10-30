/*
 * Basics
 *
 * Shows text, single dots, rows, a picture drawn in one go and the intensity setting.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

MaxMatrix Matrix(11, 10, 13);

void setup()
{
    Matrix.init();
    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);    // FC-16, see the README
}

void loop()
{
    /* static text */
    Matrix.clear();
    Matrix.setText("Hi!");
    delay(2000);

    /* draw a diagonal line dot by dot */
    Matrix.clear();
    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        Matrix.setDot(Column, Column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
        delay(30);
    }
    delay(1000);

    /* fill row 3 on every module */
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        Matrix.setRow(Module, 3, 0xFF);
    }
    delay(1000);

    /* draw a frame from many dots and show it at once, rather than dot by dot */
    Matrix.setAutoUpdate(false);
    Matrix.clear();
    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        Matrix.setDot(Column, 0, true);
        Matrix.setDot(Column, MAXMATRIX_ROW_NUMBER_OF_MODULE - 1, true);
    }
    for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
        Matrix.setDot(0, Row, true);
        Matrix.setDot(MAXMATRIX_NUMBER_OF_COLUMNS - 1, Row, true);
    }
    Matrix.update();
    Matrix.setAutoUpdate(true);
    delay(1000);

    /* fade the intensity */
    for(byte Intensity = 0; Intensity <= 15; Intensity++) {
        Matrix.setIntensity(Intensity);
        delay(100);
    }
    Matrix.setIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
}
