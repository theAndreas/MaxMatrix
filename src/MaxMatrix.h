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
#ifndef MAXMATRIX_SRC_MAXMATRIX_H_
#define MAXMATRIX_SRC_MAXMATRIX_H_

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
/*! what a MaxMatrix is doing, as GetState() tells it */
enum class MaxMatrixState : uint8_t {
    kNone,                   /*!< constructed, Init() not called yet */
    kInit,                   /*!< Init() is running */
    kReady,                  /*!< nothing to shift: a text or char shifted in is complete */
    kStringShift,           /*!< a text of SetTextWithShift() is being shifted in */
    kCharShift              /*!< a char of SetCharWithShift() is being shifted in */
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrix
 *****************************************************************************************************************************************************/
/*! The matrix a sketch draws on: a chain of 8x8 modules, in the coordinates the viewer sees.
 *
 * Every function that draws changes the buffer, and with auto_update_, the default, sends what changed before it returns:
 * one frame per changed column of a module, eight at most, however much the call drew. A sketch that builds one picture
 * out of many calls turns auto_update_ off and calls Update() when the picture is complete, so that no half of it is
 * ever shown. The intensity and other registers are sent at once either way: they are not part of the picture.
 *
 * A shifting text keeps its own pace: ShiftTask() may be called on every loop() and takes a step only when shift_interval_
 * has passed since the last one, so that no sketch has to time it.
 *
 * With MAXMATRIX_DEV_ERROR_DETECT, every function here that fails reports its name and the error to
 * MaxMatrixReportError(). */
class MaxMatrix
{
  private:
    MaxMatrixPanel panel_;
    MaxMatrixText text_;
    MaxMatrixState state_;
    bool auto_update_;
    uint16_t shift_interval_;
    uint32_t last_shift_;

    // functions
    void UpdateIfAuto() { if(auto_update_) Update(); }
    MaxMatrixStatus UpdateIfAuto(MaxMatrixStatus status) { UpdateIfAuto(); return status; }
    void StartShift(MaxMatrixState);

  public:
    MaxMatrix(byte, byte, byte);
    ~MaxMatrix();


    // get methods
    /*! \brief what the matrix is doing; MaxMatrixState::kReady once a shifted text is complete */
    MaxMatrixState GetState() const { return state_; }
    /*! \brief whether every function that draws sends its result at once, see SetAutoUpdate() */
    bool GetAutoUpdate() const { return auto_update_; }
    /*! \brief the milliseconds per column of a shifting text, see SetShiftInterval() */
    uint16_t GetShiftInterval() const { return shift_interval_; }
    /*! \brief the pin DIN of the chain is connected to */
    byte GetDataInPin() const { return panel_.GetWired().GetChain().GetDataInPin(); }
    /*! \brief the pin CS of the chain is connected to */
    byte GetChipSelectPin() const { return panel_.GetWired().GetChain().GetChipSelectPin(); }
    /*! \brief the pin CLK of the chain is connected to */
    byte GetClockPin() const { return panel_.GetWired().GetChain().GetClockPin(); }
    /*! \brief how every module is turned in its place, see SetModuleOrientation() */
    MaxMatrixModuleOrientation GetModuleOrientation() const { return panel_.GetModuleOrientation(); }
    /*! \brief how the whole matrix is turned, see SetMatrixOrientation() */
    MaxMatrixMatrixOrientation GetMatrixOrientation() const { return panel_.GetMatrixOrientation(); }
    /*! \brief the empty columns between two chars of a text */
    byte GetSpaceBetweenChars() const { return text_.GetSpaceBetweenChars(); }

