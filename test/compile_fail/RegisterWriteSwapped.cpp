// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// The value where the address belongs: RegisterWrite() has to refuse it rather
// than write register 5 with the intensity's address (CodingStyle.md FN-16).
// GCC says it cannot convert the int to Register, Clang that it cannot make a
// parameter of type Register with an rvalue of type int.
// Expected: (int[^ ]* to [^ ]*Register|Register[^ ]* with an rvalue of type)

#include "MaxMatrix.h"

void WriteSwapped(max_matrix::Matrix& matrix) {
  matrix.RegisterWrite(5, max_matrix::max7219::Register::kIntensity);
}
