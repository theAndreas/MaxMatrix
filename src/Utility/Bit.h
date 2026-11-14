// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Single bits and groups of bits of an unsigned integer: the one place
 *        where bitwise operators stand (docs/CodingStyle.md H-9).
 */

#ifndef MAXMATRIX_SRC_UTILITY_BIT_H_
#define MAXMATRIX_SRC_UTILITY_BIT_H_

#include <stdint.h>

namespace max_matrix {

/*!
 * \brief The bits of an unsigned integer, read and written by static functions,
 *        as a register, a byte of a frame or a word of a hash.
 *
 * \code{.cpp}
 * bool const lit{max_matrix::Bit<uint8_t>::ReadBit(column, 3)};
 * max_matrix::Bit<uint8_t>::SetBits(*port, mask);
 * \endcode
 *
 * A position counts from 0, the least significant bit. A group of bits is
 * given by its mask as it stands in the value; ReadBitGroup() with a position
 * is for a mask written down unshifted, as a datasheet gives a field.
 *
 * A value is shifted as an `unsigned int` where `Integer` is narrower: a
 * `uint8_t` would be promoted to an `int`, which has a sign and, on the AVR,
 * 16 bits.
 *
 * \tparam Integer The unsigned integer, as `uint8_t` or `uint32_t`; not `bool`.
 * \pre Each position and number of places lies below the width of `Integer`; a
 *      shift by as much is undefined.
 */
/*
 * MaxMatrix's own, by CodingStyle.md M-3: it depends on neither the ETL nor
 * Utl, whose utl::Bit this follows with only what is used here, the static
 * functions; no value is kept bit by bit.
 */
template <typename Integer>
class Bit {
  // -1 converted is the largest value of an unsigned type only: in a signed
  // one it stays -1, and in a bool it is true, as 1 is.
  static_assert(static_cast<Integer>(-1) > static_cast<Integer>(1),
                "Bit: Integer is an unsigned integer other than bool");

 public:
  /*! \brief Deleted: the functions are static, no Bit is made. */
  Bit() = delete;

  /*!
   * \brief Returns the mask of the bit at `position`.
   *
   * \param[in] position The position of the bit.
   * \return An integer with that bit set and every other bit cleared.
   */
  static constexpr Integer ToMask(uint8_t position) {
    return ShiftUp(Integer{1u}, position);
  }

  /*!
   * \brief Returns the bits set in either of two values: the mask of a group
   *        from the masks of its parts, or a value put together from its parts.
   *
   * What a `|` of named masks does in a `constexpr`, the one place it may
   * stand outside this class (docs/CodingStyle.md H-9); this anywhere.
   *
   * \param[in] first The one value.
   * \param[in] second The other.
   * \return Every bit that is set in one of them.
   */
  static constexpr Integer Combine(Integer first, Integer second) {
    return static_cast<Integer>(static_cast<ShiftType>(first) | second);
  }

  /*!
   * \brief Returns whether the bit at `position` of `value` is set.
   *
   * \param[in] value The integer to read.
   * \param[in] position The position of the bit.
   */
  // The value shifted down to the bit rather than a mask shifted up to it, as
  // utl::Bit does: 26 bytes less in Basics on the Uno.
  static constexpr bool ReadBit(Integer value, uint8_t position) {
    return ReadBitGroup(ShiftDown(value, position), Integer{1u}) != Integer{0u};
  }

  /*!
   * \brief Returns `value` with the bit at `position` set to `bit`.
   *
   * \param[in] value The integer to change.
   * \param[in] position The position of the bit.
   * \param[in] bit True to set the bit, false to clear it.
   * \return The changed integer.
   */
  static constexpr Integer WriteBit(Integer value, uint8_t position, bool bit) {
    return bit ? Combine(value, ToMask(position))
               : ReadBitGroup(value, Invert(ToMask(position)));
  }

  /*!
   * \brief Returns the bits of `value` that `mask` selects, where they stand.
   *
   * \param[in] value The integer to read.
   * \param[in] mask The group as it stands in the value.
   * \return The bits of the group in place, every other bit cleared.
   */
  static constexpr Integer ReadBitGroup(Integer value, Integer mask) {
    return static_cast<Integer>(static_cast<ShiftType>(value) & mask);
  }

