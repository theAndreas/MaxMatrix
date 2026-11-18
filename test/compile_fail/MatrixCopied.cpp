// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A copy of a matrix, which would send to the same chain from a buffer of its
// own: a Matrix has to refuse it (CodingStyle.md E-3).
// Expected: deleted (function|copy constructor of) [^ ]*MatrixOf

#include "MaxMatrix.h"

max_matrix::MatrixOf<4> matrix{max_matrix::DataInPin{11},
                               max_matrix::ChipSelectPin{10},
                               max_matrix::ClockPin{13}};
max_matrix::MatrixOf<4> copy{matrix};
