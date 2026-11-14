// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Bit: the bits of an unsigned integer, read and written by static functions,
 * the one place for bitwise operators (CodingStyle H-9). */

#include "Utility/Bit.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace max_matrix {
namespace {

using Bits = Bit<uint8_t>;
using WordBits = Bit<uint32_t>;

TEST(BitTest, MakesTheMaskOfAPosition) {
  static_assert(Bits::ToMask(0) == 0x01u, "made when compiling");

  EXPECT_EQ(Bits::ToMask(3), 0x08u);
  EXPECT_EQ(Bits::ToMask(7), 0x80u);
  EXPECT_EQ(WordBits::ToMask(31), 0x80000000u);
}

TEST(BitTest, CombinesTwoValues) {
  static_assert(Bits::Combine(0x0Fu, 0x30u) == 0x3Fu, "made when compiling");

  EXPECT_EQ(Bits::Combine(0x81u, 0x81u), 0x81u);
}

TEST(BitTest, ReadsABit) {
  static_assert(Bits::ReadBit(0x08u, 3), "made when compiling");

  EXPECT_FALSE(Bits::ReadBit(0xF7u, 3));
  EXPECT_TRUE(Bits::ReadBit(0x80u, 7));
  EXPECT_TRUE(WordBits::ReadBit(0x80000000u, 31));
}

TEST(BitTest, WritesABitEitherWay) {
  static_assert(Bits::WriteBit(0x00u, 2, true) == 0x04u, "made when compiling");

  EXPECT_EQ(Bits::WriteBit(0xFFu, 2, false), 0xFBu);
  EXPECT_EQ(Bits::WriteBit(0x04u, 2, true), 0x04u);
  EXPECT_EQ(Bits::WriteBit(0x00u, 2, false), 0x00u);
}

TEST(BitTest, ReadsAGroupInPlace) {
  static_assert(Bits::ReadBitGroup(0xA5u, 0x0Fu) == 0x05u,
                "made when compiling");

  EXPECT_EQ(Bits::ReadBitGroup(0xA5u, 0xF0u), 0xA0u);
}

TEST(BitTest, ReadsAGroupShiftedDownFromItsPosition) {
  // a field of three bits at bit 4, as a datasheet writes it
  EXPECT_EQ(Bits::ReadBitGroup(0xD5u, 0x07u, 4), 0x05u);
  EXPECT_EQ(WordBits::ReadBitGroup(0x12345678u, 0xFFu, 24), 0x12u);
}

TEST(BitTest, TogglesTheBitsOfAMask) {
  static_assert(Bits::ToggleBits(0xF0u, 0xFFu) == 0x0Fu, "made when compiling");

  EXPECT_EQ(Bits::ToggleBits(0xA5u, 0x00u), 0xA5u);
  EXPECT_EQ(Bits::ToggleBits(0xA5u, 0xA5u), 0x00u);
}

/* ~ of a uint8_t is a negative int; the result is cut back to the byte */
TEST(BitTest, InvertsEveryBit) {
  static_assert(Bits::Invert(0x0Fu) == 0xF0u, "made when compiling");

  EXPECT_EQ(Bits::Invert(0x00u), 0xFFu);
  EXPECT_EQ(WordBits::Invert(0u), 0xFFFFFFFFu);
}

/* a uint8_t shifted as an int would have a sign; the bits beyond the byte fall
 * out */
TEST(BitTest, ShiftsUpAndDown) {
  static_assert(Bits::ShiftUp(0x81u, 1) == 0x02u, "made when compiling");
  static_assert(Bits::ShiftDown(0x81u, 1) == 0x40u, "made when compiling");

  EXPECT_EQ(Bits::ShiftUp(0xFFu, 7), 0x80u);
  EXPECT_EQ(Bits::ShiftDown(0xFFu, 7), 0x01u);
  EXPECT_EQ(WordBits::ShiftUp(0xFFu, 24), 0xFF000000u);
}

TEST(BitTest, RotatesLeft) {
  static_assert(WordBits::RotateLeft(0x80000001u, 1) == 0x00000003u,
                "made when compiling");

  EXPECT_EQ(WordBits::RotateLeft(0x12345678u, 8), 0x34567812u);
  EXPECT_EQ(WordBits::RotateLeft(0x12345678u, 31), 0x091A2B3Cu);
  EXPECT_EQ(Bits::RotateLeft(0x81u, 4), 0x18u);
}

/* a register is volatile, a variable is not: both take the same functions, and
 * the bits outside the mask stay as they were */
TEST(BitTest, SetsAndClearsTheBitsOfATarget) {
  uint8_t volatile port{0x81u};
  uint32_t flags{0x0000FF00u};

  Bits::SetBits(port, 0x18u);
  EXPECT_EQ(port, 0x99u);
  Bits::ClearBits(port, 0x81u);
  EXPECT_EQ(port, 0x18u);

  WordBits::SetBits(flags, 0x00010001u);
  EXPECT_EQ(flags, 0x0001FF01u);
  WordBits::ClearBits(flags, 0x0000F000u);
  EXPECT_EQ(flags, 0x00010F01u);
}

}  // namespace
}  // namespace max_matrix
