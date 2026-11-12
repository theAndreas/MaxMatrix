// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The values of a switch of the configuration, and the end of a text.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIXSTANDARDTYPES_H_
#define MAXMATRIX_SRC_MAXMATRIXSTANDARDTYPES_H_

// NOLINTNEXTLINE(modernize-macro-to-enum): T-6, #if reads it
#define STD_ON 1u /*!< A switch of the configuration turned on. */
// NOLINTNEXTLINE(modernize-macro-to-enum): T-6, #if reads it
#define STD_OFF 0u /*!< A switch of the configuration turned off. */

namespace max_matrix {
namespace internal {

constexpr char kNullCharacter{'\0'}; /*!< The end of a text. */

}  // namespace internal
}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAXMATRIXSTANDARDTYPES_H_
