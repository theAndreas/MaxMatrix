/*
 * Basics
 *
 * Shows text, single dots, rows, a picture drawn in one go and the intensity
 * setting.
 *
 * Wiring (Arduino Uno / Nano):
 *   DIN -> 11, CS -> 10, CLK -> 13
 */
#include <MaxMatrix.h>

/* a chain of four modules, with the buffer for them; one of eight is
 * MaxMatrixOf<8> */
MaxMatrixOf<4> matrix(11, 10, 13);

void setup() {
  matrix.Init();
  matrix.SetModuleType(
      MaxMatrixModuleType::kFc16);  // see the README for other modules
}

void loop() {
  /* static text */
  matrix.Clear();
  matrix.SetText("Hi!");
  delay(2000);

  /* Draw a diagonal line dot by dot */
  matrix.Clear();
  for (byte column = 0; column < matrix.GetNumberOfColumns(); column++) {
    matrix.SetDot(column, column % MAXMATRIX_ROW_NUMBER_OF_MODULE, true);
    delay(30);
  }
  delay(1000);

  /* fill row 3 on every module */
  for (byte module = 0; module < matrix.GetNumberOfModules(); module++) {
    matrix.SetRow(module, 3, 0xFF);
  }
  delay(1000);

  /* Draw a frame from many dots and show it at once, rather than dot by dot */
  {
    MaxMatrixBatch batch(
        matrix);  // nothing is sent until the end of this block
    matrix.Clear();
    for (byte column = 0; column < matrix.GetNumberOfColumns(); column++) {
      matrix.SetDot(column, 0, true);
      matrix.SetDot(column, MAXMATRIX_ROW_NUMBER_OF_MODULE - 1, true);
    }
    for (byte row = 0; row < MAXMATRIX_ROW_NUMBER_OF_MODULE; row++) {
      matrix.SetDot(0, row, true);
      matrix.SetDot(matrix.GetNumberOfColumns() - 1, row, true);
    }
  }
  delay(1000);

  /* fade the intensity */
  for (byte intensity = 0; intensity <= 15; intensity++) {
    matrix.SetIntensity(intensity);
    delay(100);
  }
  matrix.SetIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
}
