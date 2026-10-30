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
/**     \file       MaxMatrixPanel.cpp
 *      \brief      The matrix as the viewer sees it
 *
 *      \details    Columns, rows and dots in the viewer's coordinates, mapped onto the buffer as wired by the matrix and the module orientation.
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_PANEL_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixPanel.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrixPanel
******************************************************************************************************************************************************/
/*! \brief          MaxMatrixPanel constructor
 *  \details        Instantiation of the matrix as the viewer sees it, in orientation 0
 *
 *  \param[in]      sDataInPin         number of the Data Input Pin
 *  \param[in]      sChipSelectPin     number of the Chip Select (CS) Pin
 *  \param[in]      sClockPin          number of the Clock Pin
 *  \return         -
 *****************************************************************************************************************************************************/
MaxMatrixPanel::MaxMatrixPanel(byte sDataInPin, byte sChipSelectPin, byte sClockPin) : Wired(sDataInPin, sChipSelectPin, sClockPin)
{
    ModuleOrientation = MAXMATRIX_MODULE_ORIENTATION_0;
    MatrixOrientation = MAXMATRIX_MATRIX_ORIENTATION_0;
} /* MaxMatrixPanel */


/******************************************************************************************************************************************************
  getColumn()
******************************************************************************************************************************************************/
/*! \brief          get a given column of the matrix
 *  \details        this function returns a column of the LED matrix as the viewer sees it
 *                  column reaches from zero to number of modules multiplied by eight
 *
 *  \param[in]      Column      column on matrix which contains eight LEDs
 *  \param[out]     Value       led value in column, bit 0 is the top row
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::getColumn(byte Column, byte* Value)
{
    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS) {
        *Value = getLine(Column, 0, true);
        return E_OK;
    } else return E_NOT_OK;
} /* getColumn */


/******************************************************************************************************************************************************
  getColumn()
******************************************************************************************************************************************************/
/*! \brief          get a given column of a module
 *  \details        this function returns a column of a module as the viewer sees it
 *
 *  \param[in]      Module      module where column will be returned
 *  \param[in]      Column      column on module which contains eight LEDs
 *  \param[out]     Value       led value in column, bit 0 is the top row
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::getColumn(byte Module, byte Column, byte* Value)
{
    if(Module < MAXMATRIX_NUMBER_OF_MODULES && Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        return getColumn((Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + Column, Value);
    } else return E_NOT_OK;
} /* getColumn */


/******************************************************************************************************************************************************
  setColumn()
******************************************************************************************************************************************************/
/*! \brief          set a given column on whole matrix
 *  \details        this function sets a column on the LED matrix as the viewer sees it
 *                  column reaches from zero to number of modules multiplied by eight
 *
 *  \param[in]      Column      column on matrix which contains eight LEDs
 *  \param[in]      Value       led value in column, bit 0 is the top row
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::setColumn(byte Column, byte Value)
{
    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS) {
        setLine(Column, 0, true, Value);
        return E_OK;
    } else return E_NOT_OK;
} /* setColumn */


/******************************************************************************************************************************************************
  setColumn()
******************************************************************************************************************************************************/
/*! \brief          set a given column on a module
 *  \details        this function sets a column on a module as the viewer sees it
 *
 *  \param[in]      Module      module where column should be set
 *  \param[in]      Column      column on module which contains eight LEDs
 *  \param[in]      Value       led value in column, bit 0 is the top row
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::setColumn(byte Module, byte Column, byte Value)
{
    if(Module < MAXMATRIX_NUMBER_OF_MODULES && Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        return setColumn((Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + Column, Value);
    } else return E_NOT_OK;
} /* setColumn */


/******************************************************************************************************************************************************
  getRow()
******************************************************************************************************************************************************/
/*! \brief          get a given row of the matrix
 *  \details        this function returns a row of the LED matrix as the viewer sees it, one byte per module
 *
 *  \param[in]      Row         row on matrix
 *  \param[out]     Value       led value in row, bit 7 of each byte is the leftmost column of its module
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::getRow(byte Row, rowType Value)
{
    if(Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) getRow(Module, Row, &Value[Module]);
        return E_OK;
    } else return E_NOT_OK;
} /* getRow */


/******************************************************************************************************************************************************
  getRow()
******************************************************************************************************************************************************/
/*! \brief          get a given row of a module
 *  \details        this function returns a row of a module as the viewer sees it. The leftmost column is in
 *                  bit 7, so that a row value written as a binary literal looks like the dots it stands for.
 *
 *  \param[in]      Module      module where row should be returned
 *  \param[in]      Row         row on module which contains eight LEDs
 *  \param[out]     Value       led value in row, bit 7 is the leftmost column
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::getRow(byte Module, byte Row, byte* Value)
{
    if(Module < MAXMATRIX_NUMBER_OF_MODULES && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        *Value = reverseByte(getLine(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, Row, false));
        return E_OK;
    } else return E_NOT_OK;
} /* getRow */


