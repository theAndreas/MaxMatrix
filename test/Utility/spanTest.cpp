// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* span: elements that lie elsewhere, reached by index within their size. */

#include "Utility/span.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

#include "Utility/array.h"

namespace max_matrix {
namespace {

TEST(SpanTest, IsEmptyWhenMadeOfNothing) {
  constexpr span<uint8_t const> kNothing{};

  static_assert(kNothing.size() == 0, "made when compiling");
  EXPECT_EQ(kNothing.data(), nullptr);
}

TEST(SpanTest, ReachesTheElementsItIsMadeOf) {
  array<uint8_t, 3> rows{{0x18, 0x3C, 0x7E}};

  span<uint8_t> const view{rows.data(), 2};

  EXPECT_EQ(view.size(), 2u);
  EXPECT_EQ(view.data(), rows.data());
  EXPECT_EQ(view[1], 0x3C);
}

TEST(SpanTest, WritesThroughToTheElements) {
  array<uint8_t, 3> rows{{0x18, 0x3C, 0x7E}};
  span<uint8_t> const view{rows};

  view[0] = 0x42;

  EXPECT_EQ(rows[0], 0x42);
}

TEST(SpanTest, TakesTheSizeOfAnArray) {
  array<uint8_t, 3> rows{{1, 2, 3}};
  array<uint8_t, 4> const font{{4, 5, 6, 7}};

  span<uint8_t> const writable{rows};
  span<uint8_t const> const readable{font};

  EXPECT_EQ(writable.size(), 3u);
  EXPECT_EQ(readable.size(), 4u);
  EXPECT_EQ(readable[3], 7);
}

/* a text a sketch writes in quotes, its NUL counted */
TEST(SpanTest, TakesTheSizeOfACArray) {
  span<char const> const text{"Hi"};

  EXPECT_EQ(text.size(), 3u);
  EXPECT_EQ(text[1], 'i');
}

TEST(SpanTest, PassesAsOneThatCannotWrite) {
  array<uint8_t, 2> rows{{1, 2}};
  span<uint8_t> const writable{rows};

  span<uint8_t const> const readable{writable};

  EXPECT_EQ(readable.data(), writable.data());
  EXPECT_EQ(readable.size(), 2u);
}

/* only where a pointer to the elements converts, so that an overload for
 * spans of another type is no ambiguity */
TEST(SpanTest, IsMadeOnlyOfElementsItCanView) {
  static_assert(std::is_convertible_v<span<uint8_t>, span<uint8_t const>>,
                "one that writes passes as one that reads");
  static_assert(!std::is_convertible_v<span<uint8_t const>, span<uint8_t>>,
                "one that reads does not pass as one that writes");
  static_assert(!std::is_convertible_v<span<uint16_t>, span<uint8_t const>>,
                "nor one of another type");
  static_assert(
      !std::is_convertible_v<array<uint16_t, 2>&, span<uint8_t const>>,
      "nor an array of another type");
  static_assert(!std::is_convertible_v<array<uint8_t, 2> const&, span<uint8_t>>,
                "nor an array that cannot be written");
}

TEST(SpanTest, GivesTheElementsFromAnOffsetOn) {
  array<uint8_t, 4> rows{{1, 2, 3, 4}};
  span<uint8_t const> const all{rows};

  span<uint8_t const> const rest{all.subspan(1)};

  EXPECT_EQ(rest.size(), 3u);
  EXPECT_EQ(rest[0], 2);
}

TEST(SpanTest, GivesItsFirstElements) {
  array<uint8_t, 4> rows{{1, 2, 3, 4}};
  span<uint8_t const> const all{rows};

  span<uint8_t const> const first{all.first(2)};

  EXPECT_EQ(first.size(), 2u);
  EXPECT_EQ(first.data(), all.data());
}

constexpr array<uint8_t, 3> kThree{{1, 2, 3}};

TEST(SpanTest, ReachesItsElementsWhenCompiling) {
  static_assert(span<uint8_t const>{kThree}.size() == 3, "its size");
  static_assert(span<uint8_t const>{kThree}.data() == kThree.data(),
                "its elements");
  static_assert(span<uint8_t const>{kThree}[2] == 3, "an element");
  static_assert(span<uint8_t const>{kThree}.first(2).size() == 2,
                "its first elements");
  static_assert(span<uint8_t const>{kThree}.subspan(1)[0] == 2,
                "from an offset");

  EXPECT_EQ(span<uint8_t const>{kThree}.size(), 3u);
}

TEST(SpanTest, FirstOfNoElementIsEmpty) {
  span<uint8_t const> const all{kThree};

  span<uint8_t const> const none{all.first(0)};

  EXPECT_EQ(none.size(), 0u);
}

TEST(SpanTest, FirstOfAllElementsIsTheWhole) {
  span<uint8_t const> const all{kThree};

  span<uint8_t const> const whole{all.first(3)};

  EXPECT_EQ(whole.size(), 3u);
  EXPECT_EQ(whole.data(), kThree.data());
}

TEST(SpanTest, SubspanFromTheStartIsTheWhole) {
  span<uint8_t const> const all{kThree};

  span<uint8_t const> const whole{all.subspan(0)};

  EXPECT_EQ(whole.size(), 3u);
  EXPECT_EQ(whole.data(), kThree.data());
}

TEST(SpanTest, SubspanFromTheEndIsEmpty) {
  span<uint8_t const> const all{kThree};

  span<uint8_t const> const none{all.subspan(3)};

  EXPECT_EQ(none.size(), 0u);
}

}  // namespace
}  // namespace max_matrix
