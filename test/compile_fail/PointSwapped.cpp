// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// The row where the column of a point belongs: a Point has to refuse it rather
// than light the dot at column 2, row 3 (CodingStyle.md FN-16).
// Expected: from [^ ]*Y[^ ]* to [^ ]*X

#include "MaxMatrix.h"

void DrawSwapped(max_matrix::Matrix& matrix) {
  matrix.SetDot(max_matrix::Point{max_matrix::Y{2}, max_matrix::X{3}}, true);
}
