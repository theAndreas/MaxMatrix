// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Byte: a byte of raw data, which takes only bitwise operators and is a
 * number only through ToInteger(). */

#include "Utility/Byte.h"

#include <gtest/gtest.h>

namespace max_matrix {
namespace {

TEST(ByteTest, HoldsTheValueItIsMadeOf) {
  static_assert(Byte{0xA5}.ToInteger() == 0xA5, "made when compiling");

  EXPECT_EQ(Byte{0x00}.ToInteger(), 0x00);
  EXPECT_EQ(Byte{0xFF}.ToInteger(), 0xFF);
}

TEST(ByteTest, IsZeroWhenMadeWithoutAValue) {
  static_assert(Byte{}.ToInteger() == 0, "made when compiling");

  Byte const value{};

  EXPECT_EQ(value.ToInteger(), 0);
}

TEST(ByteTest, ComparesByItsBits) {
  static_assert(Byte{0x3C} == Byte{0x3C}, "made when compiling");
  static_assert(Byte{0x3C} != Byte{0x3D}, "made when compiling");

  EXPECT_TRUE(Byte{0x3C} == Byte{0x3C});
  EXPECT_FALSE(Byte{0x3C} == Byte{0x3D});
  EXPECT_TRUE(Byte{0x3C} != Byte{0x3D});
  EXPECT_FALSE(Byte{0x3C} != Byte{0x3C});
}

TEST(ByteTest, CombinesBitsWithAndOrAndNot) {
  static_assert((Byte{0xF0} & Byte{0x3C}) == Byte{0x30}, "and");
  static_assert((Byte{0xF0} | Byte{0x0C}) == Byte{0xFC}, "or");
  static_assert(~Byte{0xF0} == Byte{0x0F}, "not");

  EXPECT_EQ(Byte{0xF0} & Byte{0x3C}, Byte{0x30});
  EXPECT_EQ(Byte{0xF0} | Byte{0x0C}, Byte{0xFC});
  EXPECT_EQ(~Byte{0xF0}, Byte{0x0F});
  EXPECT_EQ(~Byte{0x00}, Byte{0xFF});
}

TEST(ByteTest, ShiftsItsBitsOutAtTheEdges) {
  static_assert((Byte{0x81} << 1u) == Byte{0x02}, "left");
  static_assert((Byte{0x81} >> 1u) == Byte{0x40}, "right");

  EXPECT_EQ(Byte{0x81} << 1u, Byte{0x02});
  EXPECT_EQ(Byte{0x81} >> 1u, Byte{0x40});
  EXPECT_EQ(Byte{0x01} << 7u, Byte{0x80});
  EXPECT_EQ(Byte{0x80} >> 7u, Byte{0x01});
  EXPECT_EQ(Byte{0xFF} << 0u, Byte{0xFF});
}

TEST(ByteTest, ReadsEachBit) {
  static_assert(ReadBit(Byte{0x80}, 7), "made when compiling");

  EXPECT_TRUE(ReadBit(Byte{0x01}, 0));
  EXPECT_FALSE(ReadBit(Byte{0xFE}, 0));
  EXPECT_TRUE(ReadBit(Byte{0x80}, 7));
  EXPECT_FALSE(ReadBit(Byte{0x7F}, 7));
}

TEST(ByteTest, WritesOneBitAndKeepsTheOthers) {
  static_assert(WriteBit(Byte{0x00}, 7, true) == Byte{0x80}, "set");
  static_assert(WriteBit(Byte{0xFF}, 0, false) == Byte{0xFE}, "clear");

  EXPECT_EQ(WriteBit(Byte{0x00}, 0, true), Byte{0x01});
  EXPECT_EQ(WriteBit(Byte{0x00}, 7, true), Byte{0x80});
  EXPECT_EQ(WriteBit(Byte{0xFF}, 0, false), Byte{0xFE});
  EXPECT_EQ(WriteBit(Byte{0xFF}, 7, false), Byte{0x7F});
  EXPECT_EQ(WriteBit(Byte{0x5A}, 1, true), Byte{0x5A});
  EXPECT_EQ(WriteBit(Byte{0x5A}, 0, false), Byte{0x5A});
}

}  // namespace
}  // namespace max_matrix