  /*!
   * \brief Returns the group of bits of `value` that starts at `position`,
   *        shifted down to bit 0.
   *
   * \param[in] value The integer to read.
   * \param[in] mask The group written down unshifted, from bit 0 on.
   * \param[in] position The position of the group's lowest bit in `value`.
   * \return The bits of the group from bit 0 on, every other bit cleared.
   */
  static constexpr Integer ReadBitGroup(Integer value, Integer mask,
                                        uint8_t position) {
    return ReadBitGroup(ShiftDown(value, position), mask);
  }

  /*!
   * \brief Returns `value` with the bits of `mask` inverted, the others as they
   *        were.
   *
   * \param[in] value The integer to change.
   * \param[in] mask The bits to invert.
   * \return The changed integer.
   */
  static constexpr Integer ToggleBits(Integer value, Integer mask) {
    return static_cast<Integer>(static_cast<ShiftType>(value) ^ mask);
  }

  /*!
   * \brief Returns `value` with every bit inverted.
   *
   * \param[in] value The integer.
   * \return The bits that are not set in `value`.
   */
  static constexpr Integer Invert(Integer value) {
    // ~ of an Integer narrower than int would be a negative int.
    return static_cast<Integer>(~static_cast<ShiftType>(value));
  }

  /*!
   * \brief Returns `value` moved `places` up; the bits beyond the top fall out.
   *
   * \param[in] value The integer.
   * \param[in] places The number of places.
   */
  static constexpr Integer ShiftUp(Integer value, uint8_t places) {
    return static_cast<Integer>(static_cast<ShiftType>(value) << places);
  }

  /*!
   * \brief Returns `value` moved `places` down; the bits below bit 0 fall out.
   *
   * \param[in] value The integer.
   * \param[in] places The number of places.
   */
  static constexpr Integer ShiftDown(Integer value, uint8_t places) {
    return static_cast<Integer>(static_cast<ShiftType>(value) >> places);
  }

  /*!
   * \brief Returns `value` moved `places` up, the bits beyond the top coming in
   *        again at the bottom.
   *
   * \param[in] value The integer.
   * \param[in] places The number of places, 1 to the width less 1.
   */
  static constexpr Integer RotateLeft(Integer value, uint8_t places) {
    return Combine(ShiftUp(value, places),
                   ShiftDown(value, static_cast<uint8_t>(kWidth - places)));
  }

  /*!
   * \brief Sets the bits of `mask` in `target`, the other bits left as they
   *        were.
   *
   * \tparam Target The type of the target: `uint8_t volatile` for a port, an
   *                `Integer` for a variable, or what stands for a port with
   *                `|=` and `&=` of its own, as the mock of the tests does, so
   *                that every bit reaches its pin there.
   * \param[in,out] target The register or variable to change.
   * \param[in] mask The bits to set.
   * \warning It reads the target, changes it and writes it back, as `|=` on a
   *          register does, and not atomically: a target that an interrupt
   *          writes as well needs the caller's guard. Not for a register of
   *          flags, where writing back a 1 that was read clears a flag, nor for
   *          `PINB` and its like, where a 1 written toggles the pin.
   */
  template <typename Target>
  static void SetBits(Target& target, Integer mask) {
    target |= mask;
  }

  /*!
   * \brief Clears the bits of `mask` in `target`, the other bits left as they
   *        were.
   *
   * \tparam Target The type of the target, as for SetBits().
   * \param[in,out] target The register or variable to change.
   * \param[in] mask The bits to clear.
   * \warning It reads the target, changes it and writes it back, and not
   *          atomically; see SetBits().
   */
  template <typename Target>
  static void ClearBits(Target& target, Integer mask) {
    target &= Invert(mask);
  }

 private:
  // What Integer is shifted in: Integer itself when it is at least an
  // unsigned int, unsigned int otherwise.
  using ShiftType = decltype(Integer{} | 0u);

  // Not CHAR_BIT: clang-tidy, with the board's flags, finds the host's
  // <limits.h>, which does not parse there.
  static constexpr uint8_t kBitsPerByte{8u};
  static constexpr uint8_t kWidth{
      static_cast<uint8_t>(sizeof(Integer) * kBitsPerByte)};
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_UTILITY_BIT_H_
