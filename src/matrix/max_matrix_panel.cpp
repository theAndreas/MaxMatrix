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
/**     \file       max_matrix_panel.cpp
 *      \brief      The matrix as the viewer sees it
 *
 *      \details    Columns, rows and dots in the viewer's coordinates, mapped onto the buffer as wired by the matrix and the module orientation.
 *
 *****************************************************************************************************************************************************/
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "max_matrix_panel.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrixPanel
******************************************************************************************************************************************************/
/*! \brief          MaxMatrixPanel constructor
 *  \details        Instantiation of the matrix as the viewer sees it, in orientation 0
 *
 *  \param[in]      data_in_pin        number of the Data Input Pin
 *  \param[in]      chip_select_pin    number of the Chip Select (CS) Pin
 *  \param[in]      clock_pin          number of the Clock Pin
 *****************************************************************************************************************************************************/
MaxMatrixPanel::MaxMatrixPanel(byte data_in_pin, byte chip_select_pin, byte clock_pin) : wired_(data_in_pin, chip_select_pin, clock_pin)
{
    module_orientation_ = MaxMatrixModuleOrientation::k0;
    matrix_orientation_ = MaxMatrixMatrixOrientation::k0;
} /* MaxMatrixPanel */


/******************************************************************************************************************************************************
  GetColumn()
******************************************************************************************************************************************************/
/*! \brief          get a given column of the matrix
 *  \details        this function returns a column of the LED matrix as the viewer sees it
 *                  column reaches from zero to number of modules multiplied by eight
 *
 *  \param[in]      column      column on matrix which contains eight LEDs
 *  \return         led value in column, bit 0 is the top row
 *                  MaxMatrixError::kOutOfRange    the column is off the panel
 *****************************************************************************************************************************************************/
MaxMatrixResult<byte> MaxMatrixPanel::GetColumn(int16_t column)
{
    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS) return GetLine(column, 0, true);
    else return MaxMatrixError::kOutOfRange;
} /* GetColumn */


/******************************************************************************************************************************************************
  GetColumn()
******************************************************************************************************************************************************/
/*! \brief          get a given column of a module
 *  \details        this function returns a column of a module as the viewer sees it
 *
 *  \param[in]      module      module where column will be returned
 *  \param[in]      column      column on module which contains eight LEDs
 *  \return         led value in column, bit 0 is the top row
 *                  MaxMatrixError::kOutOfRange    the module or the column does not exist
 *****************************************************************************************************************************************************/
MaxMatrixResult<byte> MaxMatrixPanel::GetColumn(byte module, byte column)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        return GetColumn(static_cast<int16_t>((module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + column));
    } else return MaxMatrixError::kOutOfRange;
} /* GetColumn */


/******************************************************************************************************************************************************
  SetColumn()
******************************************************************************************************************************************************/
/*! \brief          set a given column on whole matrix
 *  \details        this function sets a column on the LED matrix as the viewer sees it
 *                  column reaches from zero to number of modules multiplied by eight; a column off the panel is
 *                  left out, as SetDot() explains
 *
 *  \param[in]      column      column on matrix which contains eight LEDs
 *  \param[in]      value       led value in column, bit 0 is the top row
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetColumn(int16_t column, byte value)
{
    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS) SetLine(column, 0, true, value);
} /* SetColumn */


/******************************************************************************************************************************************************
  SetColumn()
******************************************************************************************************************************************************/
/*! \brief          set a given column on a module
 *  \details        this function sets a column on a module as the viewer sees it; a module or column that does
 *                  not exist is left out. The column is checked against the module, not the panel, or column 8 of
 *                  module 0 would be drawn on module 1.
 *
 *  \param[in]      module      module where column should be set
 *  \param[in]      column      column on module which contains eight LEDs
 *  \param[in]      value       led value in column, bit 0 is the top row
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetColumn(byte module, byte column, byte value)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        SetColumn(static_cast<int16_t>((module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + column), value);
    }
} /* SetColumn */


