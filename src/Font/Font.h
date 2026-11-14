// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The font of the MaxMatrix library.
 *
 * A sprite per char, and the way from a char of a string to its sprite. Knows
 * nothing of the matrix.
 */

#ifndef MAXMATRIX_SRC_FONT_FONT_H_
#define MAXMATRIX_SRC_FONT_FONT_H_

#include "Arduino.h"
#include "MaxMatrixAttributes.h"
#include "MaxMatrixError.h"
#include "MaxMatrixStandardTypes.h"
#include "Utility/array.h"

namespace max_matrix {

/*! The most columns a Sprite has. */
constexpr uint8_t kSpriteMaxWidth{5};
/*! The bytes of kDefaultFont. */
constexpr uint16_t kDefaultFontSize{614};

/*!
 * \brief A sprite of your own: its width, its height and up to five columns.
 *
 * \code{.cpp}
 * max_matrix::Sprite const kInvader{5, 5, {{0x16, 0x0D, 0x07, 0x0D, 0x16}}};
 * \endcode
 *
 * A char of a font is a Glyph instead.
 */
struct Sprite {
  uint8_t width;  /*!< The columns it takes, 1 to kSpriteMaxWidth. */
  uint8_t height; /*!< The rows it takes, 1 to 8. */
  /*! Its columns, the leftmost first, bit 0 the top row; those beyond `width`
   * are not drawn. */
  array<uint8_t, kSpriteMaxWidth> columns;
};

/*! \brief A char of a font, as it lies in flash: where its columns are, and how
 * many. */
struct Glyph {
  uint8_t const*
      columns; /*!< Its columns in flash, leftmost first, bit 0 the top row. */
  uint8_t width;  /*!< The columns it takes, 1 to 255. */
  uint8_t height; /*!< The rows it takes, the height of its font, 1 to 8. */
};

/*! \brief A char of a text, as Font::DecodeChar() reads it. */
struct DecodedChar {
  char c;         /*!< The char, latin-1. */
  uint8_t length; /*!< The bytes it took in the text, 1 or 2. */
};

/*!
 * \brief The font MaxMatrix starts with: ASCII from the space to the tilde, the
 *        German umlauts, the sharp s and the degree sign, 8 rows high and up to
 *        5 columns wide.
 *
 * In the format of Font, which Matrix::SetFont() takes. 614 bytes
 * of flash.
 */
// Declared here, the constant data in the .cpp: no static initializer to run.
// NOLINTNEXTLINE(bugprone-dynamic-static-initializers)
extern uint8_t const kDefaultFont[kDefaultFontSize] PROGMEM;

/*!
 * \brief The format of a font, and the way from a char of a string to its
 *        glyph.
 *
 * A font is the font format of the MD_MAX72XX library, so that its fonts and
 * its font builder serve here as well: an array of bytes in flash, `PROGMEM`,
 * that starts with `'F'`, the version, the code of the first char, that of the
 * last, and the height in rows, and then holds every char from the first to the
 * last as its width and that many columns, bit 0 the top row. A char of width 0
 * is not in the font. Version 1 gives each code in one byte; version 2, which
 * lets a font go past 255, in two, high byte first.
 *
 * The format holds no length of its own, so a font is checked against the
 * length of its array: every char from the first to the last has to fit into
 * it, and the last has to end it. A font that CheckFont() took cannot lead
 * GetGlyph() past its end.
 *
 * \code{.cpp}
 * uint8_t const kDigits[] PROGMEM = {
 *     'F', 1, '0', '1', 7, // version 1, the chars 0 to 1, 7 rows high
 *     3, B00111110, B01000001, B00111110, // 0
 *     2, B01000010, B01111111,            // 1
 * };
 * \endcode
 *
 * A char is found by walking from the first char to it, as MD_MAX72XX does: in
 * the default font 34 us for an A and 211 us for a u umlaut, 220 chars on, on
 * the Uno (measured in simavr), against an index that would take two bytes of
 * flash per char. A shifting text looks a char up once.
 *
 * It keeps no font of its own: every function is handed the font, and
 * internal::Text keeps which one is set.
 */
class Font {
 public:
  /*!
   * \brief Returns whether `font` is a font of `size` bytes.
   *
   * A font in RAM fails as well, almost always, as it is read from flash: the
   * bytes found there at its address would have to add up to its length
   * exactly.
   *
   * \param[in] font The font, in flash.
   * \param[in] size The length of its array in bytes.
   * \return Error::kOk, or Error::kNotAFont if `font` is null, does not start
   *         with `'F'`, version 1 or 2, a first char no later than the last and
   *         a height of 1 to 8, or its chars do not fill exactly `size` bytes.
   */
  MAXMATRIX_NODISCARD static Status CheckFont(uint8_t const* font, size_t size);

  /*!
   * \brief Finds the glyph of a char in a font.
   *
   * \param[in] font The font, in flash, one that CheckFont() took.
   * \param[in] c The char, latin-1, as DecodeChar() gives it.
   * \return The glyph, or Error::kUnknownChar if the font has no char for it.
   */
  MAXMATRIX_NODISCARD static Result<Glyph> GetGlyph(uint8_t const* font,
                                                    char c);

  /*!
   * \brief Decodes the next char of a string.
   *
   * A UTF-8 sequence of a latin-1 char, such as an umlaut, becomes that one
   * latin-1 char; every other byte comes back as it is.
   *
   * \param[in] text The text at the char to decode; not at its terminating null
   *                 character.
   * \return The char, and the number of bytes it took in `text`.
   */
  static DecodedChar DecodeChar(char const* text);
};

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_FONT_FONT_H_
