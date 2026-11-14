// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* internal::reference_wrapper: a reference that is never null, and copies and
 * assigns as a pointer does. */

#include "Utility/reference_wrapper.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <type_traits>

namespace max_matrix {
namespace internal {
namespace {

TEST(ReferenceWrapperTest, GivesTheObjectItIsMadeOf) {
  int16_t value{7};
  reference_wrapper<int16_t> const reference{value};

  EXPECT_EQ(&reference.get(), &value);
  reference.get() = 9;
  EXPECT_EQ(value, 9);
}

TEST(ReferenceWrapperTest, PassesAsTheObject) {
  int16_t value{7};
  reference_wrapper<int16_t> const reference{value};

  auto const address_of{[](int16_t& object) { return &object; }};

  EXPECT_EQ(address_of(reference), &value);
}

TEST(ReferenceWrapperTest, IsMadeWhenCompiling) {
  static int16_t const kValue{5};
  static constexpr reference_wrapper<int16_t const> kReference{kValue};

  static_assert(&kReference.get() == &kValue, "made when compiling");
  EXPECT_EQ(kReference.get(), 5);
}

/* the reason of CodingStyle.md FN-15: a class with one as a member stays
 * assignable, which a reference member would not */
TEST(ReferenceWrapperTest, RebindsWhenAssigned) {
  int16_t first{1};
  int16_t second{2};
  reference_wrapper<int16_t> reference{first};

  reference = reference_wrapper<int16_t>{second};

  EXPECT_EQ(&reference.get(), &second);
  EXPECT_EQ(first, 1);
}

TEST(ReferenceWrapperTest, BindsNoTemporary) {
  static_assert(
      !std::is_constructible_v<reference_wrapper<int16_t const>, int16_t&&>,
      "a temporary would be gone before the first use");
  static_assert(
      std::is_constructible_v<reference_wrapper<int16_t const>, int16_t&>,
      "an lvalue binds");
}

}  // namespace
}  // namespace internal
}  // namespace max_matrix
