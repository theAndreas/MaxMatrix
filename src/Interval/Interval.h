// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief When the next step of something that moves is due.
 */

#ifndef MAXMATRIX_SRC_INTERVAL_INTERVAL_H_
#define MAXMATRIX_SRC_INTERVAL_INTERVAL_H_

#include <stdint.h>

#include "MaxMatrixAttributes.h"

namespace max_matrix {

/*!
 * \brief Says when the next step of something that moves is due, a given number
 *        of milliseconds after the last one.
 *
 * The sketch reads the clock and asks; the library reads none. So a shifting
 * text takes one step per call of Matrix::ShiftStep(), and the sketch decides
 * when, as it does for an animation of its own:
 *
 * \code{.cpp}
 * max_matrix::Interval shift_interval{50};  // ms per column
 *
 * void loop() {
 *     if (shift_interval.IsDue(millis())) {
 *         matrix.ShiftStep();
 *     }
 * }
 * \endcode
 *
 * A check that comes late is due once, not for every step it missed: catching
 * up would make a text jump after `loop()` was held up. The interval then
 * counts from the late step. The first step is due one interval after the
 * clock's 0, which `millis()` is at the start.
 */
class Interval {
 public:
  /*!
   * \brief Makes an interval of `ms` milliseconds.
   *
   * \param[in] ms The milliseconds between two steps; 0 makes every check due.
   */
  explicit Interval(uint32_t ms) : ms_{ms} {}

  /*!
   * \brief Returns whether the next step is due, and if so, counts the next
   *        interval from `now`.
   *
   * \param[in] now The clock, as `millis()` gives it; it may overflow, as the
   *                difference to the last step is taken rather than a sum.
   * \return True once `ms` milliseconds have passed since the last step.
   */
  MAXMATRIX_NODISCARD bool IsDue(uint32_t now);

  /*! \brief Returns the milliseconds between two steps. */
  MAXMATRIX_NODISCARD uint32_t GetMs() const { return ms_; }

  /*!
   * \brief Sets the milliseconds between two steps, counted from the last one.
   *
   * \param[in] ms The milliseconds; 0 makes every check due.
   */
  void SetMs(uint32_t ms) { ms_ = ms; }

 private:
  /* 32 bits, as the clock: a sketch's interval beyond 16 bits was cut short
   * with no more than a warning, as Arduino compiles with -Wno-error=narrowing
   */
  uint32_t ms_;
  uint32_t last_step_{0};
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_INTERVAL_INTERVAL_H_
