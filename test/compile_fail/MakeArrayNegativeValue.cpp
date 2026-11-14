// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A negative value for an element type without a sign: make_array() has to stop
// the build rather than take 255 for it.

#include <stdint.h>

#include "Utility/array.h"

constexpr auto kNegative{max_matrix::make_array<uint8_t>(1, -1)};
