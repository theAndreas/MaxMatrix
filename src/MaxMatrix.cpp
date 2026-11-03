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
/**     \file       MaxMatrix.cpp
 *      \brief      Main file of MaxMatrix library
 *
 *      \details    Arduino library to drive 8x8 LED display Matrix modules with MAX7219
 *
 *                  What the facade does itself: setting the whole chain up, the state of the library, which the
 *                  shifting text only reports into, and when what was drawn is sent.
 *
 *****************************************************************************************************************************************************/
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrix.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrix
******************************************************************************************************************************************************/
/*! \brief          MaxMatrix constructor
 *  \details        Instantiation of the MaxMatrix library
 *
 *  \param[in]      sDataInPin         number of the Data Input Pin
 *  \param[in]      sChipSelectPin     number of the Chip Select (CS) Pin
 *  \param[in]      sClockPin          number of the Clock Pin
 *****************************************************************************************************************************************************/
MaxMatrix::MaxMatrix(byte data_in_pin, byte chip_select_pin, byte clock_pin) : panel_(data_in_pin, chip_select_pin, clock_pin)
{
    state_ = MaxMatrixState::kNone;
    auto_update_ = true;
    shift_interval_ = MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE;
    last_shift_ = 0;
} /* MaxMatrix */


/******************************************************************************************************************************************************
  DESTRUCTOR OF MaxMatrix
******************************************************************************************************************************************************/
MaxMatrix::~MaxMatrix()
{

} /* ~MaxMatrix */


/******************************************************************************************************************************************************
  init()
******************************************************************************************************************************************************/
/*! \brief          initialization of the Matrix hardware
 *  \details        this function initializes the IO hardware and the MAX7219 controller
 *                  all LEDs of the matrix will be cleared and intensity will be set to init value.
 *                  The cleared matrix is sent even with AutoUpdate off: after power-up the LEDs show anything.
 *****************************************************************************************************************************************************/
void MaxMatrix::Init()
{
    state_ = MaxMatrixState::kInit;

    panel_.GetWired().GetChain().Init();

    /* initialize registers, turn all LEDs off */
    panel_.GetWired().Clear();
    Update();

    SetIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
    state_ = MaxMatrixState::kReady;
} /* init */


/******************************************************************************************************************************************************
  shiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts char or string over the matrix
 *  \details        this function may be called on every loop() until the state changes from
 *                  MaxMatrixState::kCharShift or MaxMatrixState::kStringShift to MaxMatrixState::kReady. It takes a step
 *                  only when ShiftInterval has passed since the last one, and with an interval of 0 on every call.
 *                  A call that comes late takes one step, not the ones it missed: catching up would make the text
 *                  jump after loop() was held up, and the interval counts from this step on.
 *  \pre            setCharWithShift or setTextWithShift must be called first
 *****************************************************************************************************************************************************/
void MaxMatrix::ShiftTask()
{
    uint32_t const now = millis();

    if(now - last_shift_ < shift_interval_) return;
    last_shift_ = now;

    if(MaxMatrixState::kStringShift == state_) {
        if(!text_.StringShiftTask(panel_)) state_ = MaxMatrixState::kReady;
    }
    if(MaxMatrixState::kCharShift == state_) {
        if(!text_.CharShiftTask(panel_)) state_ = MaxMatrixState::kReady;
    }
    UpdateIfAuto();
} /* shiftTask */


/******************************************************************************************************************************************************
  setAutoUpdate()
******************************************************************************************************************************************************/
/*! \brief          sets whether drawing is sent at once
 *  \details        this function switches between sending after every call that draws, the default, and sending only
 *                  when update() is called. Switching it on sends what was drawn while it was off.
 *
 *  \param[in]      sAutoUpdate     true to send after every call that draws
 *****************************************************************************************************************************************************/
void MaxMatrix::SetAutoUpdate(bool auto_update)
{
    auto_update_ = auto_update;
    UpdateIfAuto();
} /* setAutoUpdate */


/******************************************************************************************************************************************************
  setIntensity()
******************************************************************************************************************************************************/
/*! \brief          set led intensity of matrix module
 *  \details        
 *                  
 *  \param[in]      Intensity       intensity value from 0 to 15
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kOutOfRange    above 15; the intensity stays as it was
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrix::SetIntensity(byte intensity)
{
    if(intensity > MAX7219_REG_INTENSITY_MAX_VALUE) return MAXMATRIX_REPORT_ERROR("setIntensity", MaxMatrixStatus(MaxMatrixError::kOutOfRange));
    RegisterWrite(MAX7219_REG_INTENSITY_ADDRESS, intensity);
    return MaxMatrixError::kOk;
} /* setIntensity */


/******************************************************************************************************************************************************
  setCharWithShift()
******************************************************************************************************************************************************/
/*! \brief          shifts a char into the matrix and stops on the right side
 *  \details        this function shifts a given char into the matrix and stops shifting on the right side.
 *                  Every shift process will be triggered by calling the shiftMainFunction. 
 *                  
 *  \param[in]      c        char to print on matrix
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kUnknownChar    the font has no sprite for the char; nothing starts
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrix::SetCharWithShift(char c)
{
    MaxMatrixStatus status = MAXMATRIX_REPORT_ERROR("setCharWithShift", text_.SetCharWithShift(c));

    if(status.IsOk()) StartShift(MaxMatrixState::kCharShift);
    return status;
} /* setCharWithShift */


/******************************************************************************************************************************************************
  setTextWithShift()
******************************************************************************************************************************************************/
/*! \brief          shifts a string into the matrix and stops after the last char on the right side
 *  \details        this function shifts a given string into the matrix and stops shifting on the right side,
 *                  when the last char appears on the matrix.
 *
 *  \param[in]      sString     string to print on matrix
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kNoText         the string is NULL; nothing starts
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrix::SetTextWithShift(char* string)
{
    MaxMatrixStatus status = MAXMATRIX_REPORT_ERROR("setTextWithShift", text_.SetTextWithShift(string));

    if(status.IsOk()) StartShift(MaxMatrixState::kStringShift);
    return status;
} /* setTextWithShift */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  startShift()
******************************************************************************************************************************************************/
/*! \brief          starts shifting a char or a string
 *  \details        this function sets the state and dates the last step one interval back, so that the first step
 *                  comes on the next call of shiftTask() rather than an interval later
 *
 *  \param[in]      ShiftState      MaxMatrixState::kCharShift or MaxMatrixState::kStringShift
 *****************************************************************************************************************************************************/
void MaxMatrix::StartShift(MaxMatrixState shift_state)
{
    state_ = shift_state;
    last_shift_ = millis() - shift_interval_;
} /* startShift */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
