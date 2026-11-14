// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A value that does not fit the element type: make_array() has to stop the
// build, where braces only warn in a sketch, as Arduino compiles with
// -Wno-error=narrowing.

#include <stdint.h>

#include "Utility/array.h"

constexpr auto kTooLarge{max_matrix::make_array<uint8_t>(1, 256)};