    /*! \copydoc MaxMatrixPanel::GetColumn(int16_t) */
    MaxMatrixResult<byte> GetColumn(int16_t column) { return MAXMATRIX_REPORT_ERROR("GetColumn", panel_.GetColumn(column)); }
    /*! \copydoc MaxMatrixPanel::GetColumn(byte, byte) */
    MaxMatrixResult<byte> GetColumn(byte module, byte column) { return MAXMATRIX_REPORT_ERROR("GetColumn", panel_.GetColumn(module, column)); }
    /*! \copydoc MaxMatrixPanel::GetRow(int16_t, MaxMatrixRow&) */
    MaxMatrixStatus GetRow(int16_t row, MaxMatrixRow& value) { return MAXMATRIX_REPORT_ERROR("GetRow", panel_.GetRow(row, value)); }
    /*! \copydoc MaxMatrixPanel::GetRow(byte, byte) */
    MaxMatrixResult<byte> GetRow(byte module, byte row) { return MAXMATRIX_REPORT_ERROR("GetRow", panel_.GetRow(module, row)); }
    /*! \copydoc MaxMatrixPanel::GetDot(int16_t, int16_t) */
    MaxMatrixResult<bool> GetDot(int16_t column, int16_t row) { return MAXMATRIX_REPORT_ERROR("GetDot", panel_.GetDot(column, row)); }
    /*! \copydoc MaxMatrixFont::GetSprite(MaxMatrixSpriteIndex, MaxMatrixSprite*) */
    static MaxMatrixStatus GetSprite(MaxMatrixSpriteIndex sprite_index, MaxMatrixSprite* sprite) { return MAXMATRIX_REPORT_ERROR("GetSprite", MaxMatrixFont::GetSprite(sprite_index, sprite)); }


