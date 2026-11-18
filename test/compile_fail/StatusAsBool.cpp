// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

// A status taken for a bool, which was true on an error when it was a number:
// a Status has to refuse it, so that a sketch asks IsOk() (CodingStyle.md E-3).
// Expected: Status[^ ]* (is not contextually convertible )?to [^ ]*bool

#include "MaxMatrix.h"

bool ShowText(max_matrix::Matrix& matrix) {
  bool shown{false};
  if (matrix.SetText("Hi")) {
    shown = true;
  }
  return shown;
}
