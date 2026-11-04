// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The font of the MaxMatrix library.
 *
 * A sprite per char, and the way from a char of a string to its sprite. Knows nothing of the
 * matrix.
 */

#ifndef MAXMATRIX_SRC_FONT_MAXMATRIXFONT_H_
#define MAXMATRIX_SRC_FONT_MAXMATRIXFONT_H_

#include "Arduino.h"
#include "MaxMatrixStandardTypes.h"
#include "MaxMatrixError.h"

/* MaxMatrixFont Parameter */
#define MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS                    7
#define MAXMATRIX_ASCII_TABLE_OFFSET                                (-32)
#define MAXMATRIX_ASCII_CHAR_MIN                                    32
#define MAXMATRIX_ASCII_CHAR_MAX                                    126
#define MAXMATRIX_ASCII_CHAR_MAX_WIDTH                              5

/*! \brief The index of a sprite in the font: the char's code minus 32 for ASCII. */
typedef byte MaxMatrixSpriteIndex;

/*!
 * \brief A sprite: its width, its height and up to five columns, bit 0 the top row.
 *
 * MaxMatrixSpriteByte names its bytes.
 */
typedef byte MaxMatrixSprite[MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS];

/*! \brief The bytes of a MaxMatrixSprite. */
enum MaxMatrixSpriteByte : uint8_t {
    kSpriteWidth,   /*!< The columns it takes, 1 to 5. */
    kSpriteHeight,  /*!< The rows it takes, 1 to 8. */
    kSpriteColumn1, /*!< Its leftmost column. */
    kSpriteColumn2, /*!< Its second column. */
    kSpriteColumn3, /*!< Its third column. */
    kSpriteColumn4, /*!< Its fourth column. */
    kSpriteColumn5  /*!< Its fifth column. */
};

/*!
 * \brief The font: a sprite per char, and the way from a char of a string to its sprite.
 *
 * It has no state of its own, so every function is static: there is one font, and an instance
 * would be an empty object. The sprites lie in flash.
 */
class MaxMatrixFont
{
  public:
    /*!
     * \brief Copies the sprite at `sprite_index` from the font.
     *
     * \param[in] sprite_index The index of the sprite.
     * \param[out] sprite The sprite: width, height and up to five columns; left as it was on an
     *                    error.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kOutOfRange if the font has no sprite at the
     *         index.
     */
    static MaxMatrixStatus GetSprite(MaxMatrixSpriteIndex sprite_index, MaxMatrixSprite* sprite);

    /*!
     * \brief Copies the sprite of a char from the font.
     *
     * What every function of MaxMatrixText that draws a char needs.
     *
     * \param[in] c The char, latin-1, as DecodeChar() gives it.
     * \param[out] sprite The sprite: width, height and up to five columns; left as it was on an
     *                    error.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kUnknownChar if the font has no sprite for
     *         the char.
     */
    static MaxMatrixStatus GetCharSprite(char c, MaxMatrixSprite* sprite);

    /*!
     * \brief Returns the index of the sprite of a char.
     *
     * \param[in] c The char, latin-1.
     * \return The index in the font, or MaxMatrixError::kUnknownChar if the font has no sprite for
     *         the char.
     */
    static MaxMatrixResult<MaxMatrixSpriteIndex> ConvertCharToSprite(char c);

    /*!
     * \brief Decodes the next char of a string.
     *
     * A UTF-8 sequence of a latin-1 char, such as an umlaut, becomes that one latin-1 char; every
     * other byte comes back as it is.
     *
     * \param[in] string The string at the char to decode; not at its terminating null character.
     * \param[out] c The decoded char.
     * \return The number of bytes the char took in `string`.
     */
    static byte DecodeChar(char const* string, char* c);
};

#endif
