// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A reference that copies, as `std::reference_wrapper`, for gnu++11
 *        without the standard library.
 */

#ifndef MAXMATRIX_SRC_UTILITY_REFERENCE_WRAPPER_H_
#define MAXMATRIX_SRC_UTILITY_REFERENCE_WRAPPER_H_

#include "MaxMatrixAttributes.h"

namespace max_matrix {
namespace internal {

/*!
 * \brief A reference to a `T` that is never null and owns nothing.
 *
 * It is meant for a member that refers to an object another owns, as
 * CodingStyle.md FN-15 asks: a `T&` member takes the assignment from the class,
 * and a `T*` leaves open whether it may be null. Inside it is a pointer, 2
 * bytes on the Uno.
 *
 * It binds an lvalue only, never a temporary, which would be gone before the
 * first use. Assigning one rebinds it, as in the standard; to write through
 * it, use get().
 *
 * \tparam T The type referred to, `const` for a reference that cannot write.
 *
 * \stddiff No `operator()`, no constructor from a reference_wrapper of another
 *          type, and no `ref()` or `cref()`: nothing in the library needs them.
 *          The address is taken with `&`, not as `std::addressof()` takes it,
 *          as no class of the project overloads unary `&`.
 */
/*
 * MaxMatrix's own, by CodingStyle.md M-3: avr-gcc brings no <functional>, and
 * MaxMatrix depends on neither the ETL nor Utl, whose utl::reference_wrapper
 * this follows (OIL 28, 41).
 */
template <typename T>
class reference_wrapper {
 public:
  using type = T; /*!< The type referred to. */

  /*!
   * \brief Makes a reference to `object`.
   *
   * Implicit, as in the standard, so that a member is initialized from the
   * object itself.
   *
   * \param[in] object The object, an lvalue.
   */
  constexpr reference_wrapper(T& object)  // NOLINT(runtime/explicit): as std
      : pointer_{&object} {}

  /*!
   * \brief Deleted: a temporary would be gone before the first use.
   *
   * \note Deleted rather than left out, so that a reference to `T const` cannot
   *       bind a temporary through the `T const&` of the constructor above.
   */
  reference_wrapper(T&&) = delete;  // NOLINT(runtime/explicit): never called

  /*! \brief Returns the object referred to. */
  MAXMATRIX_NODISCARD constexpr T& get() const { return *pointer_; }

  /*!
   * \brief Returns the object referred to, so that it passes as a `T&`.
   *
   * Implicit, as in the standard, for a function that takes a `T&`.
   */
  constexpr operator T&() const { return *pointer_; }

 private:
  T* pointer_;
};

}  // namespace internal
}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_UTILITY_REFERENCE_WRAPPER_H_
