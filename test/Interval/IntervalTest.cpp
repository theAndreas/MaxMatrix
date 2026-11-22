// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Interval: when a step of something that moves is due, from a clock the
 * sketch reads, at 50 ms unless a test says otherwise. */

#include "Interval/Interval.h"

#include <gtest/gtest.h>

namespace max_matrix {
namespace {

TEST(IntervalTest, GivesTheMillisecondsItIsMadeWith) {
  Interval const interval{50};

  EXPECT_EQ(interval.GetMs(), 50);
}

TEST(IntervalTest, GivesTheMillisecondsSetLast) {
  Interval interval{50};

  interval.SetMs(120);

  EXPECT_EQ(interval.GetMs(), 120);
}

/* the clock of a sketch starts at 0, and so does the interval: the first step
 * comes one interval after the start */
TEST(IntervalTest, IsNotDueBeforeTheFirstIntervalHasPassed) {
  Interval interval{50};

  EXPECT_FALSE(interval.IsDue(49));
}

TEST(IntervalTest, IsDueOnceTheFirstIntervalHasPassed) {
  Interval interval{50};

  EXPECT_TRUE(interval.IsDue(50));
}

TEST(IntervalTest, IsNotDueBeforeTheNextIntervalHasPassed) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(1000));

  EXPECT_FALSE(interval.IsDue(1049));
}

TEST(IntervalTest, IsDueOnceTheNextIntervalHasPassed) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(1000));

  EXPECT_TRUE(interval.IsDue(1050));
}

/* a check after loop() was held up is due once, not for the steps it missed */
TEST(IntervalTest, IsDueOnceAfterALateCheck) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(1000));
  ASSERT_TRUE(interval.IsDue(1180));

  EXPECT_FALSE(interval.IsDue(1180));
}

/* the interval counts from a late step, not from when it was due */
TEST(IntervalTest, IsNotDueAnIntervalAfterTheMissedStep) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(1000));
  ASSERT_TRUE(interval.IsDue(1180));

  EXPECT_FALSE(interval.IsDue(1229));
}

TEST(IntervalTest, IsDueAnIntervalAfterTheLateStep) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(1000));
  ASSERT_TRUE(interval.IsDue(1180));

  EXPECT_TRUE(interval.IsDue(1230));
}

TEST(IntervalTest, ZeroIsDueOnEveryCheck) {
  Interval interval{0};
  ASSERT_TRUE(interval.IsDue(1000));

  EXPECT_TRUE(interval.IsDue(1000));
}

/* a new interval counts from the last step, as the old one did */
TEST(IntervalTest, NewMillisecondsCountFromTheLastStep) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(1000));

  interval.SetMs(120);

  EXPECT_FALSE(interval.IsDue(1119));
  EXPECT_TRUE(interval.IsDue(1120));
}

/* an interval beyond 16 bits is kept whole: a uint16_t took 70000 as 4464,
 * with no more than a warning in a sketch, as Arduino compiles with
 * -Wno-error=narrowing */
TEST(IntervalTest, KeepsAnIntervalBeyondSixteenBits) {
  Interval interval{70000};
  ASSERT_TRUE(interval.IsDue(70000));

  EXPECT_EQ(interval.GetMs(), 70000u);
  EXPECT_FALSE(interval.IsDue(139999));
  EXPECT_TRUE(interval.IsDue(140000));
}

/* the interval is counted across the overflow of millis() after 49.7 days: a
 * step 50 ms after 0xFFFFFFF0 comes at 0x22, and none at 0xFFFFFFFF, where a
 * comparison with the sum, which wraps first, is due (H-2) */
TEST(IntervalTest, IsNotDueJustBeforeTheClockWraps) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(0xFFFFFFF0u));

  EXPECT_FALSE(interval.IsDue(0xFFFFFFFFu));
}

TEST(IntervalTest, IsNotDueBeforeTheIntervalHasPassedAcrossTheWrap) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(0xFFFFFFF0u));

  EXPECT_FALSE(interval.IsDue(0x21u));
}

TEST(IntervalTest, IsDueOnceTheIntervalHasPassedAcrossTheWrap) {
  Interval interval{50};
  ASSERT_TRUE(interval.IsDue(0xFFFFFFF0u));

  EXPECT_TRUE(interval.IsDue(0x22u));
}

}  // namespace
}  // namespace max_matrix
