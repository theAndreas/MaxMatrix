// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// Arithmetic on a byte of dots, which is raw data and no number: a Byte has to
// refuse it, so that a column is shifted or masked, never added to
// (CodingStyle.md T-17, E-3).
// Expected: (operand types are|binary expression) [^ ]*Byte

#include "MaxMatrix.h"

max_matrix::Byte NextColumn(max_matrix::Byte column) { return column + 1; }
