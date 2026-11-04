// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as it is wired.
 *
 * The buffer mirrors what is on the LEDs, one byte per column and one bit per row, columns of all
 * modules in a row, module 0 first. Every write to the hardware updates the buffer as well, because
 * the MAX7219 cannot be read back.
 */

#include "MaxMatrixWired.h"

MaxMatrixWired::MaxMatrixWired(byte data_in_pin, byte chip_select_pin, byte clock_pin) : chain_(data_in_pin, chip_select_pin, clock_pin)
{
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) matrix_buffer_[i] = 0;
    changed_columns_ = 0;
}

void MaxMatrixWired::StoreBit(byte column, byte bit, bool value)
{
    byte column_value = matrix_buffer_[column];

    bitWrite(column_value, bit, value);
    if(column_value != matrix_buffer_[column]) {
        matrix_buffer_[column] = column_value;
        bitSet(changed_columns_, column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    }
}

void MaxMatrixWired::Clear()
{
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) matrix_buffer_[i] = 0;
    changed_columns_ = 0xFF;
}

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
}
