// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* SimulatorAvr: the simulated time it runs the AVR to. */

#include "Avr/SimulatorAvr.h"

#include <gtest/gtest.h>
#include <stdint.h>

namespace {

/* Bug: the end of a stretch was taken from simavr's microseconds as a uint32_t,
 * which wrapped after 4294967 ms, 71.6 minutes; a run of the page or the
 * control program, which lasts until Ctrl+C, then ran no cycle any more, and
 * the AVR stood still */
TEST(SimulatorAvrTest, CyclesGoOnPastSeventyOneMinutes) {
  static_assert(SimulatorAvr::ConvertToCycles(1) == 16000u,
                "made when compiling");
  uint32_t const past_the_wrap{4294968u};

  EXPECT_EQ(SimulatorAvr::ConvertToCycles(past_the_wrap), 68719488000u);
  EXPECT_GT(SimulatorAvr::ConvertToCycles(past_the_wrap),
            SimulatorAvr::ConvertToCycles(past_the_wrap - 1));
}

}  // namespace
