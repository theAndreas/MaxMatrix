// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A space between chars beyond a byte: braces only warn of it in a sketch, as
// Arduino compiles with -Wno-error=narrowing, and the text then stood 256
// columns apart less (CodingStyle.md E-2).
// Expected: MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE

#define MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE 300

#include "MaxMatrix.h"
