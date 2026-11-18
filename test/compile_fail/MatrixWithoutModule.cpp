// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A chain of no module: MatrixOf has to refuse it when compiling rather than
// give a buffer of no byte (CodingStyle.md E-3).
// Expected: a chain has 1 to kMaxNumberOfModules modules

#include "MaxMatrix.h"

max_matrix::MatrixOf<0> matrix{max_matrix::DataInPin{11},
                               max_matrix::ChipSelectPin{10},
                               max_matrix::ClockPin{13}};
