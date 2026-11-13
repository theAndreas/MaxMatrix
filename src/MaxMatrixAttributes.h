// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The attributes of a later standard, as macros, where the compiler has
 *        them.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIXATTRIBUTES_H_
#define MAXMATRIX_SRC_MAXMATRIXATTRIBUTES_H_

/*!
 * \def MAXMATRIX_NODISCARD
 * \brief Marks a function whose result must not be dropped: the compiler warns
 *        where it is.
 *
 * `[[nodiscard]]` where the compiler has it, GCC's `warn_unused_result`
 * otherwise. The library marks a getter's result, whose value would be lost,
 * and the results the parts below `Matrix` pass on; not a setter's `Status`,
 * which a sketch may drop when it has no one to tell.
 */
/*
 * Chosen by what the compiler has rather than by __cplusplus, against FN-10:
 * avr-gcc 7.3, GCC and clang have [[nodiscard]] in gnu++11 already, and
 * warn_unused_result made the examples 10 to 30 bytes larger on the Uno, while
 * [[nodiscard]] costs nothing. clang warns of it before C++17 only under
 * -Wpedantic. Two #if, as a preprocessor without __has_cpp_attribute cannot
 * read it in a condition.
 */
#ifdef __has_cpp_attribute
#if __has_cpp_attribute(nodiscard)
#define MAXMATRIX_NODISCARD [[nodiscard]]
#endif
#endif
#ifndef MAXMATRIX_NODISCARD
#define MAXMATRIX_NODISCARD __attribute__((warn_unused_result))
#endif

namespace max_matrix {

/*!
 * \brief Drops a result marked with #MAXMATRIX_NODISCARD on purpose, with the
 *        reason in a comment beside the call.
 *
 * Rather than a cast to `void`: where the macro is `warn_unused_result`, GCC
 * warns through a cast, clang does not, so a cast that passes on the host
 * could fail the build for the board.
 *
 * \tparam Value The type of the result.
 */
template <typename Value>
void Discard(Value const& /* result */) {}

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAXMATRIXATTRIBUTES_H_
