// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// The clock pin where data in belongs: the constructor has to refuse it rather
// than bit bang into the clock (CodingStyle.md FN-16).
// Expected: from [^ ]*ClockPin[^ ]* to [^ ]*DataInPin

#include "MaxMatrix.h"

max_matrix::MatrixOf<4> matrix{max_matrix::ClockPin{13},
                               max_matrix::ChipSelectPin{10},
                               max_matrix::DataInPin{11}};