/******************************************************************************************************************************************************
  GetRow()
******************************************************************************************************************************************************/
/*! \brief          get a given row of the matrix
 *  \details        this function returns a row of the LED matrix as the viewer sees it, one byte per module.
 *                  The row is taken by reference rather than as a pointer: GetRow(Module, 0) would otherwise be
 *                  ambiguous, as a literal 0 is a null pointer as well.
 *
 *  \param[in]      row         row on matrix
 *  \param[out]     value       led value in row, bit 7 of each byte is the leftmost column of its module
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kOutOfRange    the row is off the panel; Value is left as it was
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrixPanel::GetRow(int16_t row, MaxMatrixRow& value)
{
    if(row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) value[module] = GetRow(module, row).GetValue();
        return MaxMatrixError::kOk;
    } else return MaxMatrixError::kOutOfRange;
} /* GetRow */


/******************************************************************************************************************************************************
  GetRow()
******************************************************************************************************************************************************/
/*! \brief          get a given row of a module
 *  \details        this function returns a row of a module as the viewer sees it. The leftmost column is in
 *                  bit 7, so that a row value written as a binary literal looks like the dots it stands for.
 *
 *  \param[in]      module      module where row should be returned
 *  \param[in]      row         row on module which contains eight LEDs
 *  \return         led value in row, bit 7 is the leftmost column
 *                  MaxMatrixError::kOutOfRange    the module or the row does not exist
 *****************************************************************************************************************************************************/
MaxMatrixResult<byte> MaxMatrixPanel::GetRow(byte module, byte row)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        return ReverseByte(GetLine(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, row, false));
    } else return MaxMatrixError::kOutOfRange;
} /* GetRow */


/******************************************************************************************************************************************************
  SetRow()
******************************************************************************************************************************************************/
/*! \brief          set a given row on whole matrix
 *  \details        this function sets a row on the LED matrix as the viewer sees it, one byte per module; a row
 *                  off the panel is left out, as SetDot() explains
 *
 *  \param[in]      row         row on matrix
 *  \param[in]      value       led value in row, bit 7 of each byte is the leftmost column of its module
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetRow(int16_t row, MaxMatrixRow const value)
{
    if(row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        for(byte module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) SetRow(module, row, value[module]);
    }
} /* SetRow */


/******************************************************************************************************************************************************
  SetRow()
******************************************************************************************************************************************************/
/*! \brief          set a given row on a module
 *  \details        this function sets a row on a module as the viewer sees it; a module or row that does not
 *                  exist is left out
 *
 *  \param[in]      module      module where row should be set
 *  \param[in]      row         row on module which contains eight LEDs
 *  \param[in]      value       led value in row, bit 7 is the leftmost column
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetRow(byte module, byte row, byte value)
{
    if(module < MAXMATRIX_NUMBER_OF_MODULES && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        SetLine(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, row, false, ReverseByte(value));
    }
} /* SetRow */


/******************************************************************************************************************************************************
  GetDot()
******************************************************************************************************************************************************/
/*! \brief          get a Pixel (LED) on the matrix
 *  \details        this function returns a dot as the viewer sees it
 *
 *  \param[in]      column      column where to get the pixel, negative to the left of the panel
 *  \param[in]      row         row where to get the pixel, negative above the panel
 *  \return         value of the pixel
 *                  MaxMatrixError::kOutOfRange    the dot is off the panel
 *****************************************************************************************************************************************************/
MaxMatrixResult<bool> MaxMatrixPanel::GetDot(int16_t column, int16_t row)
{
    byte buffer_column, buffer_bit;

    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS && row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
        return static_cast<bool>(bitRead(wired_.GetColumn(buffer_column), buffer_bit));
    } else return MaxMatrixError::kOutOfRange;
} /* GetDot */


/******************************************************************************************************************************************************
  SetDot()
******************************************************************************************************************************************************/
/*! \brief          set a Pixel (LED) on the matrix
 *  \details        this function sets a dot as the viewer sees it. A dot off the panel is left out rather than
 *                  reported: a sprite, a line or a text that lies partly off the panel is drawn every day while
 *                  something moves in or out, and a sketch would only have to clip it itself. Adafruit GFX's
 *                  drawPixel() and Rust's embedded-graphics do the same; SetChar() always did.
 *
 *  \param[in]      column      column where to set the pixel, negative to the left of the panel
 *  \param[in]      row         row where to set the pixel, negative above the panel
 *  \param[in]      value       value of the pixel
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetDot(int16_t column, int16_t row, bool value)
{
    byte buffer_column, buffer_bit;

    if(column >= 0 && column < MAXMATRIX_NUMBER_OF_COLUMNS && row >= 0 && row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
        wired_.StoreBit(buffer_column, buffer_bit, value);
    }
} /* SetDot */

