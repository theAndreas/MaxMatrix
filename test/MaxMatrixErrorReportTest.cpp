// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The report of MAXMATRIX_DEV_ERROR_DETECT as a sketch without a report of its
 * own gets it: the library's default. A binary of its own, as the other tests
 * define the report themselves to record it. */

#include <gtest/gtest.h>

#include "MaxMatrix.h"

using max_matrix::Error;
using max_matrix::MatrixOf;

namespace {

constexpr byte kDataInPin{11};
constexpr byte kChipSelectPin{10};
constexpr byte kClockPin{13};
constexpr byte kNumberOfModules{4};

/* a call that fails prints its name and the error on Serial */
TEST(MaxMatrixErrorReportTest, DefaultReportPrintsOnSerial) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  MatrixOf<kNumberOfModules> matrix{kDataInPin, kChipSelectPin, kClockPin};
  mock::serial_output.clear();

  EXPECT_EQ(matrix.GetColumn(-1).GetError(), Error::kOutOfRange);

  EXPECT_EQ(mock::serial_output, "MaxMatrix GetColumn: out of range\r\n");
}

}  // namespace
