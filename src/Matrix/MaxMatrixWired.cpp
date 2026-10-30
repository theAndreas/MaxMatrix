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
    ChangedColumns = 0;
} /* MaxMatrixWired */


/******************************************************************************************************************************************************
  storeBit()
******************************************************************************************************************************************************/
/*! \brief          set one bit of the buffer
 *  \details        this function changes the buffer alone; send() brings the change to the chain. The column is marked
 *                  as changed only when its value changes, so that drawing what is already shown sends nothing.
 *
 *  \param[in]      Column      column in the buffer
 *  \param[in]      Bit         bit in the column, the row as wired
 *  \param[in]      Value       true for a lit LED
 *  \return         -
 *  \pre            Column is within the buffer and Bit within a byte: MaxMatrixPanel has checked and mapped them
 *****************************************************************************************************************************************************/
void MaxMatrixWired::storeBit(byte Column, byte Bit, bool Value)
{
    byte ColumnValue = MatrixBuffer[Column];

    bitWrite(ColumnValue, Bit, Value);
    if(ColumnValue != MatrixBuffer[Column]) {
        MatrixBuffer[Column] = ColumnValue;
        bitSet(ChangedColumns, Column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    }
} /* storeBit */


/******************************************************************************************************************************************************
  clear()
******************************************************************************************************************************************************/
/*! \brief          turns every LED off
 *  \details        this function empties the buffer and marks every column as changed, even one that was empty
 *                  already: after power-up the registers of the MAX7219 hold anything, and init() clears them this way
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixWired::clear()
{
    for(int i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) MatrixBuffer[i] = 0;
    ChangedColumns = 0xFF;
} /* clear */


/******************************************************************************************************************************************************
  send()
******************************************************************************************************************************************************/
/*! \brief          sends what has changed
 *  \details        this function sends every changed column of a module in one frame for all modules, and nothing
 *                  when nothing has changed
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixWired::send()
{
    byte Values[MAXMATRIX_NUMBER_OF_MODULES];

    for(byte Column = 0; Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; Column++) {
        if(bitRead(ChangedColumns, Column)) {
            for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
                Values[Module] = MatrixBuffer[Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + Column];
            }
            Chain.writeRegisterEach(MAX7219_REG_DIGIT0_ADDRESS + Column, Values);
        }
    }
    ChangedColumns = 0;
} /* send */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