/******************************************************************************************************************************************************
  ShiftLeft()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one column to the left
 *  \details        this functions shifts the states of all LEDs on the matrix one column to the left.
 *                  By activating fill with zero, the column on the right side will be initialized with LED off.
 *                  By activating rotation, the last column will be first.
 *
 *  \param[in]      rotate          activate rotation of the matrix LED states
 *  \param[in]      fill_with_zero  initialize the column on the right side with zero.
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::ShiftLeft(bool rotate, bool fill_with_zero)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    byte column_left = picture[0];
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS - 1; column++) picture[column] = picture[column + 1];
    if(rotate) picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1] = column_left;
    else if(fill_with_zero) picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1] = 0;
    SetPicture(picture);
} /* ShiftLeft */


/******************************************************************************************************************************************************
  ShiftRight()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one column to the right
 *  \details        this functions shifts the states of all LEDs on the matrix one column to the right.
 *                  By activating fill with zero, the column on the left side will be initialized with LED off.
 *                  By activating rotation, the last column will be first.
 *
 *  \param[in]      rotate          activate rotation of the matrix LED states
 *  \param[in]      fill_with_zero  initialize the column on the left side with zero.
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::ShiftRight(bool rotate, bool fill_with_zero)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    byte column_right = picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1];
    for(byte column = MAXMATRIX_NUMBER_OF_COLUMNS - 1; column > 0; column--) picture[column] = picture[column - 1];
    if(rotate) picture[0] = column_right;
    else if(fill_with_zero) picture[0] = 0;
    SetPicture(picture);
} /* ShiftRight */


/******************************************************************************************************************************************************
  ShiftUp()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one row up to the top
 *  \details        this function shifts the states of all LEDS on the matrix one row up to the top.
 *                  The row on the bottom will be initialized with LED off.
 *                  By activating rotation, the last row will be first.
 *
 *  \param[in]      rotate          activate rotation of the matrix LED states
 *****************************************************************************************************************************************************/
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
} /* ShiftUp */


/******************************************************************************************************************************************************
  ShiftDown()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one row down to the bottom
 *  \details        this function shifts the states of all LEDS on the matrix one row down to the bottom.
 *                  The row on the top will be initialized with LED off.
 *                  By activating rotation, the last row will be first.
 *
 *  \param[in]      rotate          activate rotation of the matrix LED states
 *****************************************************************************************************************************************************/
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
} /* ShiftDown */

/******************************************************************************************************************************************************
  SetModuleOrientation()
******************************************************************************************************************************************************/
/*! \brief          set the orientation of every module
 *  \details        this function turns every module in its place by 0, 90, 180 or 270 degrees, as its LEDs are
 *                  mounted. The picture stays as the viewer sees it; the buffer is turned along.
 *
 *  \param[in]      module_orientation      orientation in degree
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetModuleOrientation(MaxMatrixModuleOrientation module_orientation)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    module_orientation_ = module_orientation;
    SetPicture(picture);
} /* SetModuleOrientation */


/******************************************************************************************************************************************************
  SetMatrixOrientation()
******************************************************************************************************************************************************/
/*! \brief          set the orientation of the whole matrix
 *  \details        this function turns the whole matrix by 0 or 180 degrees, for a board mounted upside down:
 *                  the module at the input is then on the left. It turns the modules in addition to the module
 *                  orientation. The picture stays as the viewer sees it; the buffer is turned along.
 *
 *  \param[in]      matrix_orientation      orientation in degree
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetMatrixOrientation(MaxMatrixMatrixOrientation matrix_orientation)
{
    byte picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    GetPicture(picture);
    matrix_orientation_ = matrix_orientation;
    SetPicture(picture);
} /* SetMatrixOrientation */

/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  ReverseByte()
******************************************************************************************************************************************************/
/*! \brief          reverse a given byte
 *  \details        this function returns a reversed byte from a given byte
 *                  
 *  \param[in]      input           byte which should be reversed
 *  \return         reversed Byte
 *****************************************************************************************************************************************************/
