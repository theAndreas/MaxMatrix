// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A getter's result dropped, whose value is lost and the error that says
// whether there was one: the compiler has to say so in both standards, as
// MAXMATRIX_NODISCARD may be another attribute in each (FN-10).
// Expected: -Wunused-result

#include "MaxMatrix.h"

void DropColumn(max_matrix::Matrix& matrix) { matrix.GetColumn(0); }
