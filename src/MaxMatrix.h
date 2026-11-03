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
#include "MaxMatrixError.h"
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
typedef enum : uint8_t {
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
    MaxMatrixStatus updateIfAuto(MaxMatrixStatus Status) { updateIfAuto(); return Status; }
    void startShift(MaxMatrixStateType);

  public:
    MaxMatrix(byte, byte, byte);
    ~MaxMatrix();


    // get methods
    MaxMatrixStateType getState() const { return State; }
    bool getAutoUpdate() const { return AutoUpdate; }
    uint16_t getShiftInterval() const { return ShiftInterval; }
    byte getDataInPin() const { return Panel.getWired().getChain().getDataInPin(); }
    byte getChipSelectPin() const { return Panel.getWired().getChain().getChipSelectPin(); }
    byte getClockPin() const { return Panel.getWired().getChain().getClockPin(); }
    MaxMatrixModuleOrientationType getModuleOrientation() const { return Panel.getModuleOrientation(); }
    MaxMatrixMatrixOrientationType getMatrixOrientation() const { return Panel.getMatrixOrientation(); }
    byte getSpaceBetweenChars() const { return Text.getSpaceBetweenChars(); }

    MaxMatrixResult<byte> getColumn(int16_t Column) { return MAXMATRIX_REPORT_ERROR("getColumn", Panel.getColumn(Column)); }
    MaxMatrixResult<byte> getColumn(byte Module, byte Column) { return MAXMATRIX_REPORT_ERROR("getColumn", Panel.getColumn(Module, Column)); }
    MaxMatrixStatus getRow(int16_t Row, rowType& Value) { return MAXMATRIX_REPORT_ERROR("getRow", Panel.getRow(Row, Value)); }
    MaxMatrixResult<byte> getRow(byte Module, byte Row) { return MAXMATRIX_REPORT_ERROR("getRow", Panel.getRow(Module, Row)); }
    MaxMatrixResult<bool> getDot(int16_t Column, int16_t Row) { return MAXMATRIX_REPORT_ERROR("getDot", Panel.getDot(Column, Row)); }
    static MaxMatrixStatus getSprite(spriteIndexType SpriteIndex, spriteType* Sprite) { return MAXMATRIX_REPORT_ERROR("getSprite", MaxMatrixFont::getSprite(SpriteIndex, Sprite)); }


    // set methods
    void setDataInPin(byte sDataInPin) { Panel.getWired().getChain().setDataInPin(sDataInPin); }
    void setChipSelectPin(byte sChipSelectPin) { Panel.getWired().getChain().setChipSelectPin(sChipSelectPin); }
    void setClockPin(byte sClockPin) { Panel.getWired().getChain().setClockPin(sClockPin); }
    void setAutoUpdate(bool);
    void setShiftInterval(uint16_t sShiftInterval) { ShiftInterval = sShiftInterval; }
    MaxMatrixStatus setIntensity(byte);
    void setColumn(int16_t Column, byte Value) { Panel.setColumn(Column, Value); updateIfAuto(); }
    void setColumn(byte Module, byte Column, byte Value) { Panel.setColumn(Module, Column, Value); updateIfAuto(); }
    void setRow(int16_t Row, rowType const Value) { Panel.setRow(Row, Value); updateIfAuto(); }
    void setRow(byte Module, byte Row, byte Value) { Panel.setRow(Module, Row, Value); updateIfAuto(); }
    void setDot(int16_t Column, int16_t Row, bool Value) { Panel.setDot(Column, Row, Value); updateIfAuto(); }
    MaxMatrixStatus setChar(int16_t X, int16_t Y, char Char) { return updateIfAuto(MAXMATRIX_REPORT_ERROR("setChar", Text.setChar(Panel, X, Y, Char))); }
    MaxMatrixStatus setCharWithShift(char);
    MaxMatrixStatus setText(char const* String) { return updateIfAuto(MAXMATRIX_REPORT_ERROR("setText", Text.setText(Panel, String))); }
    MaxMatrixStatus setTextWithShift(char*);
    void setSpaceBetweenChars(byte sSpaceBetweenChars) { Text.setSpaceBetweenChars(sSpaceBetweenChars); }
    void setSprite(int16_t X, int16_t Y, spriteType const Sprite) { Text.setSprite(Panel, X, Y, Sprite); updateIfAuto(); }
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
    MaxMatrixBatch(MaxMatrixBatch const&) = delete;
    MaxMatrixBatch& operator=(MaxMatrixBatch const&) = delete;
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
