// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// An array of no element: array has to refuse it when compiling, as its C array
// of no element is none C++ allows (CodingStyle.md E-3).
// Expected: needs at least one element

#include <stdint.h>

#include "Utility/array.h"

max_matrix::array<uint8_t, 0> const kNothing{};
