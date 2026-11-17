// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// Pins as bare numbers, whose order nothing checks: the constructor has to
// refuse them (CodingStyle.md FN-16).
// Expected: from [^ ]*int[^ ]* to [^ ]*DataInPin

#include "MaxMatrix.h"

max_matrix::MatrixOf<4> matrix{11, 10, 13};