byte MaxMatrixPanel::ReverseByte(byte input)
{
   input = (input & 0xF0) >> 4 | (input & 0x0F) << 4;
   input = (input & 0xCC) >> 2 | (input & 0x33) << 2;
   input = (input & 0xAA) >> 1 | (input & 0x55) << 1;
   
   return input;
} /* ReverseByte */

/******************************************************************************************************************************************************
  ConvertDotToBuffer()
******************************************************************************************************************************************************/
/*! \brief          find a dot in the matrix buffer
 *  \details        this function returns where a dot as the viewer sees it is kept: its column in the matrix
 *                  buffer and its bit there. The matrix orientation turns the whole panel first, the module
 *                  orientation then every module in its place.
 *                  It is the only function that knows the orientations. While every public function handled them
 *                  itself, rows, shifts and dots disagreed in 90, 180 and 270 degrees.
 *
 *  \param[in]      column          column of the dot as the viewer sees it
 *  \param[in]      row             row of the dot as the viewer sees it
 *  \param[out]     buffer_column   column in the matrix buffer
 *  \param[out]     buffer_bit      bit in that column
 *  \pre            Column and Row are within the matrix
 *****************************************************************************************************************************************************/
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
} /* ConvertDotToBuffer */


/******************************************************************************************************************************************************
  GetLine()
******************************************************************************************************************************************************/
/*! \brief          get eight dots in a line
 *  \details        this function returns eight dots as the viewer sees them, from Column, Row on downwards or
 *                  to the right
 *
 *  \param[in]      column      column of the first dot
 *  \param[in]      row         row of the first dot
 *  \param[in]      vertical    downwards rather than to the right
 *  \return         the dots, the first one in bit 0
 *  \pre            the line is within the matrix
 *****************************************************************************************************************************************************/
byte MaxMatrixPanel::GetLine(byte column, byte row, bool vertical)
{
    byte buffer_column, buffer_bit;
    byte value = 0;

    for(byte dot = 0; dot < 8; dot++) {
        ConvertDotToBuffer(vertical ? column : column + dot, vertical ? row + dot : row, &buffer_column, &buffer_bit);
        bitWrite(value, dot, bitRead(wired_.GetColumn(buffer_column), buffer_bit));
    }
    return value;
} /* GetLine */


/******************************************************************************************************************************************************
  SetLine()
******************************************************************************************************************************************************/
/*! \brief          set eight dots in a line
 *  \details        this function sets eight dots as the viewer sees them, from Column, Row on downwards or to
 *                  the right
 *
 *  \param[in]      column      column of the first dot
 *  \param[in]      row         row of the first dot
 *  \param[in]      vertical    downwards rather than to the right
 *  \param[in]      value       the dots, the first one in bit 0
 *  \pre            the line is within one module
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetLine(byte column, byte row, bool vertical, byte value)
{
    byte buffer_column, buffer_bit;

    for(byte dot = 0; dot < 8; dot++) {
        ConvertDotToBuffer(vertical ? column : column + dot, vertical ? row + dot : row, &buffer_column, &buffer_bit);
        wired_.StoreBit(buffer_column, buffer_bit, bitRead(value, dot));
    }
} /* SetLine */


/******************************************************************************************************************************************************
  GetPicture()
******************************************************************************************************************************************************/
/*! \brief          get the whole picture
 *  \details        this function returns the picture as the viewer sees it, one byte per column with the top row
 *                  in bit 0. Shifting and turning work on this picture rather than on the buffer, so that they
 *                  are the same in every orientation.
 *
 *  \param[out]     picture     one byte per column of the matrix
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::GetPicture(byte* picture)
{
    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) picture[column] = GetLine(column, 0, true);
} /* GetPicture */


/******************************************************************************************************************************************************
  SetPicture()
******************************************************************************************************************************************************/
/*! \brief          set the whole picture
 *  \details        this function writes a picture as the viewer sees it into the buffer
 *
 *  \param[in]      picture     one byte per column of the matrix, the top row in bit 0
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::SetPicture(byte const* picture)
{
    byte buffer_column, buffer_bit;

    for(byte column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
        for(byte row = 0; row < MAXMATRIX_ROW_NUMBER_OF_MODULE; row++) {
            ConvertDotToBuffer(column, row, &buffer_column, &buffer_bit);
            wired_.StoreBit(buffer_column, buffer_bit, bitRead(picture[column], row));
        }
    }
} /* SetPicture */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
