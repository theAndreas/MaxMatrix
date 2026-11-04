// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as the viewer sees it.
 *
 * Columns, rows and dots in the viewer's coordinates, mapped onto the buffer as wired by the matrix
 * and the module orientation.
 */

#include "MaxMatrixPanel.h"

MaxMatrixPanel::MaxMatrixPanel(byte data_in_pin, byte chip_select_pin, byte clock_pin) : wired_(data_in_pin, chip_select_pin, clock_pin)
{
    module_orientation_ = MaxMatrixModuleOrientation::k0;
    matrix_orientation_ = MaxMatrixMatrixOrientation::k0;
}

MaxMatrixResult<byte> MaxMatrixPanel::GetColumn(int16_t column)
{
    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS) return GetLine(column, 0, true);
    else return MaxMatrixError::kOutOfRange;
}

MaxMatrixResult<byte> MaxMatrixPanel::GetColumn(byte module, byte column)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        return GetColumn(static_cast<int16_t>((module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + column));
    } else return MaxMatrixError::kOutOfRange;
}

void MaxMatrixPanel::SetColumn(int16_t column, byte value)
{
    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS) SetLine(column, 0, true, value);
}

void MaxMatrixPanel::SetColumn(byte module, byte column, byte value)
{
    // The column is checked against the module, not the panel, or column 8 of module 0 would be
    // drawn on module 1.
    if(module < MAXMATRIX_NUMBER_OF_MODULES && column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        SetColumn(static_cast<int16_t>((module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + column), value);
    }
}

// The row is taken by reference rather than as a pointer: GetRow(module, 0) would otherwise be
// ambiguous, as a literal 0 is a null pointer as well.
MaxMatrixStatus MaxMatrixPanel::GetRow(int16_t row, MaxMatrixRow& value)
{
    if(row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) value[module] = GetRow(module, row).GetValue();
        return MaxMatrixError::kOk;
    } else return MaxMatrixError::kOutOfRange;
}

MaxMatrixResult<byte> MaxMatrixPanel::GetRow(byte module, byte row)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        return ReverseByte(GetLine(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, row, false));
    } else return MaxMatrixError::kOutOfRange;
}

void MaxMatrixPanel::SetRow(int16_t row, MaxMatrixRow const value)
{
    if(row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) SetRow(module, row, value[module]);
    }
}

void MaxMatrixPanel::SetRow(byte module, byte row, byte value)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        SetLine(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, row, false, ReverseByte(value));
    }
}

MaxMatrixResult<bool> MaxMatrixPanel::GetDot(int16_t column, int16_t row)
{
    byte buffer_column, buffer_bit;

    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS && row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
        return static_cast<bool>(bitRead(wired_.GetColumn(buffer_column), buffer_bit));
    } else return MaxMatrixError::kOutOfRange;
}

void MaxMatrixPanel::SetDot(int16_t column, int16_t row, bool value)
{
    byte buffer_column, buffer_bit;

    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS && row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
        wired_.StoreBit(buffer_column, buffer_bit, value);
    }
}

void MaxMatrixPanel::ShiftLeft(bool rotate, bool fill_with_zero)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    byte column_left = picture[0];
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS - 1; column++) picture[column] = picture[column + 1];
    if(rotate) picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1] = column_left;
    else if(fill_with_zero) picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1] = 0;
    SetPicture(picture);
}

void MaxMatrixPanel::ShiftRight(bool rotate, bool fill_with_zero)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    byte column_right = picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1];
    for(byte column = MAXMATRIX_NUMBER_OF_COLUMNS - 1; column > 0; column--) picture[column] = picture[column - 1];
    if(rotate) picture[0] = column_right;
    else if(fill_with_zero) picture[0] = 0;
    SetPicture(picture);
}

void MaxMatrixPanel::ShiftUp(bool rotate)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
        bool top_dot = bitRead(picture[column], 0);
        picture[column] >>= 1;
        if(rotate) bitWrite(picture[column], MAXMATRIX_ROW_NUMBER_OF_MODULE - 1, top_dot);
    }
    SetPicture(picture);
}

void MaxMatrixPanel::ShiftDown(bool rotate)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
        bool bottom_dot = bitRead(picture[column], MAXMATRIX_ROW_NUMBER_OF_MODULE - 1);
        picture[column] <<= 1;
        if(rotate) bitWrite(picture[column], 0, bottom_dot);
    }
    SetPicture(picture);
}

