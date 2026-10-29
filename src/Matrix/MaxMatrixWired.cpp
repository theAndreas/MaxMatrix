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
/**     \file       MaxMatrixWired.cpp
 *      \brief      The matrix as it is wired
 *
 *      \details    The buffer mirrors what is on the LEDs, one byte per column and one bit per row, columns of all
 *                  modules in a row, module 0 first. Every write to the hardware updates the buffer as well, because
 *                  the MAX7219 cannot be read back.
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_WIRED_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixWired.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrixWired
******************************************************************************************************************************************************/
/*! \brief          MaxMatrixWired constructor
 *  \details        Instantiation of the matrix as it is wired, with an empty buffer
 *
 *  \param[in]      sDataInPin         number of the Data Input Pin
 *  \param[in]      sChipSelectPin     number of the Chip Select (CS) Pin
 *  \param[in]      sClockPin          number of the Clock Pin
 *  \return         -
 *****************************************************************************************************************************************************/
MaxMatrixWired::MaxMatrixWired(byte sDataInPin, byte sChipSelectPin, byte sClockPin) : Chain(sDataInPin, sChipSelectPin, sClockPin)
{
    for(int i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) MatrixBuffer[i] = 0;
} /* MaxMatrixWired */


/******************************************************************************************************************************************************
  setColumn()
******************************************************************************************************************************************************/
/*! \brief          set a given column as wired
 *  \details        this function sets a column of the buffer and sends it to its module
 *                  column reaches from zero to number of modules multiplied by eight - 1
 *
 *  \param[in]      Column      column in the buffer
 *  \param[in]      Value       led value in column each bit means one led
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixWired::setColumn(byte Column, byte Value)
{
    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS) {
        byte Module = Column / MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
        byte ModuleColumn = Column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
        Chain.writeRegister(Module, MAX7219_REG_DIGIT0_ADDRESS + ModuleColumn, Value);
        MatrixBuffer[Column] = Value;
        return E_OK;
    } else {
        return E_NOT_OK;
    }
} /* setColumn */


/******************************************************************************************************************************************************
  setColumnOnAllModules()
******************************************************************************************************************************************************/
/*! \brief          set a given column on all matrix modules
 *  \details        this function sets the same column on every module with the given value, in one frame
 *
 *  \param[in]      Column      column on a module, from zero to seven
 *  \param[in]      Value       led value in column each bit means one LED
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixWired::setColumnOnAllModules(byte Column, byte Value)
{
    if(Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        Chain.writeRegisterAll(MAX7219_REG_DIGIT0_ADDRESS + Column, Value);
        for(int i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) MatrixBuffer[(i * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + Column] = Value;
        return E_OK;
    } else {
        return E_NOT_OK;
    }
} /* setColumnOnAllModules */


/******************************************************************************************************************************************************
  clear()
******************************************************************************************************************************************************/
/*! \brief          clears all LEDs of matrix module
 *  \details        this function turns every LED off, in eight frames, and empties the buffer
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixWired::clear()
{
    /* clear all LEDs on Matrix */
    for(int i = 0; i < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; i++) { setColumnOnAllModules(i, 0); }
    /* clear Matrix buffer */
    for(int i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) { MatrixBuffer[i] = 0; }
} /* clear */


/******************************************************************************************************************************************************
  reload()
******************************************************************************************************************************************************/
/*! \brief          reload whole matrix
 *  \details        this function sends the whole buffer, one frame per column of a module
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixWired::reload()
{
    byte Values[MAXMATRIX_NUMBER_OF_MODULES];

    for(int i = 0; i < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; i++)
    {
        for(int j = 0; j < MAXMATRIX_NUMBER_OF_MODULES; j++) Values[j] = MatrixBuffer[(j * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + i];
        Chain.writeRegisterEach(MAX7219_REG_DIGIT0_ADDRESS + i, Values);
    }
} /* reload */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
