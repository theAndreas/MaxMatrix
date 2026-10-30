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
#define _MAXMATRIX_SOURCE_

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
 *  \return         -
 *****************************************************************************************************************************************************/
MaxMatrix::MaxMatrix(byte sDataInPin, byte sChipSelectPin, byte sClockPin) : Panel(sDataInPin, sChipSelectPin, sClockPin)
{
    State = MAXMATRIX_STATE_NONE;
    AutoUpdate = true;
    ShiftInterval = MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE;
    LastShift = 0;
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
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::init()
{
    State = MAXMATRIX_STATE_INIT;

    Panel.getWired().getChain().init();

    /* initialize registers, turn all LEDs off */
    Panel.getWired().clear();
    update();

    setIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
    State = MAXMATRIX_STATE_READY;
} /* init */


/******************************************************************************************************************************************************
  shiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts char or string over the matrix
 *  \details        this function may be called on every loop() until the state changes from
 *                  MAXMATRIX_STATE_CHAR_SHIFT or MAXMATRIX_STATE_STRING_SHIFT to MAXMATRIX_STATE_READY. It takes a step
 *                  only when ShiftInterval has passed since the last one, and with an interval of 0 on every call.
 *                  A call that comes late takes one step, not the ones it missed: catching up would make the text
 *                  jump after loop() was held up, and the interval counts from this step on.
 *  \return         -
 *  \pre            setCharWithShift or setTextWithShift must be called first
 *****************************************************************************************************************************************************/
void MaxMatrix::shiftTask()
{
    const uint32_t Now = millis();

    if(Now - LastShift < ShiftInterval) return;
    LastShift = Now;

    if(MAXMATRIX_STATE_STRING_SHIFT == State) {
        if(!Text.stringShiftTask(Panel)) State = MAXMATRIX_STATE_READY;
    }
    if(MAXMATRIX_STATE_CHAR_SHIFT == State) {
        if(!Text.charShiftTask(Panel)) State = MAXMATRIX_STATE_READY;
    }
    updateIfAuto();
} /* shiftTask */


/******************************************************************************************************************************************************
  setAutoUpdate()
******************************************************************************************************************************************************/
/*! \brief          sets whether drawing is sent at once
 *  \details        this function switches between sending after every call that draws, the default, and sending only
 *                  when update() is called. Switching it on sends what was drawn while it was off.
 *
 *  \param[in]      sAutoUpdate     true to send after every call that draws
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::setAutoUpdate(bool sAutoUpdate)
{
    AutoUpdate = sAutoUpdate;
    updateIfAuto();
} /* setAutoUpdate */


/******************************************************************************************************************************************************
  setIntensity()
******************************************************************************************************************************************************/
/*! \brief          set led intensity of matrix module
 *  \details        
 *                  
 *  \param[in]      Intensity       intensity value from 0 to 15
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setIntensity(byte Intensity)
{
    if(Intensity <= MAX7219_REG_INTENSITY_MAX_VALUE) {
        RegisterWrite(MAX7219_REG_INTENSITY_ADDRESS, Intensity);
        return E_OK;
    } else {
        return E_NOT_OK; 
    }
} /* setIntensity */


/******************************************************************************************************************************************************
  setCharWithShift()
******************************************************************************************************************************************************/
/*! \brief          shifts a char into the matrix and stops on the right side
 *  \details        this function shifts a given char into the matrix and stops shifting on the right side.
 *                  Every shift process will be triggered by calling the shiftMainFunction. 
 *                  
 *  \param[in]      Char        char to print on matrix
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setCharWithShift(char Char)
{
    if(E_OK == Text.setCharWithShift(Char)) {
        startShift(MAXMATRIX_STATE_CHAR_SHIFT);
        return E_OK;
    } else return E_NOT_OK;
} /* setCharWithShift */


/******************************************************************************************************************************************************
  setTextWithShift()
******************************************************************************************************************************************************/
/*! \brief          shifts a string into the matrix and stops after the last char on the right side
 *  \details        this function shifts a given string into the matrix and stops shifting on the right side,
 *                  when the last char appears on the matrix.
 *
 *  \param[in]      sString     string to print on matrix
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setTextWithShift(char* sString)
{
    if(E_OK == Text.setTextWithShift(sString)) {
        startShift(MAXMATRIX_STATE_STRING_SHIFT);
        return E_OK;
    } else return E_NOT_OK;
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
 *  \param[in]      ShiftState      MAXMATRIX_STATE_CHAR_SHIFT or MAXMATRIX_STATE_STRING_SHIFT
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::startShift(MaxMatrixStateType ShiftState)
{
    State = ShiftState;
    LastShift = millis() - ShiftInterval;
} /* startShift */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