    // set methods
    /*! \brief sets the pin DIN of the chain is connected to; Init() makes it an output */
    void SetDataInPin(byte data_in_pin) { panel_.GetWired().GetChain().SetDataInPin(data_in_pin); }
    /*! \brief sets the pin CS of the chain is connected to; Init() makes it an output */
    void SetChipSelectPin(byte chip_select_pin) { panel_.GetWired().GetChain().SetChipSelectPin(chip_select_pin); }
    /*! \brief sets the pin CLK of the chain is connected to; Init() makes it an output */
    void SetClockPin(byte clock_pin) { panel_.GetWired().GetChain().SetClockPin(clock_pin); }
    void SetAutoUpdate(bool);
    /*! \brief sets the milliseconds per column of a shifting text; 0 makes every ShiftTask() a step */
    void SetShiftInterval(uint16_t shift_interval) { shift_interval_ = shift_interval; }
    MaxMatrixStatus SetIntensity(byte);
    /*! \copydoc MaxMatrixPanel::SetColumn(int16_t, byte) */
    void SetColumn(int16_t column, byte value) { panel_.SetColumn(column, value); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::SetColumn(byte, byte, byte) */
    void SetColumn(byte module, byte column, byte value) { panel_.SetColumn(module, column, value); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::SetRow(int16_t, MaxMatrixRow const) */
    void SetRow(int16_t row, MaxMatrixRow const value) { panel_.SetRow(row, value); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::SetRow(byte, byte, byte) */
    void SetRow(byte module, byte row, byte value) { panel_.SetRow(module, row, value); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::SetDot(int16_t, int16_t, bool) */
    void SetDot(int16_t column, int16_t row, bool value) { panel_.SetDot(column, row, value); UpdateIfAuto(); }
    /*! \brief draws a char with its top left corner at X, Y; what lies off the matrix is left out
     *  \param[in]      x           column of the left edge, may lie off the matrix
     *  \param[in]      y           row of the top edge, may lie off the matrix
     *  \param[in]      c        the char, latin-1
     *  \return         MaxMatrixError::kOk
     *                  MaxMatrixError::kUnknownChar    the font has no sprite for the char; nothing is drawn */
    MaxMatrixStatus SetChar(int16_t x, int16_t y, char c) { return UpdateIfAuto(MAXMATRIX_REPORT_ERROR("SetChar", text_.SetChar(panel_, x, y, c))); }
    MaxMatrixStatus SetCharWithShift(char);
    /*! \brief draws a text from the left edge, as far as it fits
     *  \param[in]      string      the text, UTF-8 or latin-1
     *  \return         MaxMatrixError::kOk
     *                  MaxMatrixError::kNoText         String is NULL
     *                  MaxMatrixError::kUnknownChar    a char of the text is not in the font; the chars before it are drawn */
    MaxMatrixStatus SetText(char const* string) { return UpdateIfAuto(MAXMATRIX_REPORT_ERROR("SetText", text_.SetText(panel_, string))); }
    MaxMatrixStatus SetTextWithShift(char*);
    /*! \brief sets the empty columns between two chars of a text */
    void SetSpaceBetweenChars(byte space_between_chars) { text_.SetSpaceBetweenChars(space_between_chars); }
    /*! \brief draws a sprite with its top left corner at X, Y; what lies off the matrix is left out
     *  \param[in]      x           column of the left edge, may lie off the matrix
     *  \param[in]      y           row of the top edge, may lie off the matrix
     *  \param[in]      sprite      width, height and up to five columns, bit 0 the top row */
    void SetSprite(int16_t x, int16_t y, MaxMatrixSprite const sprite) { text_.SetSprite(panel_, x, y, sprite); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::SetModuleOrientation(MaxMatrixModuleOrientation) */
    void SetModuleOrientation(MaxMatrixModuleOrientation module_orientation) { panel_.SetModuleOrientation(module_orientation); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::SetMatrixOrientation(MaxMatrixMatrixOrientation) */
    void SetMatrixOrientation(MaxMatrixMatrixOrientation matrix_orientation) { panel_.SetMatrixOrientation(matrix_orientation); UpdateIfAuto(); }


    // methods
    void Init();
    void ShiftTask();
    /*! \brief sends what was drawn since the last update; only needed with auto_update_ off, see MaxMatrixBatch */
    void Update() { panel_.GetWired().Send(); }
    /*! \brief turns every LED off */
    void Clear() { panel_.GetWired().Clear(); UpdateIfAuto(); }
    /*! \brief writes one register of every MAX7219 in the chain, at once
     *  \param[in]      register_address    address of the register, one of the MAX7219_REG_..._ADDRESS
     *  \param[in]      register_value      the value for every module */
    void RegisterWrite(byte register_address, byte register_value) { panel_.GetWired().GetChain().WriteRegisterAll(register_address, register_value); }

    /*! \copydoc MaxMatrixPanel::ShiftLeft(bool, bool) */
    void ShiftLeft(bool rotate = false, bool fill_with_zero = true) { panel_.ShiftLeft(rotate, fill_with_zero); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::ShiftRight(bool, bool) */
    void ShiftRight(bool rotate = false, bool fill_with_zero = true) { panel_.ShiftRight(rotate, fill_with_zero); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::ShiftUp(bool) */
    void ShiftUp(bool rotate = false) { panel_.ShiftUp(rotate); UpdateIfAuto(); }
    /*! \copydoc MaxMatrixPanel::ShiftDown(bool) */
    void ShiftDown(bool rotate = false) { panel_.ShiftDown(rotate); UpdateIfAuto(); }
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixBatch
 *****************************************************************************************************************************************************/
/*! Holds the sending back for as long as it exists, as std::lock_guard holds a mutex: the constructor turns auto_update_
 * off, the destructor puts back what was there before. So the picture is sent at the end of the block, also on a
 * return or break out of it, and of nested ones only the outermost sends. It has to be a named variable: an unnamed
 * MaxMatrixBatch{Matrix}; is destroyed at once and holds nothing back. A picture built over several calls of loop()
 * does not fit into a block; that is what SetAutoUpdate() and Update() are for. */
class MaxMatrixBatch
{
  private:
    MaxMatrix& matrix_;
    bool previous_auto_update_;

  public:
    /*! \brief turns auto_update_ of the matrix off until the end of the block
     *  \param[in,out]  matrix      the matrix; it has to outlive the batch */
    explicit MaxMatrixBatch(MaxMatrix& matrix) : matrix_(matrix)
    {
        previous_auto_update_ = matrix_.GetAutoUpdate();
        matrix_.SetAutoUpdate(false);
    }
    ~MaxMatrixBatch() { matrix_.SetAutoUpdate(previous_auto_update_); }

    /* a copy would restore the state twice */
    MaxMatrixBatch(MaxMatrixBatch const&) = delete;
    MaxMatrixBatch& operator=(MaxMatrixBatch const&) = delete;
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
