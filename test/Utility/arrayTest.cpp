// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* array: a C array that knows its size, and make_array(), which takes the size
 * from the values. */

#include "Utility/array.h"

#include <gtest/gtest.h>

#include <cstdint>

namespace max_matrix {
namespace {

TEST(ArrayTest, HoldsItsElements) {
  constexpr array<uint8_t, 3> kRows{{0x18, 0x3C, 0x7E}};

  static_assert(kRows.size() == 3, "made when compiling");
  static_assert(kRows[1] == 0x3C, "made when compiling");
  EXPECT_EQ(kRows[0], 0x18);
  EXPECT_EQ(kRows[2], 0x7E);
}

TEST(ArrayTest, WritesAnElementByIndex) {
  array<uint8_t, 2> rows{{0x00, 0x00}};

  rows[1] = 0x42;

  EXPECT_EQ(rows[0], 0x00);
  EXPECT_EQ(rows[1], 0x42);
}

/* the address of the elements, for a function that takes a C string */
TEST(ArrayTest, GivesTheAddressOfItsElements) {
  array<char, 3> text{{'H', 'i', '\0'}};
  array<char, 3> const& readable{text};

  EXPECT_STREQ(text.data(), "Hi");
  EXPECT_EQ(readable.data(), text.data());
}

/* the size of a font, which a sketch would otherwise have to count */
TEST(ArrayTest, MakeArrayTakesTheSizeFromTheValues) {
  constexpr auto kFont{make_array<uint8_t>('F', 1, 32, 255)};

  static_assert(kFont.size() == 4, "the size taken from the values");
  static_assert(kFont[0] == 'F', "a char as its code");
  static_assert(kFont[3] == 255, "the largest value that fits");
  EXPECT_EQ(kFont[2], 32);
}

/* a table of texts, whose elements are of the type already */
TEST(ArrayTest, MakeArrayTakesElementsOfTheTypeAsTheyAre) {
  static char const kFirst[] = "first";
  static char const kSecond[] = "second";

  constexpr auto kTexts{make_array<char const*>(kFirst, kSecond)};

  static_assert(kTexts.size() == 2, "the size taken from the values");
  EXPECT_EQ(kTexts[0], kFirst);
  EXPECT_EQ(kTexts[1], kSecond);
}

TEST(ArrayTest, MakeArrayTakesNegativeValuesThatFit) {
  constexpr auto kOffsets{make_array<int8_t>(-128, 0, 127)};

  static_assert(kOffsets[0] == -128, "the smallest value that fits");
  EXPECT_EQ(kOffsets[2], 127);
}

/* texts that may be written, into an array that only reads them: the range
 * check cast each pointer back to char*, which did not compile */
TEST(ArrayTest, MakeArrayTakesWhatConvertsWithoutNarrowing) {
  char first[] = "first";
  char second[] = "second";

  auto const texts{make_array<char const*>(first, second)};

  EXPECT_EQ(texts[0], first);
  EXPECT_EQ(texts[1], second);
}

/* an aggregate of the type, which the range check cannot compare */
TEST(ArrayTest, MakeArrayTakesAnAggregateOfTheType) {
  struct Point {
    int8_t x;
    int8_t y;
  };

  constexpr auto kCorners{make_array<Point>(Point{1, 2}, Point{3, 4})};

  static_assert(kCorners[1].y == 4, "the aggregate as it is");
  EXPECT_EQ(kCorners[0].x, 1);
}

}  // namespace
}  // namespace max_matrix
