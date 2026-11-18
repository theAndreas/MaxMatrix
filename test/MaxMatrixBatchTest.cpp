// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Batch: auto update held off for a block, and put back as it was. */

#include "MaxMatrixBatch.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include "Arduino.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class MaxMatrixBatchTest : public MaxMatrixTest {};

TEST_F(MaxMatrixBatchTest, TurnsAutoUpdateOffForItsBlock) {
  bool auto_update_inside{true};

  {
    Batch const batch{matrix_};
    auto_update_inside = matrix_.GetAutoUpdate();
  }

  EXPECT_FALSE(auto_update_inside);
}

TEST_F(MaxMatrixBatchTest, PutsAutoUpdateBackAtTheEndOfItsBlock) {
  {
    Batch const batch{matrix_};
  }

  EXPECT_TRUE(matrix_.GetAutoUpdate());
}

TEST_F(MaxMatrixBatchTest, SendsNothingInsideItsBlock) {
  int32_t const frames_before{mock::frames};
  int32_t frames_inside{0};

  {
    Batch const batch{matrix_};
    matrix_.SetDot(Point{X{0}, Y{0}}, true);
    frames_inside = mock::frames;
  }

  EXPECT_EQ(frames_inside, frames_before);
}

/* the end of the block sends each changed column of a module once: columns 0
 * and 3 here */
TEST_F(MaxMatrixBatchTest, SendsAtTheEndOfItsBlock) {
  int32_t const frames_before{mock::frames};

  {
    Batch const batch{matrix_};
    matrix_.SetDot(Point{X{0}, Y{0}}, true);
    matrix_.SetDot(Point{X{3}, Y{2}}, true);
  }

  EXPECT_EQ(mock::frames - frames_before, 2);
}

/* the inner batch puts back the state it found, which the outer one had turned
 * off, so that nothing is sent before the outer block ends */
TEST_F(MaxMatrixBatchTest, NestedBatchesSendAtTheOutermost) {
  int32_t const frames_before{mock::frames};
  int32_t frames_after_inner{0};

  {
    Batch const outer{matrix_};
    {
      Batch const inner{matrix_};
      matrix_.SetDot(Point{X{1}, Y{1}}, true);
    }
    frames_after_inner = mock::frames;
    matrix_.SetDot(Point{X{2}, Y{2}}, true);
  }

  EXPECT_EQ(frames_after_inner, frames_before);
  EXPECT_EQ(mock::frames - frames_before, 2);
}

/* a batch inside a sketch that sends with Update() itself leaves it that way */
TEST_F(MaxMatrixBatchTest, KeepsAutoUpdateOffThatWasOff) {
  matrix_.SetAutoUpdate(false);

  {
    Batch const batch{matrix_};
  }

  EXPECT_FALSE(matrix_.GetAutoUpdate());
}

}  // namespace
}  // namespace max_matrix
