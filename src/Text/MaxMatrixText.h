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
/**     \file       MaxMatrixText.h
 *      \brief      Text on the matrix
 *
 *      \details    Chars and strings in the font, drawn on the matrix as the viewer sees it, standing or shifting in.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_TEXT_H_
#define _MAXMATRIX_TEXT_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "Font/MaxMatrixFont.h"
#include "Matrix/MaxMatrixPanel.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* Type which describes the internal state of the MaxMatrix shift task */
typedef enum {
    MAXMATRIX_SPRITE_SHIFT_STATE_READY,
    MAXMATRIX_SPRITE_SHIFT_STATE_RUNNING
} MaxMatrixShiftStateType;


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixText
 *****************************************************************************************************************************************************/
/* Every function that draws is given the panel rather than the class keeping a reference to it. A reference costs two
 * bytes of RAM, and avr-gcc no longer sees through it that the panel is the one inside the global MaxMatrix of the
 * sketch: ScrollText was 86 bytes of flash larger with it. */
class MaxMatrixText
{
  private:
    spriteType SpriteBuffer;
    byte SpriteShiftCounter;
    byte SpaceBetweenChars;
    char* String;

  public:
    MaxMatrixText();

    // get methods
    byte getSpaceBetweenChars() { return SpaceBetweenChars; }

    // set methods
    stdReturnType setChar(MaxMatrixPanel&, int, int, char);
    stdReturnType setCharWithShift(char);
    stdReturnType setText(MaxMatrixPanel&, const char*);
    stdReturnType setTextWithShift(char*);
    stdReturnType setSpaceBetweenChars(byte);
    void setSprite(MaxMatrixPanel&, int, int, const spriteType);

    // methods
    bool charShiftTask(MaxMatrixPanel&);
    bool stringShiftTask(MaxMatrixPanel&);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
