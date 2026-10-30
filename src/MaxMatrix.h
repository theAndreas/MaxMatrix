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
/**     \file       MaxMatrix.h
 *      \brief      Main header file of MaxMatrix library
 *
 *      \details    Arduino library to drive 8x8 LED display Matrix modules with MAX7219
 *
 *                  The class MaxMatrix is the facade over the parts in the folders beside this file, one per subject:
 *                  Max7219/ the transfer to the chain, Matrix/ the buffer as wired and the coordinates as the viewer
 *                  sees them, Font/ the sprites of the chars, Text/ the text that stands or shifts. A function here
 *                  that only passes a call on is documented where the call goes.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_H_
#define _MAXMATRIX_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixStandardTypes.h"
#include "MaxMatrixConfig.h"
#include "Max7219/MaxMatrixChain.h"
#include "Matrix/MaxMatrixPanel.h"
#include "Font/MaxMatrixFont.h"
#include "Text/MaxMatrixText.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* Type which describes the internal state of the MaxMatrix */
typedef enum {
    MAXMATRIX_STATE_NONE,
    MAXMATRIX_STATE_INIT,
    MAXMATRIX_STATE_READY,
    MAXMATRIX_STATE_STRING_SHIFT,
    MAXMATRIX_STATE_CHAR_SHIFT
} MaxMatrixStateType;


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrix
 *****************************************************************************************************************************************************/
class MaxMatrix
{
  private:
    MaxMatrixPanel Panel;
    MaxMatrixText Text;
    MaxMatrixStateType State;

  public:
    MaxMatrix(byte, byte, byte);
    ~MaxMatrix();


    // get methods
    MaxMatrixStateType getState() { return State; }
    byte getDataInPin() { return Panel.getWired().getChain().getDataInPin(); }
    byte getChipSelectPin() { return Panel.getWired().getChain().getChipSelectPin(); }
    byte getClockPin() { return Panel.getWired().getChain().getClockPin(); }
    MaxMatrixModuleOrientationType getModuleOrientation() { return Panel.getModuleOrientation(); }
    MaxMatrixMatrixOrientationType getMatrixOrientation() { return Panel.getMatrixOrientation(); }
    byte getSpaceBetweenChars() { return Text.getSpaceBetweenChars(); }

    stdReturnType getColumn(byte Column, byte* Value) { return Panel.getColumn(Column, Value); }
    stdReturnType getColumn(byte Module, byte Column, byte* Value) { return Panel.getColumn(Module, Column, Value); }
    stdReturnType getRow(byte Row, rowType Value) { return Panel.getRow(Row, Value); }
    stdReturnType getRow(byte Module, byte Row, byte* Value) { return Panel.getRow(Module, Row, Value); }
    stdReturnType getDot(byte Column, byte Row, bool* Value) { return Panel.getDot(Column, Row, Value); }
    stdReturnType getSprite(spriteIndexType SpriteIndex, spriteType* Sprite) { return MaxMatrixFont::getSprite(SpriteIndex, Sprite); }


    // set methods
    void setDataInPin(byte sDataInPin) { Panel.getWired().getChain().setDataInPin(sDataInPin); }
    void setChipSelectPin(byte sChipSelectPin) { Panel.getWired().getChain().setChipSelectPin(sChipSelectPin); }
    void setClockPin(byte sClockPin) { Panel.getWired().getChain().setClockPin(sClockPin); }
    stdReturnType setIntensity(byte);
    stdReturnType setColumn(byte Column, byte Value) { return Panel.setColumn(Column, Value); }
    stdReturnType setColumn(byte Module, byte Column, byte Value) { return Panel.setColumn(Module, Column, Value); }
    stdReturnType setRow(byte Row, const rowType Value) { return Panel.setRow(Row, Value); }
    stdReturnType setRow(byte Module, byte Row, byte Value) { return Panel.setRow(Module, Row, Value); }
    stdReturnType setDot(byte Column, byte Row, bool Value) { return Panel.setDot(Column, Row, Value); }
    stdReturnType setChar(int X, int Y, char Char) { return Text.setChar(Panel, X, Y, Char); }
    stdReturnType setCharWithShift(char);
    stdReturnType setText(const char* String) { return Text.setText(Panel, String); }
    stdReturnType setTextWithShift(char*);
    stdReturnType setSpaceBetweenChars(byte sSpaceBetweenChars) { return Text.setSpaceBetweenChars(sSpaceBetweenChars); }
    void setSprite(int X, int Y, const spriteType Sprite) { Text.setSprite(Panel, X, Y, Sprite); }
    void setModuleOrientation(MaxMatrixModuleOrientationType sModuleOrientation) { Panel.setModuleOrientation(sModuleOrientation); }
    void setMatrixOrientation(MaxMatrixMatrixOrientationType sMatrixOrientation) { Panel.setMatrixOrientation(sMatrixOrientation); }


    // methods
    void init();
    void shiftTask();
    void clear() { Panel.getWired().clear(); }
    void RegisterWrite(byte RegisterAddress, byte RegisterValue) { Panel.getWired().getChain().writeRegisterAll(RegisterAddress, RegisterValue); }

    void shiftLeft(bool Rotate = false, bool FillWithZero = true) { Panel.shiftLeft(Rotate, FillWithZero); }
    void shiftRight(bool Rotate = false, bool FillWithZero = true) { Panel.shiftRight(Rotate, FillWithZero); }
    void shiftUp(bool Rotate = false) { Panel.shiftUp(Rotate); }
    void shiftDown(bool Rotate = false) { Panel.shiftDown(Rotate); }
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
