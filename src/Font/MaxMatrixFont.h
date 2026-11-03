/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       MaxMatrixFont.h
 *      \brief      The font of the MaxMatrix library
 *
 *      \details    A sprite per char, and the way from a char of a string to its sprite. Knows nothing of the matrix.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_FONT_H_
#define _MAXMATRIX_FONT_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixStandardTypes.h"
#include "MaxMatrixError.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* MaxMatrixFont Parameter */
#define MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS                    7
#define MAXMATRIX_ASCII_TABLE_OFFSET                                (-32)
#define MAXMATRIX_ASCII_CHAR_MIN                                    32
#define MAXMATRIX_ASCII_CHAR_MAX                                    126
#define MAXMATRIX_ASCII_CHAR_MAX_WIDTH                              5


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/*! the index of a sprite in the font, the char's code minus 32 for ASCII */
typedef byte spriteIndexType;

/*! a sprite: its width, its height and up to five columns, bit 0 the top row, as the bytes are named below */
typedef byte spriteType[MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS];

/*! the bytes of a spriteType */
enum : uint8_t {
    ASCII_TABLE_SPRITE_WIDTH,               /*!< the columns it takes, 1 to 5 */
    ASCII_TABLE_SPRITE_HEIGHT,              /*!< the rows it takes, 1 to 8 */
    ASCII_TABLE_SPRITE_COLUMN1,             /*!< its leftmost column */
    ASCII_TABLE_SPRITE_COLUMN2,             /*!< its second column */
    ASCII_TABLE_SPRITE_COLUMN3,             /*!< its third column */
    ASCII_TABLE_SPRITE_COLUMN4,             /*!< its fourth column */
    ASCII_TABLE_SPRITE_COLUMN5              /*!< its fifth column */
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixFont
 *****************************************************************************************************************************************************/
/* no state of its own, so every function is static: there is one font, and an instance would be an empty object */
class MaxMatrixFont
{
  public:
    static MaxMatrixStatus getSprite(spriteIndexType, spriteType*);
    static MaxMatrixStatus getCharSprite(char, spriteType*);
    static MaxMatrixResult<spriteIndexType> convertCharToSprite(char);
    static byte decodeChar(char const*, char*);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
