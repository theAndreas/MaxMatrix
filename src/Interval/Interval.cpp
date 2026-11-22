// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief When the next step of something that moves is due.
 */

#include "Interval.h"

#include <stdint.h>

namespace max_matrix {

bool Interval::IsDue(uint32_t now) {
  // The difference rather than a comparison with last_step_ + ms_, which
  // wraps before the clock does (H-2).
  uint32_t const passed{now - last_step_};
  if (passed < ms_) {
    return false;
  }
  last_step_ = now;
  return true;
}

}  // namespace max_matrix
