// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A C array that knows its size, as `std::array`, and make_array(),
 *        which takes the size from the values.
 */

#ifndef MAXMATRIX_SRC_UTILITY_ARRAY_H_
#define MAXMATRIX_SRC_UTILITY_ARRAY_H_

#include <stddef.h>

#include "MaxMatrixAttributes.h"

namespace max_matrix {

/*!
 * \brief A C array of `Size` elements that knows its size.
 *
 * \code{.cpp}
 * max_matrix::array<uint8_t, 3> const rows{{0x18, 0x3C, 0x18}};
 * \endcode
 *
 * A struct with a public member, as `std::array` is: so it is an aggregate, is
 * made as a C array is, and can lie in `PROGMEM` as one. A table in `PROGMEM`
 * is made with make_array(), which counts its values.
 *
 * \tparam Element The type of the elements.
 * \tparam Size The number of elements, at least 1.
 *
 * \stddiff Only size(), the index operator and data(): nothing in the library
 *          needs more yet. No array of no elements.
 */
/*
 * MaxMatrix's own, by CodingStyle.md M-3: avr-gcc brings no <array>, and
 * MaxMatrix depends on neither the ETL nor Utl, whose utl::array this follows
 * (OIL 28, 41).
 */
template <typename Element, size_t Size>
struct array {
  // A C array of no elements is ill-formed.
  static_assert(Size > 0, "max_matrix::array needs at least one element");

  using value_type = Element; /*!< The type of an element. */
  using size_type = size_t;   /*!< The type of a size and an index. */

  /*!
   * \brief The elements.
   *
   * Public, so that the array is an aggregate; use the index operator rather
   * than this member.
   */
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, what it wraps
  Element elements[Size];

  /*! \brief Returns the number of elements, `Size`. */
  MAXMATRIX_NODISCARD constexpr size_type size() const { return Size; }

  /*!
   * \brief Returns the element at `index`, unchecked.
   *
   * \param[in] index The position, from 0; below `Size`.
   */
  Element& operator[](size_type index) & { return elements[index]; }

  /*! \copydoc operator[](size_type) & */
  constexpr Element const& operator[](size_type index) const& {
    return elements[index];
  }

  /*! \brief Deleted, as a reference into a temporary would dangle (C-16). */
  Element const& operator[](size_type index) const&& = delete;

  /*! \brief Returns the address of the first element, as a C array gives it. */
  MAXMATRIX_NODISCARD Element* data() & { return elements; }

  /*! \copydoc data() & */
  MAXMATRIX_NODISCARD constexpr Element const* data() const& {
    return elements;
  }

  /*! \brief Deleted, as a reference into a temporary would dangle (C-16). */
  MAXMATRIX_NODISCARD Element const* data() const&& = delete;
};

namespace internal {

/*!
 * \brief Declared only, so that make_array() stops the build with its name.
 *
 * A value that does not fit calls it, and a call of a function that is not
 * `constexpr` ends the evaluation when compiling with an error.
 */
template <typename Element>
Element ValueOutOfRange();

/*!
 * \brief Returns whether `value` is below 0, without comparing a type that has
 *        no sign with 0, which `-Wtype-limits` reports.
 *
 * \param[in] value The value.
 */
template <typename Value>
constexpr bool IsNegative(Value value) {
  return (value != Value{0}) && !(value > Value{0});
}

/*!
 * \brief Declared only: in a `decltype`, a value of type `Declared`, where
 *        nothing is evaluated, as `std::declval()`, which avr-gcc lacks.
 */
template <typename Declared>
Declared&& DeclVal();

/*!
 * \brief Converts a value of make_array() that would narrow, a number, into an
 *        element.
 *
 * \tparam Element The type of the elements.
 * \tparam Value The type of the value, as written.
 * \tparam Braced `Element`, if an `Element` is made of a `Value` in braces,
 *                which refuse narrowing; then the specialization below applies.
 */
template <typename Element, typename Value, typename Braced = Element>
struct ArrayElement {
  /*!
   * \brief Returns whether `value` is the same as an element as it was.
   *
   * \param[in] value The value.
   */
  static constexpr bool Fits(Value value) {
    return (static_cast<Value>(static_cast<Element>(value)) == value) &&
           (IsNegative(value) == IsNegative(static_cast<Element>(value)));
  }

  /*!
   * \brief Returns `value` as an element, or stops the build if it does not
   *        fit.
   *
   * \param[in] value The value.
   */
  static constexpr Element Of(Value value) {
    return Fits(value) ? static_cast<Element>(value)
                       : ValueOutOfRange<Element>();
  }
};

/*!
 * \brief A value of make_array() that becomes an element without narrowing: one
 *        of the type, a smaller number, or a `char*` for a `char const*`, which
 *        the range check could not cast back.
 */
template <typename Element, typename Value>
struct ArrayElement<Element, Value, decltype(Element{DeclVal<Value>()})> {
  /*!
   * \brief Returns `value` as an element.
   *
   * \param[in] value The value.
   */
  static constexpr Element Of(Value const& value) { return value; }
};

}  // namespace internal

/*!
 * \brief Returns an array of `Element` of its values, with the size taken from
 *        them.
 *
 * What C++17 deduces from `std::array a{...}`, and gnu++11 cannot. A font or a
 * bitmap in `PROGMEM` is written so, its values as MD_MAX72XX writes them:
 *
 * \code{.cpp}
 * constexpr auto kHeart PROGMEM{
 *     max_matrix::make_array<uint8_t>(0x0C, 0x1E, 0x3C, 0x1E, 0x0C)};
 * \endcode
 *
 * A value that does not fit `Element`, as 256 or -1 for a `uint8_t`, stops the
 * build with an error that names `ValueOutOfRange`, also in a sketch, which
 * Arduino compiles with `-Wno-error=narrowing`, so that braces would only warn.
 *
 * \note For values known when compiling, as a table in `PROGMEM` has to be
 *       made: made at start-up, it would be written to flash, which avr-gcc
 *       refuses.
 *
 * \tparam Element The type of the elements.
 * \param[in] values The elements, each what becomes an `Element` without
 *                   narrowing, as a `char*` for a `char const*`, or a number
 *                   that fits one.
 *
 * \stddiff `std::experimental::make_array<Element>()` refuses a number of
 *          another type even where it fits, so that every value of a font would
 *          have to be written as a `uint8_t`. `Element` cannot be left out.
 */
template <typename Element, typename... Values>
constexpr array<Element, sizeof...(Values)> make_array(Values... values) {
  return {{internal::ArrayElement<Element, Values>::Of(values)...}};
}

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_UTILITY_ARRAY_H_
