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
 *****************************************************************************************************************************************************/
MaxMatrixWired::MaxMatrixWired(byte data_in_pin, byte chip_select_pin, byte clock_pin) : chain_(data_in_pin, chip_select_pin, clock_pin)
{
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) matrix_buffer_[i] = 0;
    changed_columns_ = 0;
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
 *  \pre            Column is within the buffer and Bit within a byte: MaxMatrixPanel has checked and mapped them
 *****************************************************************************************************************************************************/
void MaxMatrixWired::StoreBit(byte column, byte bit, bool value)
{
    byte column_value = matrix_buffer_[column];

    bitWrite(column_value, bit, value);
    if(column_value != matrix_buffer_[column]) {
        matrix_buffer_[column] = column_value;
        bitSet(changed_columns_, column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    }
} /* storeBit */


/******************************************************************************************************************************************************
  clear()
******************************************************************************************************************************************************/
/*! \brief          turns every LED off
 *  \details        this function empties the buffer and marks every column as changed, even one that was empty
 *                  already: after power-up the registers of the MAX7219 hold anything, and init() clears them this way
 *****************************************************************************************************************************************************/
void MaxMatrixWired::Clear()
{
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) matrix_buffer_[i] = 0;
    changed_columns_ = 0xFF;
} /* clear */


/******************************************************************************************************************************************************
  send()
******************************************************************************************************************************************************/
/*! \brief          sends what has changed
 *  \details        this function sends every changed column of a module in one frame for all modules, and nothing
 *                  when nothing has changed
 *****************************************************************************************************************************************************/
void MaxMatrixWired::Send()
{
    byte values[MAXMATRIX_NUMBER_OF_MODULES];

    for(byte column = 0; column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; column++) {
        if(bitRead(changed_columns_, column)) {
            for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) {
                values[module] = matrix_buffer_[module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + column];
            }
            chain_.WriteRegisterEach(MAX7219_REG_DIGIT0_ADDRESS + column, values);
        }
    }
    changed_columns_ = 0;
} /* send */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
