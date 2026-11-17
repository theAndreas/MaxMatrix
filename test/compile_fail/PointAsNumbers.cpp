// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A point of bare numbers, whose column and row could be swapped unnoticed: a
// Point has to refuse them (CodingStyle.md FN-16).
// Expected: from [^ ]*int[^ ]* to [^ ]*X

#include "MaxMatrix.h"

void DrawByNumbers(max_matrix::Matrix& matrix) {
  matrix.SetDot(max_matrix::Point{3, 2}, true);
}
