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
#include "../StandardTypes.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* MaxMatrixFont Parameter */
#define MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS                    7
#define MAXMATRIX_ASCII_TABLE_OFFSET                                -32
#define MAXMATRIX_ASCII_CHAR_MIN                                    32
#define MAXMATRIX_ASCII_CHAR_MAX                                    126
#define MAXMATRIX_ASCII_CHAR_MAX_WIDTH                              5


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* sprite types */
typedef byte spriteIndexType;

/* sprite array type */
typedef byte spriteType[MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS];

/* types for sprite table columns */
enum {
    ASCII_TABLE_SPRITE_WIDTH,
    ASCII_TABLE_SPRITE_HEIGHT,
    ASCII_TABLE_SPRITE_COLUMN1,
    ASCII_TABLE_SPRITE_COLUMN2,
    ASCII_TABLE_SPRITE_COLUMN3,
    ASCII_TABLE_SPRITE_COLUMN4,
    ASCII_TABLE_SPRITE_COLUMN5
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixFont
 *****************************************************************************************************************************************************/
/* no state of its own, so every function is static: there is one font, and an instance would be an empty object */
class MaxMatrixFont
{
  public:
    static stdReturnType getSprite(spriteIndexType, spriteType*);
    static stdReturnType convertCharToSprite(char, spriteIndexType*);
    static byte decodeChar(const char*, char*);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
