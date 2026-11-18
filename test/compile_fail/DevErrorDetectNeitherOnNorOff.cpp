// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A switch that is neither STD_ON nor STD_OFF: #if takes it for STD_OFF, and
// no error is reported where the sketch asked for the reports
// (CodingStyle.md E-2).
// Expected: MAXMATRIX_DEV_ERROR_DETECT

#define MAXMATRIX_DEV_ERROR_DETECT 2

#include "MaxMatrix.h"
