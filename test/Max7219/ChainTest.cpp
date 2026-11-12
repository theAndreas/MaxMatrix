// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The transfer to the chain: every bit within a frame. */

#include "MaxMatrixTestSupport.h"

using max_matrix::ModuleType;

/* chip select was not an output in SPI mode, and the SPI transaction was never
 * ended */
TEST_F(MaxMatrixTest, TransfersAreFramed) {
  char const text[] = "Hello";

  EXPECT_EQ(mock::pin_mode[kChipSelectPin], OUTPUT);
  matrix_.SetText("Hi!");
  matrix_.SetModuleType(ModuleType::kIcstation);
  ShiftWholeText(text);

  EXPECT_GT(mock::frames, 0);
  EXPECT_EQ(mock::errors, 0);
  EXPECT_EQ(mock::pin_level[kChipSelectPin], HIGH);
  EXPECT_FALSE(mock::transaction_open);
  EXPECT_EQ(mock::UsesSpi(), MAXMATRIX_USE_SPI == STD_ON);
}
