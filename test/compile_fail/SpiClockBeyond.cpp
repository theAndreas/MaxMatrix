// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// An SPI clock beyond the 10 MHz the MAX7219 takes: the core picks the fastest
// clock up to it, which a board faster than the Uno would send too fast for the
// chain (CodingStyle.md E-2).
// Expected: MAXMATRIX_SPI_CLOCK_SPEED

#define MAXMATRIX_SPI_CLOCK_SPEED 20000000

#include "MaxMatrix.h"
