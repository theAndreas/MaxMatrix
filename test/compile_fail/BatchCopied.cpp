// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A copy of a batch, which would put auto update back twice, the second time
// to what the first had turned off: a Batch has to refuse it (CodingStyle.md
// E-3).
// Expected: deleted (function|constructor of) ('const )?[^ ]*Batch

#include "MaxMatrix.h"

void DrawTwice(max_matrix::Matrix& matrix) {
  max_matrix::Batch const batch{matrix};
  max_matrix::Batch const copy{batch};
}
