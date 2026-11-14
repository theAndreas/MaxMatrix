// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Elements that lie elsewhere, reached by index within their size, as
 *        `std::span`.
 */

#ifndef MAXMATRIX_SRC_UTILITY_SPAN_H_
#define MAXMATRIX_SRC_UTILITY_SPAN_H_

#include <stddef.h>

#include "MaxMatrixAttributes.h"
#include "Utility/array.h"

namespace max_matrix {

namespace internal {

/*!
 * \brief Returns `pointer`; in a `decltype`, whether a pointer converts to a
 *        `To*` without a cast, so that a constructor applies only then.
 *
 * \param[in] pointer The pointer.
 */
template <typename To>
constexpr To* ConvertPointer(To* pointer) {
  return pointer;
}

/*! \brief Names a type only if an `Element*` converts to a `T*`. */
template <typename T, typename Element>
using IfPointerConverts =
    decltype(ConvertPointer<T>(static_cast<Element*>(nullptr)));

}  // namespace internal

/*!
 * \brief A view of `size()` elements that another owns: a font, a bitmap, a
 *        text or a buffer.
 *
 * It is made of an array, which gives its size, so that a function that takes
 * one cannot read past the end:
 *
 * \code{.cpp}
 * matrix.SetText("Hi");  // the span takes the 3 chars of "Hi", its NUL counted
 * \endcode
 *
 * It owns nothing; what it views has to live as long as it is used.
 *
 * \tparam T The type of the elements, `const` for a view that cannot write.
 *
 * \stddiff Only a size known at run time, and only data(), size(), the index
 *          operator, first() and subspan(): nothing in the library needs more
 *          yet.
 */
/*
 * MaxMatrix's own, by CodingStyle.md M-3: avr-gcc brings no <span>, which is
 * C++20, and MaxMatrix depends on neither the ETL nor Utl (OIL 28, 41).
 */
template <typename T>
class span {
 public:
  using element_type = T;   /*!< The type of an element. */
  using size_type = size_t; /*!< The type of a size and an index. */

  /*! \brief Makes a span of no elements. */
  constexpr span() = default;

  /*!
   * \brief Makes a span of `size` elements from `data` on.
   *
   * \param[in] data The first element.
   * \param[in] size The number of elements.
   */
  constexpr span(T* data, size_type size) : data_{data}, size_{size} {}

  /*!
   * \brief Makes a span of a C array, as a text in quotes is.
   *
   * \tparam N The number of elements, which the array says itself.
   * \param[in] elements The array.
   */
  template <size_t N>
  constexpr span(T (&elements)[N])  // NOLINT(runtime/explicit): as std
      : data_{elements}, size_{N} {}

  /*!
   * \brief Makes a span of an array.
   *
   * \tparam Element The type of its elements, `T` or `T` without `const`.
   * \tparam N The number of elements, which the array says itself.
   * \param[in] elements The array.
   */
  template <typename Element, size_t N,
            typename = internal::IfPointerConverts<T, Element>>
  constexpr span(array<Element, N>& elements)  // NOLINT(runtime/explicit): std
      : data_{elements.data()}, size_{N} {}

  /*! \copydoc span(array<Element, N>&) */
  template <typename Element, size_t N,
            typename = internal::IfPointerConverts<T, Element const>>
  constexpr span(  // NOLINT(runtime/explicit): as std
      array<Element, N> const& elements)
      : data_{elements.data()}, size_{N} {}

  /*!
   * \brief Makes a span of what `other` views, as one of `uint8_t` into one of
   *        `uint8_t const`.
   *
   * \tparam Element The type of its elements, `T` without `const`.
   * \param[in] other The span.
   */
  template <typename Element,
            typename = internal::IfPointerConverts<T, Element>>
  constexpr span(span<Element> const& other)  // NOLINT(runtime/explicit): std
      : data_{other.data()}, size_{other.size()} {}

  /*! \brief Returns the address of the first element. */
  MAXMATRIX_NODISCARD constexpr T* data() const { return data_; }

  /*! \brief Returns the number of elements. */
  MAXMATRIX_NODISCARD constexpr size_type size() const { return size_; }

  /*!
   * \brief Returns the element at `index`, unchecked.
   *
   * \param[in] index The position, from 0; below size().
   */
  constexpr T& operator[](size_type index) const {
    // T-20: the one place that reaches an element through its address.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return data_[index];
  }

  /*!
   * \brief Returns a span of the first `count` elements.
   *
   * \param[in] count The number of elements, up to size().
   */
  MAXMATRIX_NODISCARD constexpr span first(size_type count) const {
    return span{data_, count};
  }

  /*!
   * \brief Returns a span of the elements from `offset` on.
   *
   * \param[in] offset The position of the first, up to size().
   */
  MAXMATRIX_NODISCARD constexpr span subspan(size_type offset) const {
    // T-20: the one place that moves an address.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    return span{data_ + offset, size_ - offset};
  }

 private:
  T* data_{nullptr};
  size_type size_{0};
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_UTILITY_SPAN_H_
