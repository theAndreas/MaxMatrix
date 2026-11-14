// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A byte of raw data, as a column of dots, which is no number.
 */

#ifndef MAXMATRIX_SRC_UTILITY_BYTE_H_
#define MAXMATRIX_SRC_UTILITY_BYTE_H_

#include <stdint.h>

namespace max_matrix {

/*!
 * \brief A byte of raw data: eight dots of a column or a row, or the value of a
 *        register of the MAX7219.
 *
 * It takes only the bitwise operators, and is a number only through
 * ToInteger(), so that a column cannot be passed where its value is meant, nor
 * the other way round. A sketch makes one from a number in braces:
 *
 * \code{.cpp}
 * matrix.SetColumn(3, max_matrix::Byte{0x3C});
 * \endcode
 *
 * As `std::byte`, which it stands in for until every target has C++17, a byte
 * made without braces holds any bits, and one made with empty braces none:
 * `Byte column{};`. It is one `uint8_t` and costs no RAM of its own; the
 * examples are 34 to 50 bytes of flash smaller with it than with `uint8_t` on
 * the Uno (OIL 41).
 */
/*
 * A class rather than the enum class of CodingStyle.md T-17: before C++17 an
 * enum class cannot be made from a number in braces, Byte{0x3C}, and a sketch
 * would have to write a static_cast (OIL 41). Named Byte against the names of
 * CodingStyle.md, which give a type that mimics the standard's its name: a
 * max_matrix::byte would meet Arduino's byte in a sketch that is using
 * namespace max_matrix.
 */
class Byte {
 public:
  /*!
   * \brief Makes a byte whose bits are not set to anything, as `std::byte`;
   *        `Byte{}` has none set.
   */
  // cppcheck-suppress uninitMemberVar ; T-13, the reason is at value_
  Byte() = default;

  /*!
   * \brief Makes a byte of the bits of `value`.
   *
   * \param[in] value The bits, bit 0 the first dot.
   */
  constexpr explicit Byte(uint8_t value) : value_{value} {}

  /*!
   * \brief Returns the bits as a number.
   *
   * \param[in] value The byte.
   */
  // C-12: std::byte's to_integer() is a free function as well, so that a
  // switch to std::byte changes only its name.
  friend constexpr uint8_t ToInteger(Byte value) { return value.value_; }

 private:
  /*
   * T-13: not initialized here, as std::byte's is not. Initialized, it made the
   * constructor not trivial: the buffer of every MatrixOf was cleared by a loop
   * at start, and memset() could not empty it, which cost the examples up to 66
   * bytes of flash on the Uno. Byte{} and Byte{value} set it.
   */
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): T-13, see above
  uint8_t value_;
};

// A friend defined in its class is found only by its argument; declared here
// as well, so that max_matrix::ToInteger() finds it.
constexpr uint8_t ToInteger(Byte value);

/*!
 * \brief Returns whether two bytes have the same bits.
 *
 * \param[in] left The one byte.
 * \param[in] right The other.
 */
constexpr bool operator==(Byte left, Byte right) {
  return ToInteger(left) == ToInteger(right);
}

/*!
 * \brief Returns whether two bytes differ in a bit.
 *
 * \param[in] left The one byte.
 * \param[in] right The other.
 */
constexpr bool operator!=(Byte left, Byte right) { return !(left == right); }

/*!
 * \brief Returns the bits set in both bytes.
 *
 * \param[in] left The one byte.
 * \param[in] right The other.
 */
constexpr Byte operator&(Byte left, Byte right) {
  return Byte{static_cast<uint8_t>(ToInteger(left) & ToInteger(right))};
}

/*!
 * \brief Returns the bits set in either byte.
 *
 * \param[in] left The one byte.
 * \param[in] right The other.
 */
constexpr Byte operator|(Byte left, Byte right) {
  return Byte{static_cast<uint8_t>(ToInteger(left) | ToInteger(right))};
}

/*!
 * \brief Returns the bits that are not set in `value`.
 *
 * \param[in] value The byte.
 */
constexpr Byte operator~(Byte value) {
  return Byte{static_cast<uint8_t>(~ToInteger(value))};
}

/*!
 * \brief Returns the bits moved `shift` places up; those beyond bit 7 fall out.
 *
 * \param[in] value The byte.
 * \param[in] shift The places, less than 8.
 */
constexpr Byte operator<<(Byte value, uint8_t shift) {
  return Byte{static_cast<uint8_t>(ToInteger(value) << shift)};
}

/*!
 * \brief Returns the bits moved `shift` places down; those below bit 0 fall
 *        out.
 *
 * \param[in] value The byte.
 * \param[in] shift The places, less than 8.
 */
constexpr Byte operator>>(Byte value, uint8_t shift) {
  return Byte{static_cast<uint8_t>(ToInteger(value) >> shift)};
}

/*!
 * \brief Returns whether a bit is set, as the core's `bitRead()` for a number.
 *
 * \param[in] value The byte.
 * \param[in] bit The bit, 0 to 7.
 */
// The value shifted down to the bit rather than a mask shifted up to it: 26
// bytes less in Basics on the Uno. Shifted as an unsigned int, as a uint8_t
// would be promoted to an int, which has a sign.
constexpr bool ReadBit(Byte value, uint8_t bit) {
  using ShiftType = decltype(uint8_t{} | 0U);
  return ((static_cast<ShiftType>(ToInteger(value)) >> bit) & 1U) != 0U;
}

/*!
 * \brief Returns `value` with one bit set or cleared, as the core's
 *        `bitWrite()` for a number.
 *
 * \param[in] value The byte.
 * \param[in] bit The bit, 0 to 7.
 * \param[in] set True to set the bit, false to clear it.
 */
constexpr Byte WriteBit(Byte value, uint8_t bit, bool set) {
  return set ? (value | (Byte{1} << bit)) : (value & ~(Byte{1} << bit));
}

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_UTILITY_BYTE_H_
