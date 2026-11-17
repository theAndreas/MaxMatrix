// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// The module as a bare number, which a column can stand in for unnoticed:
// GetColumn() has to refuse it (CodingStyle.md FN-16).
// Expected: from [^ ]*int[^ ]* to [^ ]*Module

#include "MaxMatrix.h"

max_matrix::Result<max_matrix::Byte> GetByNumbers(max_matrix::Matrix& matrix) {
  return matrix.GetColumn(1, 3);
}