/******************************************************************************************************************************************************
  setRow()
******************************************************************************************************************************************************/
/*! \brief          set a given row on whole matrix
 *  \details        this function sets a row on the LED matrix as the viewer sees it, one byte per module
 *
 *  \param[in]      Row         row on matrix
 *  \param[in]      Value       led value in row, bit 7 of each byte is the leftmost column of its module
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::setRow(byte Row, const rowType Value)
{
    if(Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) setRow(Module, Row, Value[Module]);
        return E_OK;
    } else return E_NOT_OK;
} /* setRow */


/******************************************************************************************************************************************************
  setRow()
******************************************************************************************************************************************************/
/*! \brief          set a given row on a module
 *  \details        this function sets a row on a module as the viewer sees it
 *
 *  \param[in]      Module      module where row should be set
 *  \param[in]      Row         row on module which contains eight LEDs
 *  \param[in]      Value       led value in row, bit 7 is the leftmost column
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::setRow(byte Module, byte Row, byte Value)
{
    if(Module < MAXMATRIX_NUMBER_OF_MODULES && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        setLine(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, Row, false, reverseByte(Value));
        return E_OK;
    } else return E_NOT_OK;
} /* setRow */


/******************************************************************************************************************************************************
  getDot()
******************************************************************************************************************************************************/
/*! \brief          get a Pixel (LED) on the matrix
 *  \details        this function returns a dot as the viewer sees it
 *
 *  \param[in]      Column      column where to get the pixel
 *  \param[in]      Row         row where to get the pixel
 *  \param[out]     Value       value of the pixel
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::getDot(byte Column, byte Row, bool* Value)
{
    byte BufferColumn, BufferBit;

    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        convertDotToBuffer(Column, Row, &BufferColumn, &BufferBit);
        *Value = bitRead(Wired.getColumn(BufferColumn), BufferBit);
        return E_OK;
    } else return E_NOT_OK;
} /* getDot */


/******************************************************************************************************************************************************
  setDot()
******************************************************************************************************************************************************/
/*! \brief          set a Pixel (LED) on the matrix
 *  \details        this function sets a dot as the viewer sees it
 *
 *  \param[in]      Column      column where to set the pixel
 *  \param[in]      Row         row where to set the pixel
 *  \param[in]      Value       value of the pixel
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixPanel::setDot(byte Column, byte Row, bool Value)
{
    byte BufferColumn, BufferBit;

    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        convertDotToBuffer(Column, Row, &BufferColumn, &BufferBit);
        Wired.storeBit(BufferColumn, BufferBit, Value);
        return E_OK;
    } else return E_NOT_OK;
} /* setDot */

/******************************************************************************************************************************************************
  shiftLeft()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one column to the left
 *  \details        this functions shifts the states of all LEDs on the matrix one column to the left.
 *                  By activating fill with zero, the column on the right side will be initialized with LED off.
 *                  By activating rotation, the last column will be first.
 *
 *  \param[in]      Rotate          activate rotation of the matrix LED states
 *  \param[in]      FillWithZero    initialize the column on the right side with zero.
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::shiftLeft(bool Rotate, bool FillWithZero)
{
    byte Picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    getPicture(Picture);
    byte ColumnLeft = Picture[0];
    for(int Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS - 1; Column++) Picture[Column] = Picture[Column + 1];
    if(Rotate) Picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1] = ColumnLeft;
    else if(FillWithZero) Picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1] = 0;
    setPicture(Picture);
} /* shiftLeft */


/******************************************************************************************************************************************************
  shiftRight()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one column to the right
 *  \details        this functions shifts the states of all LEDs on the matrix one column to the right.
 *                  By activating fill with zero, the column on the left side will be initialized with LED off.
 *                  By activating rotation, the last column will be first.
 *
 *  \param[in]      Rotate          activate rotation of the matrix LED states
 *  \param[in]      FillWithZero    initialize the column on the left side with zero.
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::shiftRight(bool Rotate, bool FillWithZero)
{
    byte Picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    getPicture(Picture);
    byte ColumnRight = Picture[MAXMATRIX_NUMBER_OF_COLUMNS - 1];
    for(int Column = MAXMATRIX_NUMBER_OF_COLUMNS - 1; Column > 0; Column--) Picture[Column] = Picture[Column - 1];
    if(Rotate) Picture[0] = ColumnRight;
    else if(FillWithZero) Picture[0] = 0;
    setPicture(Picture);
} /* shiftRight */