void MaxMatrixPanel::SetModuleOrientation(MaxMatrixModuleOrientation module_orientation)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    module_orientation_ = module_orientation;
    SetPicture(picture);
}

void MaxMatrixPanel::SetMatrixOrientation(MaxMatrixMatrixOrientation matrix_orientation)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    matrix_orientation_ = matrix_orientation;
    SetPicture(picture);
}

// Returns `input` with its bits in the opposite order.
byte MaxMatrixPanel::ReverseByte(byte input)
{
   input = (input & 0xF0) >> 4 | (input & 0x0F) << 4;
   input = (input & 0xCC) >> 2 | (input & 0x33) << 2;
   input = (input & 0xAA) >> 1 | (input & 0x55) << 1;
   
   return input;
}

/*
 * Finds where a dot as the viewer sees it is kept: its column in the matrix buffer and its bit
 * there. The matrix orientation turns the whole panel first, the module orientation then every
 * module in its place. The dot has to lie within the matrix.
 *
 * It is the only function that knows the orientations. While every public function handled them
 * itself, rows, shifts and dots disagreed in 90, 180 and 270 degrees.
 */
void MaxMatrixPanel::ConvertDotToBuffer(byte column, byte row, byte* buffer_column, byte* buffer_bit)
{
    if(matrix_orientation_ == MaxMatrixMatrixOrientation::k180) {
        column = MAXMATRIX_NUMBER_OF_COLUMNS - 1 - column;
        row = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - row;
    }

    byte module = column / MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
    byte module_column = column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
    byte wired_column, wired_row;

    if(module_orientation_ == MaxMatrixModuleOrientation::k90) {
        wired_column = MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 - row;
        wired_row = module_column;
    } else if(module_orientation_ == MaxMatrixModuleOrientation::k180) {
        wired_column = MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 - module_column;
        wired_row = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - row;
    } else if(module_orientation_ == MaxMatrixModuleOrientation::k270) {
        wired_column = row;
        wired_row = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - module_column;
    } else {
        wired_column = module_column;
        wired_row = row;
    }
    *buffer_column = (module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + wired_column;
    *buffer_bit = wired_row;
}

/*
 * Returns eight dots as the viewer sees them, from `column`, `row` on to the right, or downwards
 * if `vertical`; the first in bit 0. The line has to lie within the matrix.
 */
byte MaxMatrixPanel::GetLine(byte column, byte row, bool vertical)
{
    byte buffer_column, buffer_bit;
    byte value = 0;

    for(byte dot = 0; dot < 8; dot++) {
        ConvertDotToBuffer(vertical ? column : column + dot, vertical ? row + dot : row, &buffer_column, &buffer_bit);
        bitWrite(value, dot, bitRead(wired_.GetColumn(buffer_column), buffer_bit));
    }
    return value;
}

/*
 * Sets eight dots as the viewer sees them, from `column`, `row` on to the right, or downwards if
 * `vertical`; the first in bit 0 of `value`. The line has to lie within one module.
 */
void MaxMatrixPanel::SetLine(byte column, byte row, bool vertical, byte value)
{
    byte buffer_column, buffer_bit;

    for(byte dot = 0; dot < 8; dot++) {
        ConvertDotToBuffer(vertical ? column : column + dot, vertical ? row + dot : row, &buffer_column, &buffer_bit);
        wired_.StoreBit(buffer_column, buffer_bit, bitRead(value, dot));
    }
}

/*
 * Gives the picture as the viewer sees it, one byte per column, the top row in bit 0. Shifting
 * and turning work on this picture rather than on the buffer, so that they are the same in every
 * orientation.
 */
void MaxMatrixPanel::GetPicture(byte* picture)
{
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) picture[column] = GetLine(column, 0, true);
}

// Writes a picture as the viewer sees it, one byte per column, the top row in bit 0, into the
// buffer.
void MaxMatrixPanel::SetPicture(byte const* picture)
{
    byte buffer_column, buffer_bit;

    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
        for(byte row = 0; row < MAXMATRIX_ROW_NUMBER_OF_MODULE; row++) {
            ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
            wired_.StoreBit(buffer_column, buffer_bit, bitRead(picture[column], row));
        }
    }
}
