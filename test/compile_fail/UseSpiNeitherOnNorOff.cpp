// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A switch that is neither STD_ON nor STD_OFF: #if takes it for STD_OFF, and
// the library bit bangs where the sketch meant SPI (CodingStyle.md E-2).
// Expected: MAXMATRIX_USE_SPI

#define MAXMATRIX_USE_SPI 2

#include "MaxMatrix.h"
