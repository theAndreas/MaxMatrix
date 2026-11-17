// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// The column where the module belongs: SetColumn() has to refuse it rather than
// draw column 1 of module 3 for column 3 of module 1 (CodingStyle.md FN-16).
// Expected: from [^ ]*int[^ ]* to [^ ]*Module

#include "MaxMatrix.h"

void SetSwapped(max_matrix::Matrix& matrix) {
  matrix.SetColumn(3, max_matrix::Module{1}, max_matrix::Byte{0x3C});
}
