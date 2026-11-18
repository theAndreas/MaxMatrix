// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A column of the panel as a bare number, which a row could stand in for
// unnoticed: SetColumn() has to refuse it (CodingStyle.md FN-16, E-3).
// Expected: from [^ ]*int[^ ]* to [^ ]*X

#include "MaxMatrix.h"

void DrawByNumber(max_matrix::Matrix& matrix) {
  matrix.SetColumn(5, max_matrix::Byte{0x3C});
}
