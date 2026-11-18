// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A bare number where a byte of dots belongs: SetColumn() has to refuse it, so
// that a number is never taken for dots unnoticed (CodingStyle.md T-17, E-3).
// Expected: from [^ ]*int[^ ]* to [^ ]*Byte

#include "MaxMatrix.h"

void DrawByNumber(max_matrix::Matrix& matrix) {
  matrix.SetColumn(max_matrix::X{1}, 0x3C);
}
