// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// An intensity Init() sets beyond the MAX7219's 15: Init() dropped the error of
// SetIntensity(), and the matrix started at whatever intensity the chips had
// (CodingStyle.md E-2).
// Expected: MAXMATRIX_INTENSITY_INIT_VALUE

#define MAXMATRIX_INTENSITY_INIT_VALUE 16

#include "MaxMatrix.h"