/******************************************************************************************************************************************************
  shiftUp()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one row up to the top
 *  \details        this function shifts the states of all LEDS on the matrix one row up to the top.
 *                  The row on the bottom will be initialized with LED off.
 *                  By activating rotation, the last row will be first.
 *
 *  \param[in]      Rotate          activate rotation of the matrix LED states
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::shiftUp(bool Rotate)
{
    byte Picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    getPicture(Picture);
    for(int Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        bool TopDot = bitRead(Picture[Column], 0);
        Picture[Column] >>= 1;
        if(Rotate) bitWrite(Picture[Column], MAXMATRIX_ROW_NUMBER_OF_MODULE - 1, TopDot);
    }
    setPicture(Picture);
} /* shiftUp */


/******************************************************************************************************************************************************
  shiftDown()
******************************************************************************************************************************************************/
/*! \brief          shift the whole matrix led states one row down to the bottom
 *  \details        this function shifts the states of all LEDS on the matrix one row down to the bottom.
 *                  The row on the top will be initialized with LED off.
 *                  By activating rotation, the last row will be first.
 *
 *  \param[in]      Rotate          activate rotation of the matrix LED states
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::shiftDown(bool Rotate)
{
    byte Picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    getPicture(Picture);
    for(int Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        bool BottomDot = bitRead(Picture[Column], MAXMATRIX_ROW_NUMBER_OF_MODULE - 1);
        Picture[Column] <<= 1;
        if(Rotate) bitWrite(Picture[Column], 0, BottomDot);
    }
    setPicture(Picture);
} /* shiftDown */

/******************************************************************************************************************************************************
  setModuleOrientation()
******************************************************************************************************************************************************/
/*! \brief          set the orientation of every module
 *  \details        this function turns every module in its place by 0, 90, 180 or 270 degrees, as its LEDs are
 *                  mounted. The picture stays as the viewer sees it; the buffer is turned along.
 *
 *  \param[in]      sModuleOrientation      orientation in degree
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::setModuleOrientation(MaxMatrixModuleOrientationType sModuleOrientation)
{
    byte Picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    getPicture(Picture);
    ModuleOrientation = sModuleOrientation;
    setPicture(Picture);
} /* setModuleOrientation */


/******************************************************************************************************************************************************
  setMatrixOrientation()
******************************************************************************************************************************************************/
/*! \brief          set the orientation of the whole matrix
 *  \details        this function turns the whole matrix by 0 or 180 degrees, for a board mounted upside down:
 *                  the module at the input is then on the left. It turns the modules in addition to the module
 *                  orientation. The picture stays as the viewer sees it; the buffer is turned along.
 *
 *  \param[in]      sMatrixOrientation      orientation in degree
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::setMatrixOrientation(MaxMatrixMatrixOrientationType sMatrixOrientation)
{
    byte Picture[MAXMATRIX_NUMBER_OF_COLUMNS];

    getPicture(Picture);
    MatrixOrientation = sMatrixOrientation;
    setPicture(Picture);
} /* setMatrixOrientation */

/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  reverseByte()
******************************************************************************************************************************************************/
/*! \brief          reverse a given byte
 *  \details        this function returns a reversed byte from a given byte
 *                  
 *  \param[in]      Input           byte which should be reversed
 *  \return         reversed Byte
 *****************************************************************************************************************************************************/
byte MaxMatrixPanel::reverseByte(byte Input)
{
   Input = (Input & 0xF0) >> 4 | (Input & 0x0F) << 4;
   Input = (Input & 0xCC) >> 2 | (Input & 0x33) << 2;
   Input = (Input & 0xAA) >> 1 | (Input & 0x55) << 1;
   
   return Input;
} /* reverseByte */

