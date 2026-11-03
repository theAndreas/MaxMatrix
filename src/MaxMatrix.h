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
/*! what a MaxMatrix is doing, as getState() tells it */
typedef enum : uint8_t {
    MAXMATRIX_STATE_NONE,                   /*!< constructed, init() not called yet */
    MAXMATRIX_STATE_INIT,                   /*!< init() is running */
    MAXMATRIX_STATE_READY,                  /*!< nothing to shift: a text or char shifted in is complete */
    MAXMATRIX_STATE_STRING_SHIFT,           /*!< a text of setTextWithShift() is being shifted in */
    MAXMATRIX_STATE_CHAR_SHIFT              /*!< a char of setCharWithShift() is being shifted in */
} MaxMatrixStateType;


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrix
 *****************************************************************************************************************************************************/
/*! The matrix a sketch draws on: a chain of 8x8 modules, in the coordinates the viewer sees.
 *
 * Every function that draws changes the buffer, and with AutoUpdate, the default, sends what changed before it returns:
 * one frame per changed column of a module, eight at most, however much the call drew. A sketch that builds one picture
 * out of many calls turns AutoUpdate off and calls update() when the picture is complete, so that no half of it is
 * ever shown. The intensity and other registers are sent at once either way: they are not part of the picture.
 *
 * A shifting text keeps its own pace: shiftTask() may be called on every loop() and takes a step only when ShiftInterval
 * has passed since the last one, so that no sketch has to time it.
 *
 * With MAXMATRIX_DEV_ERROR_DETECT, every function here that fails reports its name and the error to
 * maxMatrixReportError(). */
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
    /*! \brief what the matrix is doing; MAXMATRIX_STATE_READY once a shifted text is complete */
    MaxMatrixStateType getState() const { return State; }
    /*! \brief whether every function that draws sends its result at once, see setAutoUpdate() */
    bool getAutoUpdate() const { return AutoUpdate; }
    /*! \brief the milliseconds per column of a shifting text, see setShiftInterval() */
    uint16_t getShiftInterval() const { return ShiftInterval; }
    /*! \brief the pin DIN of the chain is connected to */
    byte getDataInPin() const { return Panel.getWired().getChain().getDataInPin(); }
    /*! \brief the pin CS of the chain is connected to */
    byte getChipSelectPin() const { return Panel.getWired().getChain().getChipSelectPin(); }
    /*! \brief the pin CLK of the chain is connected to */
    byte getClockPin() const { return Panel.getWired().getChain().getClockPin(); }
    /*! \brief how every module is turned in its place, see setModuleOrientation() */
    MaxMatrixModuleOrientationType getModuleOrientation() const { return Panel.getModuleOrientation(); }
    /*! \brief how the whole matrix is turned, see setMatrixOrientation() */
    MaxMatrixMatrixOrientationType getMatrixOrientation() const { return Panel.getMatrixOrientation(); }
    /*! \brief the empty columns between two chars of a text */
    byte getSpaceBetweenChars() const { return Text.getSpaceBetweenChars(); }

    /*! \copydoc MaxMatrixPanel::getColumn(int16_t) */
    MaxMatrixResult<byte> getColumn(int16_t Column) { return MAXMATRIX_REPORT_ERROR("getColumn", Panel.getColumn(Column)); }
    /*! \copydoc MaxMatrixPanel::getColumn(byte, byte) */
    MaxMatrixResult<byte> getColumn(byte Module, byte Column) { return MAXMATRIX_REPORT_ERROR("getColumn", Panel.getColumn(Module, Column)); }
    /*! \copydoc MaxMatrixPanel::getRow(int16_t, rowType&) */
    MaxMatrixStatus getRow(int16_t Row, rowType& Value) { return MAXMATRIX_REPORT_ERROR("getRow", Panel.getRow(Row, Value)); }
    /*! \copydoc MaxMatrixPanel::getRow(byte, byte) */
    MaxMatrixResult<byte> getRow(byte Module, byte Row) { return MAXMATRIX_REPORT_ERROR("getRow", Panel.getRow(Module, Row)); }
    /*! \copydoc MaxMatrixPanel::getDot(int16_t, int16_t) */
    MaxMatrixResult<bool> getDot(int16_t Column, int16_t Row) { return MAXMATRIX_REPORT_ERROR("getDot", Panel.getDot(Column, Row)); }
    /*! \copydoc MaxMatrixFont::getSprite(spriteIndexType, spriteType*) */
    static MaxMatrixStatus getSprite(spriteIndexType SpriteIndex, spriteType* Sprite) { return MAXMATRIX_REPORT_ERROR("getSprite", MaxMatrixFont::getSprite(SpriteIndex, Sprite)); }


    // set methods
    /*! \brief sets the pin DIN of the chain is connected to; init() makes it an output */
    void setDataInPin(byte sDataInPin) { Panel.getWired().getChain().setDataInPin(sDataInPin); }
    /*! \brief sets the pin CS of the chain is connected to; init() makes it an output */
    void setChipSelectPin(byte sChipSelectPin) { Panel.getWired().getChain().setChipSelectPin(sChipSelectPin); }
    /*! \brief sets the pin CLK of the chain is connected to; init() makes it an output */
    void setClockPin(byte sClockPin) { Panel.getWired().getChain().setClockPin(sClockPin); }
    void setAutoUpdate(bool);
    /*! \brief sets the milliseconds per column of a shifting text; 0 makes every shiftTask() a step */
    void setShiftInterval(uint16_t sShiftInterval) { ShiftInterval = sShiftInterval; }
    MaxMatrixStatus setIntensity(byte);
    /*! \copydoc MaxMatrixPanel::setColumn(int16_t, byte) */
    void setColumn(int16_t Column, byte Value) { Panel.setColumn(Column, Value); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::setColumn(byte, byte, byte) */
    void setColumn(byte Module, byte Column, byte Value) { Panel.setColumn(Module, Column, Value); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::setRow(int16_t, rowType const) */
    void setRow(int16_t Row, rowType const Value) { Panel.setRow(Row, Value); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::setRow(byte, byte, byte) */
    void setRow(byte Module, byte Row, byte Value) { Panel.setRow(Module, Row, Value); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::setDot(int16_t, int16_t, bool) */
    void setDot(int16_t Column, int16_t Row, bool Value) { Panel.setDot(Column, Row, Value); updateIfAuto(); }
    /*! \brief draws a char with its top left corner at X, Y; what lies off the matrix is left out
     *  \param[in]      X           column of the left edge, may lie off the matrix
     *  \param[in]      Y           row of the top edge, may lie off the matrix
     *  \param[in]      Char        the char, latin-1
     *  \return         MAXMATRIX_E_OK
     *                  MAXMATRIX_E_UNKNOWN_CHAR    the font has no sprite for the char; nothing is drawn */
    MaxMatrixStatus setChar(int16_t X, int16_t Y, char Char) { return updateIfAuto(MAXMATRIX_REPORT_ERROR("setChar", Text.setChar(Panel, X, Y, Char))); }
    MaxMatrixStatus setCharWithShift(char);
    /*! \brief draws a text from the left edge, as far as it fits
     *  \param[in]      String      the text, UTF-8 or latin-1
     *  \return         MAXMATRIX_E_OK
     *                  MAXMATRIX_E_NO_TEXT         String is NULL
     *                  MAXMATRIX_E_UNKNOWN_CHAR    a char of the text is not in the font; the chars before it are drawn */
    MaxMatrixStatus setText(char const* String) { return updateIfAuto(MAXMATRIX_REPORT_ERROR("setText", Text.setText(Panel, String))); }
    MaxMatrixStatus setTextWithShift(char*);
    /*! \brief sets the empty columns between two chars of a text */
    void setSpaceBetweenChars(byte sSpaceBetweenChars) { Text.setSpaceBetweenChars(sSpaceBetweenChars); }
    /*! \brief draws a sprite with its top left corner at X, Y; what lies off the matrix is left out
     *  \param[in]      X           column of the left edge, may lie off the matrix
     *  \param[in]      Y           row of the top edge, may lie off the matrix
     *  \param[in]      Sprite      width, height and up to five columns, bit 0 the top row */
    void setSprite(int16_t X, int16_t Y, spriteType const Sprite) { Text.setSprite(Panel, X, Y, Sprite); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::setModuleOrientation(MaxMatrixModuleOrientationType) */
    void setModuleOrientation(MaxMatrixModuleOrientationType sModuleOrientation) { Panel.setModuleOrientation(sModuleOrientation); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::setMatrixOrientation(MaxMatrixMatrixOrientationType) */
    void setMatrixOrientation(MaxMatrixMatrixOrientationType sMatrixOrientation) { Panel.setMatrixOrientation(sMatrixOrientation); updateIfAuto(); }


    // methods
    void init();
    void shiftTask();
    /*! \brief sends what was drawn since the last update; only needed with AutoUpdate off, see MaxMatrixBatch */
    void update() { Panel.getWired().send(); }
    /*! \brief turns every LED off */
    void clear() { Panel.getWired().clear(); updateIfAuto(); }
    /*! \brief writes one register of every MAX7219 in the chain, at once
     *  \param[in]      RegisterAddress     address of the register, one of the MAX7219_REG_..._ADDRESS
     *  \param[in]      RegisterValue       the value for every module */
    void RegisterWrite(byte RegisterAddress, byte RegisterValue) { Panel.getWired().getChain().writeRegisterAll(RegisterAddress, RegisterValue); }

    /*! \copydoc MaxMatrixPanel::shiftLeft(bool, bool) */
    void shiftLeft(bool Rotate = false, bool FillWithZero = true) { Panel.shiftLeft(Rotate, FillWithZero); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::shiftRight(bool, bool) */
    void shiftRight(bool Rotate = false, bool FillWithZero = true) { Panel.shiftRight(Rotate, FillWithZero); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::shiftUp(bool) */
    void shiftUp(bool Rotate = false) { Panel.shiftUp(Rotate); updateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::shiftDown(bool) */
    void shiftDown(bool Rotate = false) { Panel.shiftDown(Rotate); updateIfAuto(); }
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixBatch
 *****************************************************************************************************************************************************/
/*! Holds the sending back for as long as it exists, as std::lock_guard holds a mutex: the constructor turns AutoUpdate
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
    /*! \brief turns AutoUpdate of the matrix off until the end of the block
     *  \param[in,out]  sMatrix     the matrix; it has to outlive the batch */
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
