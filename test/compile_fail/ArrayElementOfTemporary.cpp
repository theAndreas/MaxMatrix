// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// The element of a temporary array: the reference would dangle at the end of
// the statement, so the index operator refuses it (CodingStyle.md C-16).
// Expected: deleted (function|operator)

#include <stdint.h>

#include "Utility/array.h"

max_matrix::array<uint8_t, 2> MakeArray() { return {{1, 2}}; }

uint8_t const& kDangling{MakeArray()[0]};
