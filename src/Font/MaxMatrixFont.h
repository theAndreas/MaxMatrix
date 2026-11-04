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

/*! the index of a sprite in the font, the char's code minus 32 for ASCII */
typedef byte MaxMatrixSpriteIndex;

/*! a sprite: its width, its height and up to five columns, bit 0 the top row, as the bytes are named below */
typedef byte MaxMatrixSprite[MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS];

/*! the bytes of a MaxMatrixSprite */
enum MaxMatrixSpriteByte : uint8_t {
    kSpriteWidth,               /*!< the columns it takes, 1 to 5 */
    kSpriteHeight,              /*!< the rows it takes, 1 to 8 */
    kSpriteColumn1,             /*!< its leftmost column */
    kSpriteColumn2,             /*!< its second column */
    kSpriteColumn3,             /*!< its third column */
    kSpriteColumn4,             /*!< its fourth column */
    kSpriteColumn5              /*!< its fifth column */
};

/* no state of its own, so every function is static: there is one font, and an instance would be an empty object */
class MaxMatrixFont
{
  public:
    static MaxMatrixStatus GetSprite(MaxMatrixSpriteIndex, MaxMatrixSprite*);
    static MaxMatrixStatus GetCharSprite(char, MaxMatrixSprite*);
    static MaxMatrixResult<MaxMatrixSpriteIndex> ConvertCharToSprite(char);
    static byte DecodeChar(char const*, char*);
};

#endif