/******************************************************************************************************************************************************
  convertDotToBuffer()
******************************************************************************************************************************************************/
/*! \brief          find a dot in the matrix buffer
 *  \details        this function returns where a dot as the viewer sees it is kept: its column in the matrix
 *                  buffer and its bit there. The matrix orientation turns the whole panel first, the module
 *                  orientation then every module in its place.
 *                  It is the only function that knows the orientations. While every public function handled them
 *                  itself, rows, shifts and dots disagreed in 90, 180 and 270 degrees.
 *
 *  \param[in]      Column          column of the dot as the viewer sees it
 *  \param[in]      Row             row of the dot as the viewer sees it
 *  \param[out]     BufferColumn    column in the matrix buffer
 *  \param[out]     BufferBit       bit in that column
 *  \return         -
 *  \pre            Column and Row are within the matrix
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::convertDotToBuffer(byte Column, byte Row, byte* BufferColumn, byte* BufferBit)
{
    if(MatrixOrientation == MAXMATRIX_MATRIX_ORIENTATION_180) {
        Column = MAXMATRIX_NUMBER_OF_COLUMNS - 1 - Column;
        Row = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - Row;
    }

    byte Module = Column / MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
    byte ModuleColumn = Column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
    byte WiredColumn, WiredRow;

    if(ModuleOrientation == MAXMATRIX_MODULE_ORIENTATION_90) {
        WiredColumn = MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 - Row;
        WiredRow = ModuleColumn;
    } else if(ModuleOrientation == MAXMATRIX_MODULE_ORIENTATION_180) {
        WiredColumn = MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 - ModuleColumn;
        WiredRow = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - Row;
    } else if(ModuleOrientation == MAXMATRIX_MODULE_ORIENTATION_270) {
        WiredColumn = Row;
        WiredRow = MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 - ModuleColumn;
    } else {
        WiredColumn = ModuleColumn;
        WiredRow = Row;
    }
    *BufferColumn = (Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + WiredColumn;
    *BufferBit = WiredRow;
} /* convertDotToBuffer */


/******************************************************************************************************************************************************
  getLine()
******************************************************************************************************************************************************/
/*! \brief          get eight dots in a line
 *  \details        this function returns eight dots as the viewer sees them, from Column, Row on downwards or
 *                  to the right
 *
 *  \param[in]      Column      column of the first dot
 *  \param[in]      Row         row of the first dot
 *  \param[in]      Vertical    downwards rather than to the right
 *  \return         the dots, the first one in bit 0
 *  \pre            the line is within the matrix
 *****************************************************************************************************************************************************/
byte MaxMatrixPanel::getLine(byte Column, byte Row, bool Vertical)
{
    byte BufferColumn, BufferBit;
    byte Value = 0;

    for(byte Dot = 0; Dot < 8; Dot++) {
        convertDotToBuffer(Vertical ? Column : Column + Dot, Vertical ? Row + Dot : Row, &BufferColumn, &BufferBit);
        bitWrite(Value, Dot, bitRead(Wired.getColumn(BufferColumn), BufferBit));
    }
    return Value;
} /* getLine */


/******************************************************************************************************************************************************
  setLine()
******************************************************************************************************************************************************/
/*! \brief          set eight dots in a line
 *  \details        this function sets eight dots as the viewer sees them, from Column, Row on downwards or to
 *                  the right
 *
 *  \param[in]      Column      column of the first dot
 *  \param[in]      Row         row of the first dot
 *  \param[in]      Vertical    downwards rather than to the right
 *  \param[in]      Value       the dots, the first one in bit 0
 *  \return         -
 *  \pre            the line is within one module
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::setLine(byte Column, byte Row, bool Vertical, byte Value)
{
    byte BufferColumn, BufferBit;

    for(byte Dot = 0; Dot < 8; Dot++) {
        convertDotToBuffer(Vertical ? Column : Column + Dot, Vertical ? Row + Dot : Row, &BufferColumn, &BufferBit);
        Wired.storeBit(BufferColumn, BufferBit, bitRead(Value, Dot));
    }
} /* setLine */


/******************************************************************************************************************************************************
  getPicture()
******************************************************************************************************************************************************/
/*! \brief          get the whole picture
 *  \details        this function returns the picture as the viewer sees it, one byte per column with the top row
 *                  in bit 0. Shifting and turning work on this picture rather than on the buffer, so that they
 *                  are the same in every orientation.
 *
 *  \param[out]     Picture     one byte per column of the matrix
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::getPicture(byte* Picture)
{
    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) Picture[Column] = getLine(Column, 0, true);
} /* getPicture */


/******************************************************************************************************************************************************
  setPicture()
******************************************************************************************************************************************************/
/*! \brief          set the whole picture
 *  \details        this function writes a picture as the viewer sees it into the buffer
 *
 *  \param[in]      Picture     one byte per column of the matrix, the top row in bit 0
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixPanel::setPicture(const byte* Picture)
{
    byte BufferColumn, BufferBit;

    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
            convertDotToBuffer(Column, Row, &BufferColumn, &BufferBit);
            Wired.storeBit(BufferColumn, BufferBit, bitRead(Picture[Column], Row));
        }
    }
} /* setPicture */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
