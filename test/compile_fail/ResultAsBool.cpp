// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A result taken for a bool, which would say whether the call worked, not what
// it found: a Result has to refuse it (CodingStyle.md E-3).
// Expected: Result[^ ]* (is not contextually convertible )?to [^ ]*bool

#include "MaxMatrix.h"

bool IsCornerLit(max_matrix::Matrix& matrix) {
  bool lit{false};
  if (matrix.GetDot(max_matrix::Point{max_matrix::X{0}, max_matrix::Y{0}})) {
    lit = true;
  }
  return lit;
}
