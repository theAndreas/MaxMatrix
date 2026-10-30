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
 *                  sees them, Font/ the sprites of the chars, Text/ the text that stands or shifts, Command/ the lines a
 *                  control page sends over Serial, built on this class. A function here that only passes a call on is
 *                  documented where the call goes.
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
#include "Command/MaxMatrixCommand.h"


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
/* Every function that draws changes the buffer, and with AutoUpdate, the default, sends what changed before it returns:
 * one frame per changed column of a module, eight at most, however much the call drew. A sketch that builds one picture
 * out of many calls turns AutoUpdate off and calls update() when the picture is complete, so that no half of it is
 * ever shown. The intensity and other registers are sent at once either way: they are not part of the picture.
 *
 * A shifting text keeps its own pace: shiftTask() may be called on every loop() and takes a step only when ShiftInterval
 * has passed since the last one, so that no sketch has to time it. */
class MaxMatrix
{
  private:
    MaxMatrixPanel Panel;
    MaxMatrixText Text;
    MaxMatrixStateType State;
    bool AutoUpdate;
    uint16_t ShiftInterval;
    uint32_t LastShift;

    // functions
    void updateIfAuto() { if(AutoUpdate) update(); }
    stdReturnType updateIfAuto(stdReturnType Result) { updateIfAuto(); return Result; }
    void startShift(MaxMatrixStateType);

  public:
    MaxMatrix(byte, byte, byte);
    ~MaxMatrix();


    // get methods
    MaxMatrixStateType getState() { return State; }
    bool getAutoUpdate() { return AutoUpdate; }
    uint16_t getShiftInterval() { return ShiftInterval; }
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
    void setAutoUpdate(bool);
    void setShiftInterval(uint16_t sShiftInterval) { ShiftInterval = sShiftInterval; }
    stdReturnType setIntensity(byte);
    stdReturnType setColumn(byte Column, byte Value) { return updateIfAuto(Panel.setColumn(Column, Value)); }
    stdReturnType setColumn(byte Module, byte Column, byte Value) { return updateIfAuto(Panel.setColumn(Module, Column, Value)); }
    stdReturnType setRow(byte Row, const rowType Value) { return updateIfAuto(Panel.setRow(Row, Value)); }
    stdReturnType setRow(byte Module, byte Row, byte Value) { return updateIfAuto(Panel.setRow(Module, Row, Value)); }
    stdReturnType setDot(byte Column, byte Row, bool Value) { return updateIfAuto(Panel.setDot(Column, Row, Value)); }
    stdReturnType setChar(int16_t X, int16_t Y, char Char) { return updateIfAuto(Text.setChar(Panel, X, Y, Char)); }
    stdReturnType setCharWithShift(char);
    stdReturnType setText(const char* String) { return updateIfAuto(Text.setText(Panel, String)); }
    stdReturnType setTextWithShift(char*);
    stdReturnType setSpaceBetweenChars(byte sSpaceBetweenChars) { return Text.setSpaceBetweenChars(sSpaceBetweenChars); }
    void setSprite(int16_t X, int16_t Y, const spriteType Sprite) { Text.setSprite(Panel, X, Y, Sprite); updateIfAuto(); }
    void setModuleOrientation(MaxMatrixModuleOrientationType sModuleOrientation) { Panel.setModuleOrientation(sModuleOrientation); updateIfAuto(); }
    void setMatrixOrientation(MaxMatrixMatrixOrientationType sMatrixOrientation) { Panel.setMatrixOrientation(sMatrixOrientation); updateIfAuto(); }


    // methods
    void init();
    void shiftTask();
    void update() { Panel.getWired().send(); }
    void clear() { Panel.getWired().clear(); updateIfAuto(); }
    void RegisterWrite(byte RegisterAddress, byte RegisterValue) { Panel.getWired().getChain().writeRegisterAll(RegisterAddress, RegisterValue); }

    void shiftLeft(bool Rotate = false, bool FillWithZero = true) { Panel.shiftLeft(Rotate, FillWithZero); updateIfAuto(); }
    void shiftRight(bool Rotate = false, bool FillWithZero = true) { Panel.shiftRight(Rotate, FillWithZero); updateIfAuto(); }
    void shiftUp(bool Rotate = false) { Panel.shiftUp(Rotate); updateIfAuto(); }
    void shiftDown(bool Rotate = false) { Panel.shiftDown(Rotate); updateIfAuto(); }
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixBatch
 *****************************************************************************************************************************************************/
/* Holds the sending back for as long as it exists, as std::lock_guard holds a mutex: the constructor turns AutoUpdate
 * off, the destructor puts back what was there before. So the picture is sent at the end of the block, also on a
 * return or break out of it, and of nested ones only the outermost sends. It has to be a named variable: an unnamed
 * MaxMatrixBatch{Matrix}; is destroyed at once and holds nothing back. A picture built over several calls of loop()
 * does not fit into a block; that is what setAutoUpdate() and update() are for. */
class MaxMatrixBatch
{
  private:
    MaxMatrix& Matrix;
    bool PreviousAutoUpdate;

  public:
    explicit MaxMatrixBatch(MaxMatrix& sMatrix) : Matrix(sMatrix)
    {
        PreviousAutoUpdate = Matrix.getAutoUpdate();
        Matrix.setAutoUpdate(false);
    }
    ~MaxMatrixBatch() { Matrix.setAutoUpdate(PreviousAutoUpdate); }

    /* a copy would restore the state twice */
    MaxMatrixBatch(const MaxMatrixBatch&) = delete;
    MaxMatrixBatch& operator=(const MaxMatrixBatch&) = delete;
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
