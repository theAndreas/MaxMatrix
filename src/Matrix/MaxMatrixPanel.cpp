// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as the viewer sees it.
 *
 * Columns, rows and dots in the viewer's coordinates, mapped onto the buffer as wired by the matrix
 * orientation and the module type.
 */

#include "MaxMatrixPanel.h"

MaxMatrixPanel::MaxMatrixPanel(byte data_in_pin, byte chip_select_pin, byte clock_pin, byte* buffer, byte number_of_modules)
    : wired_(data_in_pin, chip_select_pin, clock_pin, buffer, number_of_modules)
{
    module_type_ = MaxMatrixModuleType::kDr0Cr0Rr1;
    matrix_orientation_ = MaxMatrixMatrixOrientation::k0;
}

MaxMatrixResult<byte> MaxMatrixPanel::GetColumn(int16_t column)
{
    if(column >= 0 && column < GetNumberOfColumns()) return GetLine(column, 0, true);
    else return MaxMatrixError::kOutOfRange;
}

MaxMatrixResult<byte> MaxMatrixPanel::GetColumn(byte module, byte column)
{
    if(module < GetNumberOfModules() && column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        return GetColumn(static_cast<int16_t>((module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + column));
    } else return MaxMatrixError::kOutOfRange;
}

void MaxMatrixPanel::SetColumn(int16_t column, byte value)
{
    if(column >= 0 && column < GetNumberOfColumns()) SetLine(column, 0, true, value);
}

void MaxMatrixPanel::SetColumn(byte module, byte column, byte value)
{
    // The column is checked against the module, not the panel, or column 8 of module 0 would be
    // drawn on module 1.
    if(module < GetNumberOfModules() && column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        SetColumn(static_cast<int16_t>((module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + column), value);
    }
}

// The row is taken by reference rather than as a pointer: GetRow(module, 0) would otherwise be
// ambiguous, as a literal 0 is a null pointer as well.
MaxMatrixStatus MaxMatrixPanel::GetRow(int16_t row, byte* values, byte number_of_values)
{
    if(row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE && number_of_values >= GetNumberOfModules()) {
        byte const modules = GetNumberOfModules();
        for(byte module = 0; module < modules; module++) values[module] = GetRow(module, row).GetValue();
        return MaxMatrixError::kOk;
    } else return MaxMatrixError::kOutOfRange;
}

MaxMatrixResult<byte> MaxMatrixPanel::GetRow(byte module, byte row)
{
    if(module < GetNumberOfModules() && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        return ReverseByte(GetLine(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, row, false));
    } else return MaxMatrixError::kOutOfRange;
}

void MaxMatrixPanel::SetRow(int16_t row, byte const* values, byte number_of_values)
{
    if(row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        byte const modules = GetNumberOfModules() < number_of_values ? GetNumberOfModules() : number_of_values;
        for(byte module = 0; module < modules; module++) SetRow(module, row, values[module]);
    }
}

void MaxMatrixPanel::SetRow(byte module, byte row, byte value)
{
    if(module < GetNumberOfModules() && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        SetLine(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, row, false, ReverseByte(value));
    }
}

MaxMatrixResult<bool> MaxMatrixPanel::GetDot(int16_t column, int16_t row)
{
    byte buffer_column, buffer_bit;

    if(column >= 0 && column < GetNumberOfColumns() && row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
        return static_cast<bool>(bitRead(wired_.GetColumn(buffer_column), buffer_bit));
    } else return MaxMatrixError::kOutOfRange;
}

void MaxMatrixPanel::SetDot(int16_t column, int16_t row, bool value)
{
    byte buffer_column, buffer_bit;

    if(column >= 0 && column < GetNumberOfColumns() && row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
        wired_.StoreBit(buffer_column, buffer_bit, value);
    }
}

/*
 * The shifts work on the picture as the viewer sees it, a column at a time, so that they are the
 * same for every module type and orientation; in place, as a copy of the whole picture would need
 * its length at compile time.
 */
void MaxMatrixPanel::ShiftLeft(bool rotate, bool fill_with_zero)
{
    byte const last = static_cast<byte>(GetNumberOfColumns() - 1);
    byte const column_left = GetLine(0, 0, true);

    for(byte column = 0; column < last; column++) SetLine(column, 0, true, GetLine(column + 1, 0, true));
    if(rotate) SetLine(last, 0, true, column_left);
    else if(fill_with_zero) SetLine(last, 0, true, 0);
}

void MaxMatrixPanel::ShiftRight(bool rotate, bool fill_with_zero)
{
    byte const last = static_cast<byte>(GetNumberOfColumns() - 1);
    byte const column_right = GetLine(last, 0, true);

    for(byte column = last; column > 0; column--) SetLine(column, 0, true, GetLine(column - 1, 0, true));
    if(rotate) SetLine(0, 0, true, column_right);
    else if(fill_with_zero) SetLine(0, 0, true, 0);
}

void MaxMatrixPanel::ShiftUp(bool rotate)
{
    byte const columns = GetNumberOfColumns();

    for(byte column = 0; column < columns; column++) {
        byte value = GetLine(column, 0, true);
        bool const top_dot = bitRead(value, 0);
        value >>= 1;
        if(rotate) bitWrite(value, MAXMATRIX_ROW_NUMBER_OF_MODULE - 1, top_dot);
        SetLine(column, 0, true, value);
    }
}

void MaxMatrixPanel::ShiftDown(bool rotate)
{
    byte const columns = GetNumberOfColumns();

    for(byte column = 0; column < columns; column++) {
        byte value = GetLine(column, 0, true);
        bool const bottom_dot = bitRead(value, MAXMATRIX_ROW_NUMBER_OF_MODULE - 1);
        value <<= 1;
        if(rotate) bitWrite(value, 0, bottom_dot);
        SetLine(column, 0, true, value);
    }
}

void MaxMatrixPanel::SetModuleType(MaxMatrixModuleType module_type)
{
    ChangeMapping(matrix_orientation_, module_type);
}

void MaxMatrixPanel::SetMatrixOrientation(MaxMatrixMatrixOrientation matrix_orientation)
{
    ChangeMapping(matrix_orientation, module_type_);
}

/*
 * Maps the panel anew, to a matrix orientation and a module type, so that the viewer sees the same
 * picture, rewriting the buffer a pair of modules at a time: the viewer's modules m and n - 1 - m
 * lie in the buffer's modules m and n - 1 - m whatever the mapping, as the matrix orientation only
 * swaps them and the module type rearranges each in its place. So the pair is read in the old
 * mapping and written in the new without touching any other module, and 16 bytes do for any chain,
 * where a copy of the whole picture would need its length at compile time.
 */
void MaxMatrixPanel::ChangeMapping(MaxMatrixMatrixOrientation matrix_orientation, MaxMatrixModuleType module_type)
{
    MaxMatrixMatrixOrientation const old_matrix_orientation = matrix_orientation_;
    MaxMatrixModuleType const old_module_type = module_type_;

    for(int16_t left = 0, right = static_cast<int16_t>(GetNumberOfModules() - 1); left <= right; left++, right--) {
        byte const left_column = static_cast<byte>(left * MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
        byte const right_column = static_cast<byte>(right * MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
        byte pair[2 * MAXMATRIX_COLUMN_NUMBER_OF_MODULE];

        matrix_orientation_ = old_matrix_orientation;
        module_type_ = old_module_type;
        for(byte column = 0; column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; column++) {
            pair[column] = GetLine(left_column + column, 0, true);
            pair[MAXMATRIX_COLUMN_NUMBER_OF_MODULE + column] = GetLine(right_column + column, 0, true);
        }
        matrix_orientation_ = matrix_orientation;
        module_type_ = module_type;
        for(byte column = 0; column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; column++) {
            SetLine(left_column + column, 0, true, pair[column]);
            SetLine(right_column + column, 0, true, pair[MAXMATRIX_COLUMN_NUMBER_OF_MODULE + column]);
        }
    }
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
 * there. The matrix orientation turns the whole panel first; then the module type says which digit
 * register and which of its bits drive the dot within its module, as MD_MAX72XX describes a module
 * from its input on the right. A buffer column is a digit register, its bit a segment. The dot has
 * to lie within the matrix.
 *
 * It is the only function that knows the matrix orientation and the module type. While every
 * public function handled the orientations itself, rows, shifts and dots disagreed in 90, 180 and
 * 270 degrees. Three properties rather than a case for each of the eight types, so that a mirrored
 * module costs no code of its own.
 */
void MaxMatrixPanel::ConvertDotToBuffer(byte column, byte row, byte* buffer_column, byte* buffer_bit)
{
    if(matrix_orientation_ == MaxMatrixMatrixOrientation::k180) {
        column = GetNumberOfColumns() - 1 - column;
        row = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - row;
    }

    byte const module = column / MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
    byte const column_from_right = MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 - (column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    byte const type = static_cast<byte>(module_type_);
    bool const digits_as_rows = (type & MAXMATRIX_MODULE_TYPE_DIGITS_AS_ROWS) != 0;
    byte digit = digits_as_rows ? row : column_from_right;
    byte segment = digits_as_rows ? column_from_right : row;

    if((type & MAXMATRIX_MODULE_TYPE_REVERSED_ROWS) != 0) digit = MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 - digit;
    if((type & MAXMATRIX_MODULE_TYPE_REVERSED_COLUMNS) != 0) segment = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - segment;
    *buffer_column = (module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + digit;
    *buffer_bit = segment;
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
