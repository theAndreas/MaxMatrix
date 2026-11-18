// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A shift interval beyond 16 bits: braces only warn of it in a sketch, as
// Arduino compiles with -Wno-error=narrowing, and a text then shifted 65536 ms
// faster than asked (CodingStyle.md E-2).
// Expected: MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE

#define MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE 70000

#include "MaxMatrix.h"
