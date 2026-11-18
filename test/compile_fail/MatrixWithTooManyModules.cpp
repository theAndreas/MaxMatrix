// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// More modules than a byte counts the columns of: MatrixOf has to refuse them
// when compiling rather than let the index of a column wrap (CodingStyle.md
// E-3).
// Expected: a chain has 1 to kMaxNumberOfModules modules

#include "MaxMatrix.h"

max_matrix::MatrixOf<32> matrix{max_matrix::DataInPin{11},
                                max_matrix::ChipSelectPin{10},
                                max_matrix::ClockPin{13}};
