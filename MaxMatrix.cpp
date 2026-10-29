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
/**     \file       MaxMatrix.c
 *      \brief      Main file of MaxMatrix library
 *
 *      \details    Arduino library to drive 8x8 LED display Matrix modules with MAX7219
 *                  
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrix.h"
#if(MAXMATRIX_USE_SPI == STD_ON)
    #include "SPI.h"
#endif
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    #include "digitalWriteFast.h"
#endif


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
MaxMatrix::MaxMatrix(byte sDataInPin, byte sChipSelectPin, byte sClockPin)
{
    DataInPin = sDataInPin;
    ChipSelectPin = sChipSelectPin;
    ClockPin = sClockPin;
    String = NULL;
    SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_READY;
    SpaceBetweenChars = MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE;
    ModuleOrientation = MAXMATRIX_MODULE_ORIENTATION_0;
    MatrixOrientation = MAXMATRIX_MATRIX_ORIENTATION_0;
    State = MAXMATRIX_STATE_NONE;
    
    /* initialize buffers */
    for (int i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) MatrixBuffer[i] = 0;
    for (int i = 0; i < MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS; i++) SpriteBuffer[i] = 0;
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
 *                  all LEDs of the matrix will be cleared and intensity will be set to init value
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::init()
{
    State = MAXMATRIX_STATE_INIT;

    pinMode(ChipSelectPin, OUTPUT);
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ChipSelectPin, HIGH);
#else
    digitalWrite(ChipSelectPin, HIGH);
#endif
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.begin();
#else
    pinMode(DataInPin, OUTPUT);
    pinMode(ClockPin, OUTPUT);
    /* clock idles low, MAX7219 samples data on the rising edge */
# if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ClockPin, LOW);
# else
    digitalWrite(ClockPin, LOW);
# endif
#endif
    
    RegisterWrite(MAX7219_REG_SCAN_LIMIT_ADDRESS, MAX7219_REG_SCAN_LIMIT_DISPLAY_DIGIT_0_TO_7);     // display all digits  
    RegisterWrite(MAX7219_REG_DECODE_MODE_ADDRESS, MAX7219_REG_DECODE_MODE_NO_DECODE);              // using an led matrix (not digits)
    RegisterWrite(MAX7219_REG_SHUTDOWN_ADDRESS, MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);        // normal operation mode
    RegisterWrite(MAX7219_REG_DISPLAY_TEST_ADDRESS, MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);     // no display test
    
    /* initialize registers, turn all LEDs off */
    clear();
    
    setIntensity(MAXMATRIX_INTENSITY_INIT_VALUE);
    State = MAXMATRIX_STATE_READY;
} /* init */


/******************************************************************************************************************************************************
  shiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts char or string over the matrix
 *  \details        this function has to be called periodically until state change from 
 *                  MAXMATRIX_STATE_CHAR_SHIFT or MAXMATRIX_STATE_STRING_SHIFT to MAXMATRIX_STATE_READY
 *  \return         -
 *  \pre            setCharWithShift or setTextWithShift must be called first
 *****************************************************************************************************************************************************/
void MaxMatrix::shiftTask()
{
    if(MAXMATRIX_STATE_STRING_SHIFT == State) stringShiftTask();
    if(MAXMATRIX_STATE_CHAR_SHIFT == State) charShiftTask();
} /* shiftTask */


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
  clear()
******************************************************************************************************************************************************/
/*! \brief          clears all LEDs of matrix module
 *  \details        
 *                  
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::clear()
{
    /* clear all LEDs on Matrix */
    for(int i = 0; i < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; i++) { setColumnOnAllModulesLL(i, 0); }
    /* clear Matrix buffer */
    for(int i = 0; i < MAXMATRIX_NUMBER_OF_COLUMNS; i++) { MatrixBuffer[i] = 0; }
} /* clear */


/******************************************************************************************************************************************************
  RegisterWrite()
******************************************************************************************************************************************************/
/*! \brief          send SPI command to MAX7219
 *  \details        
 *                  
 *  \param[in]      RegisterAddress     Address of the MAX7219 Register
 *  \param[in]      Value               Register Value
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::RegisterWrite(byte RegisterAddress, byte RegisterValue)
{
    startTransmission();
    for(int Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++)
    {
#if(MAXMATRIX_USE_SPI == STD_ON)
        SPI.transfer(RegisterAddress);
        SPI.transfer(RegisterValue);
#else
        shiftOut(DataInPin, ClockPin, MSBFIRST, RegisterAddress);
        shiftOut(DataInPin, ClockPin, MSBFIRST, RegisterValue);
#endif
    }
    stopTransmission();
} /* RegisterWrite */


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
stdReturnType MaxMatrix::getColumn(byte Column, byte* Value)
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
stdReturnType MaxMatrix::getColumn(byte Module, byte Column, byte* Value)
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
stdReturnType MaxMatrix::setColumn(byte Column, byte Value)
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
stdReturnType MaxMatrix::setColumn(byte Module, byte Column, byte Value)
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
stdReturnType MaxMatrix::getRow(byte Row, rowType Value)
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
stdReturnType MaxMatrix::getRow(byte Module, byte Row, byte* Value)
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
stdReturnType MaxMatrix::setRow(byte Row, const rowType Value)
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
stdReturnType MaxMatrix::setRow(byte Module, byte Row, byte Value)
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
stdReturnType MaxMatrix::getDot(byte Column, byte Row, bool* Value)
{
    byte BufferColumn, BufferBit;

    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        convertDotToBuffer(Column, Row, &BufferColumn, &BufferBit);
        *Value = bitRead(MatrixBuffer[BufferColumn], BufferBit);
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
stdReturnType MaxMatrix::setDot(byte Column, byte Row, bool Value)
{
    byte BufferColumn, BufferBit;

    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE) {
        convertDotToBuffer(Column, Row, &BufferColumn, &BufferBit);
        byte ColumnValue = MatrixBuffer[BufferColumn];
        bitWrite(ColumnValue, BufferBit, Value);
        return setColumnLL(BufferColumn, ColumnValue);
    } else return E_NOT_OK;
} /* setDot */


/******************************************************************************************************************************************************
  setChar()
******************************************************************************************************************************************************/
/*! \brief          prints a char on the matrix
 *  \details        
 *                  
 *  \param[in]      X           x coordinate
 *  \param[in]      Y           y coordinate
 *  \param[in]      Char        char to print on matrix
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setChar(int X, int Y, char Char)
{   
    spriteIndexType SpriteIndex;
    spriteType Sprite;
    
    if(E_OK == convertCharToSprite(Char, &SpriteIndex)) {
        if(E_OK == getSprite(SpriteIndex, &Sprite)) {
            setSprite(X, Y, Sprite);
            return E_OK;
        } else return E_NOT_OK;
    } else return E_NOT_OK;
} /* setChar */


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
    spriteIndexType SpriteIndex;
    
    if(E_OK == convertCharToSprite(Char, &SpriteIndex)) {
        if(E_OK == getSprite(SpriteIndex, &SpriteBuffer)) {
            SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_RUNNING;
            State = MAXMATRIX_STATE_CHAR_SHIFT;
            return E_OK;
        } else return E_NOT_OK;
    } else return E_NOT_OK;
} /* setCharWithShift */


/******************************************************************************************************************************************************
  setText()
******************************************************************************************************************************************************/
/*! \brief          prints a string on the matrix
 *  \details        
 *                  
 *  \param[in]      String        string to print on matrix
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setText(const char* String)
{   
    spriteIndexType SpriteIndex;
    spriteType Sprite;
    int CharColumn = 0;
    char Char;

    /* check for valid string */
    if(String != NULL)
    {
        while(*String != STD_NULL_CHARACTER)
        {
            String += decodeChar(String, &Char);
            if(E_NOT_OK == convertCharToSprite(Char, &SpriteIndex)) { return E_NOT_OK; }
            else if(E_NOT_OK == getSprite(SpriteIndex, &Sprite)) { return E_NOT_OK; }
            else {
                setSprite(CharColumn, 0, Sprite);
                CharColumn += Sprite[ASCII_TABLE_SPRITE_WIDTH] + SpaceBetweenChars;
                if(CharColumn > MAXMATRIX_NUMBER_OF_COLUMNS) break;
            }
        }
        return E_OK;
    } else {
        return E_NOT_OK;
    }
} /* setText */


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
    if(sString != NULL) {
        SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_READY;
        String = sString;
        State = MAXMATRIX_STATE_STRING_SHIFT;
        return E_OK;
    } else return E_NOT_OK;
} /* setTextWithShift */


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
void MaxMatrix::shiftLeft(bool Rotate, bool FillWithZero)
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
void MaxMatrix::shiftRight(bool Rotate, bool FillWithZero)
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
void MaxMatrix::shiftUp(bool Rotate)
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
void MaxMatrix::shiftDown(bool Rotate)
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
  getSprite()
******************************************************************************************************************************************************/
/*! \brief          get an sprite from the sprite table
 *  \details        this function returns an sprite by an given sprite index
 *                  
 *  \param[in]      SpriteIndex     index of sprite from sprite table
 *  \param[out]     Sprite          sprite from sprite table (width, height, value column 1 ... 5)
 *  \return         -
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::getSprite(spriteIndexType SpriteIndex, spriteType* Sprite)
{
    if(SpriteIndex < MAXMATRIX_SPRITE_TABLE_NUMBER_OF_ROWS) {
        memcpy_P(*Sprite, SpriteTable + MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS * SpriteIndex, MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS);
        return E_OK;
    } else return E_NOT_OK;
} /* getSprite */


/******************************************************************************************************************************************************
  setSprite()
******************************************************************************************************************************************************/
/*! \brief          print sprite on matrix
 *  \details        this function prints an sprite on the matrix located to the given coordinates
 *                  
 *  \param[in]      X           x coordinate
 *  \param[in]      Y           y coordinate
 *  \param[in]      Sprite      sprite from sprite table (width, height, value column 1 ... 5)
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::setSprite(int X, int Y, const spriteType Sprite)
{
    int SpriteWidth = Sprite[ASCII_TABLE_SPRITE_WIDTH];
    int SpriteHeight = Sprite[ASCII_TABLE_SPRITE_HEIGHT];
    
    /* if height of Sprite = height of Matrix set whole column */
    if(SpriteHeight == MAXMATRIX_ROW_NUMBER_OF_MODULE && Y == 0) {
        for(int SpriteColumn = 0; SpriteColumn < SpriteWidth; SpriteColumn++)
        {
            int Column = X + SpriteColumn;
            if(Column >= 0 && Column < MAXMATRIX_NUMBER_OF_COLUMNS)
                setColumn(Column, Sprite[SpriteColumn + ASCII_TABLE_SPRITE_COLUMN1]);
        } 
    } else { /* otherwise we have to set every single dot */
        for(int SpriteColumn = 0; SpriteColumn < SpriteWidth; SpriteColumn++)
        {
            for(int SpriteRow = 0; SpriteRow < SpriteHeight; SpriteRow++)
            {
                int Column = X + SpriteColumn;
                int Row = Y + SpriteRow;
                if(Column >= 0 && Column < MAXMATRIX_NUMBER_OF_COLUMNS && Row >= 0 && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE)
                    setDot(Column, Row, bitRead(Sprite[SpriteColumn + ASCII_TABLE_SPRITE_COLUMN1], SpriteRow));
            }
        }
    }
} /* setSprite */


/******************************************************************************************************************************************************
  setSpaceBetweenChars()
******************************************************************************************************************************************************/
/*! \brief          set space between chars
 *  \details        this function sets the number of spaces between chars
 *                  
 *  \param[in]      sSpaceBetweenChars      number of spaces
 *  \return         E_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setSpaceBetweenChars(byte sSpaceBetweenChars)
{
    /* every byte value is a valid number of spaces */
    SpaceBetweenChars = sSpaceBetweenChars;
    return E_OK;
} /* setSpaceBetweenChars */


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
void MaxMatrix::setModuleOrientation(MaxMatrixModuleOrientationType sModuleOrientation)
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
void MaxMatrix::setMatrixOrientation(MaxMatrixMatrixOrientationType sMatrixOrientation)
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
byte MaxMatrix::reverseByte(byte Input)
{
   Input = (Input & 0xF0) >> 4 | (Input & 0x0F) << 4;
   Input = (Input & 0xCC) >> 2 | (Input & 0x33) << 2;
   Input = (Input & 0xAA) >> 1 | (Input & 0x55) << 1;
   
   return Input;
} /* reverseByte */


/******************************************************************************************************************************************************
  startTransmission()
******************************************************************************************************************************************************/
/*! \brief          start transmission to MAX7219
 *  \details        this function pulls chip select low, in SPI mode the SPI bus will be reserved too
 *                  
 *  \return         -
 *  \post           stopTransmission() has to be called after data was transferred
 *****************************************************************************************************************************************************/
void MaxMatrix::startTransmission()
{
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.beginTransaction(SPISettings(MAXMATRIX_SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE0));
#endif
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ChipSelectPin, LOW);
#else
    digitalWrite(ChipSelectPin, LOW);
#endif
} /* startTransmission */


/******************************************************************************************************************************************************
  stopTransmission()
******************************************************************************************************************************************************/
/*! \brief          stop transmission to MAX7219
 *  \details        this function pulls chip select high, so the MAX7219 latches the transferred data.
 *                  In SPI mode the SPI bus will be released for other devices.
 *                  
 *  \return         -
 *  \pre            startTransmission() has to be called first
 *****************************************************************************************************************************************************/
void MaxMatrix::stopTransmission()
{
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ChipSelectPin, HIGH);
#else
    digitalWrite(ChipSelectPin, HIGH);
#endif
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.endTransaction();
#endif
} /* stopTransmission */


/******************************************************************************************************************************************************
  reload()
******************************************************************************************************************************************************/
/*! \brief          reload whole matrix
 *  \details        this function reloads the whole matrix with the matrix buffer values
 *                  
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::reload()
{
    for(int i = 0; i < MAXMATRIX_COLUMN_NUMBER_OF_MODULE; i++)
    {
        int Column = i;
        startTransmission();
        for(int j = 0; j < MAXMATRIX_NUMBER_OF_MODULES; j++)
        {
#if(MAXMATRIX_USE_SPI == STD_ON)
            SPI.transfer(i + 1);
            SPI.transfer(MatrixBuffer[Column]);
#else
            shiftOut(DataInPin, ClockPin, MSBFIRST, i + 1);
            shiftOut(DataInPin, ClockPin, MSBFIRST, MatrixBuffer[Column]);
#endif
            Column += MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
        }
        stopTransmission();
    }
} /* reload */


/******************************************************************************************************************************************************
  charShiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts char one column to the left
 *  \details        this function has to be called periodically until state change from 
 *                  MAXMATRIX_STATE_CHAR_SHIFT or MAXMATRIX_STATE_STRING_SHIFT to MAXMATRIX_STATE_READY
 *  \return         -
 *  \pre            has to be called from shiftTask()
 *****************************************************************************************************************************************************/
void MaxMatrix::charShiftTask()
{
    shiftLeft(false, true);
    SpriteShiftCounter++;
    
    /* if sprite is completed set only spaces */
    if(SpriteShiftCounter <= SpriteBuffer[ASCII_TABLE_SPRITE_WIDTH]) {
        setSprite(MAXMATRIX_NUMBER_OF_COLUMNS - SpriteShiftCounter, 0, SpriteBuffer);
    }
    /* if sprite and spaces are completed task has finished */
    else if(SpriteShiftCounter == SpriteBuffer[ASCII_TABLE_SPRITE_WIDTH] + SpaceBetweenChars + 1) {
        if(MAXMATRIX_STATE_CHAR_SHIFT == State) State = MAXMATRIX_STATE_READY;
        SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_READY;
    }
} /* charShiftTask */


/******************************************************************************************************************************************************
  stringShiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts string one column to the left
 *  \details        this function has to be called periodically until state change from
 *                  MAXMATRIX_STATE_CHAR_SHIFT or MAXMATRIX_STATE_STRING_SHIFT to MAXMATRIX_STATE_READY
 *  \return         -
 *  \pre            has to be called from shiftTask()
 *****************************************************************************************************************************************************/
void MaxMatrix::stringShiftTask()
{
    spriteIndexType SpriteIndex;
    char Char;

    /* has char shift task finished shifting sprite or char */
    if(SpriteShiftCounter == MAXMATRIX_SPRITE_SHIFT_STATE_READY)
    {
        /* If end of string is not reached, load next char */
        if(*String != STD_NULL_CHARACTER) {
            String += decodeChar(String, &Char);
            /* unknown chars will be skipped */
            if(E_OK == convertCharToSprite(Char, &SpriteIndex) && E_OK == getSprite(SpriteIndex, &SpriteBuffer)) {
                SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_RUNNING;
                charShiftTask();
            }
        } else { /* otherwise task has finished */
            State = MAXMATRIX_STATE_READY;
            shiftLeft(false, true);
        }
    } else {
        /* go on shifting Sprite */
        charShiftTask();
    }
} /* stringShiftTask */


/******************************************************************************************************************************************************
  convertCharToSprite()
******************************************************************************************************************************************************/
/*! \brief          converts char to sprite
 *  \details        this function converts the ASCII char to sprite index
 *                  
 *  \param[in]      Char            char to convert to sprite
 *  \param[out]     SpriteIndex     appropriate sprite
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::convertCharToSprite(char Char, spriteIndexType* SpriteIndex)
{
    stdReturnType ReturnValue = E_NOT_OK;

    /* for umlauts we need a special treatment, compare with latin-1 codes to be independent of source file encoding */
    if(0xC4 == (byte)Char) { *SpriteIndex = 95; ReturnValue = E_OK; }          // Ä
    else if(0xD6 == (byte)Char) { *SpriteIndex = 96; ReturnValue = E_OK; }     // Ö
    else if(0xDC == (byte)Char) { *SpriteIndex = 97; ReturnValue = E_OK; }     // Ü
    else if(0xE4 == (byte)Char) { *SpriteIndex = 98; ReturnValue = E_OK; }     // ä
    else if(0xF6 == (byte)Char) { *SpriteIndex = 99; ReturnValue = E_OK; }     // ö
    else if(0xFC == (byte)Char) { *SpriteIndex = 100; ReturnValue = E_OK; }    // ü
    /* for all others only add offset */
    else if(Char >= MAXMATRIX_ASCII_CHAR_MIN && Char <= MAXMATRIX_ASCII_CHAR_MAX)
        { *SpriteIndex = Char + MAXMATRIX_ASCII_TABLE_OFFSET; ReturnValue = E_OK; }

    return ReturnValue;
} /* convertCharToSprite */


/******************************************************************************************************************************************************
  decodeChar()
******************************************************************************************************************************************************/
/*! \brief          decodes the next char of a string
 *  \details        this function decodes UTF-8 sequences of latin-1 chars (e.g. umlauts) to a single latin-1 char,
 *                  all other bytes are returned unchanged
 *
 *  \param[in]      String          string where the next char should be decoded, must not point to the null character
 *  \param[out]     Char            decoded char
 *  \return         number of bytes of the decoded char
 *****************************************************************************************************************************************************/
byte MaxMatrix::decodeChar(const char* String, char* Char)
{
    byte LeadByte = String[0];
    byte NextByte = String[1];

    /* two byte UTF-8 sequence of a char from U+0080 to U+00FF */
    if((0xC2 == LeadByte || 0xC3 == LeadByte) && 0x80 == (NextByte & 0xC0)) {
        *Char = ((LeadByte & 0x03) << 6) | (NextByte & 0x3F);
        return 2;
    } else {
        *Char = LeadByte;
        return 1;
    }
} /* decodeChar */


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
void MaxMatrix::convertDotToBuffer(byte Column, byte Row, byte* BufferColumn, byte* BufferBit)
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
byte MaxMatrix::getLine(byte Column, byte Row, bool Vertical)
{
    byte BufferColumn, BufferBit;
    byte Value = 0;

    for(byte Dot = 0; Dot < 8; Dot++) {
        convertDotToBuffer(Vertical ? Column : Column + Dot, Vertical ? Row + Dot : Row, &BufferColumn, &BufferBit);
        bitWrite(Value, Dot, bitRead(MatrixBuffer[BufferColumn], BufferBit));
    }
    return Value;
} /* getLine */


/******************************************************************************************************************************************************
  setLine()
******************************************************************************************************************************************************/
/*! \brief          set eight dots in a line
 *  \details        this function sets eight dots as the viewer sees them, from Column, Row on downwards or to
 *                  the right, and sends every buffer column it changed once. Along a line within a module the
 *                  buffer column either stays the same or changes with every dot, so sending it whenever it
 *                  changes sends one frame where the line is a buffer column and eight where it is a buffer row.
 *
 *  \param[in]      Column      column of the first dot
 *  \param[in]      Row         row of the first dot
 *  \param[in]      Vertical    downwards rather than to the right
 *  \param[in]      Value       the dots, the first one in bit 0
 *  \return         -
 *  \pre            the line is within one module
 *****************************************************************************************************************************************************/
void MaxMatrix::setLine(byte Column, byte Row, bool Vertical, byte Value)
{
    byte BufferColumn, BufferBit;
    byte SentColumn = MAXMATRIX_NUMBER_OF_COLUMNS;

    for(byte Dot = 0; Dot < 8; Dot++) {
        convertDotToBuffer(Vertical ? Column : Column + Dot, Vertical ? Row + Dot : Row, &BufferColumn, &BufferBit);
        bitWrite(MatrixBuffer[BufferColumn], BufferBit, bitRead(Value, Dot));
    }
    for(byte Dot = 0; Dot < 8; Dot++) {
        convertDotToBuffer(Vertical ? Column : Column + Dot, Vertical ? Row + Dot : Row, &BufferColumn, &BufferBit);
        if(BufferColumn != SentColumn) {
            setColumnLL(BufferColumn, MatrixBuffer[BufferColumn]);
            SentColumn = BufferColumn;
        }
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
void MaxMatrix::getPicture(byte* Picture)
{
    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) Picture[Column] = getLine(Column, 0, true);
} /* getPicture */


/******************************************************************************************************************************************************
  setPicture()
******************************************************************************************************************************************************/
/*! \brief          set the whole picture
 *  \details        this function writes a picture as the viewer sees it into the buffer and sends the buffer
 *
 *  \param[in]      Picture     one byte per column of the matrix, the top row in bit 0
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrix::setPicture(const byte* Picture)
{
    byte BufferColumn, BufferBit;

    for(byte Column = 0; Column < MAXMATRIX_NUMBER_OF_COLUMNS; Column++) {
        for(byte Row = 0; Row < MAXMATRIX_ROW_NUMBER_OF_MODULE; Row++) {
            convertDotToBuffer(Column, Row, &BufferColumn, &BufferBit);
            bitWrite(MatrixBuffer[BufferColumn], BufferBit, bitRead(Picture[Column], Row));
        }
    }
    reload();
} /* setPicture */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S   L O W   L E V E L
 *****************************************************************************************************************************************************/
/*!  \details       low level functions do not take care of the orientation of the Matrix modules
 *                  So the high level function has to call the right low level functions depending
                    on the orientation of the matrix modules
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  setColumnLL()
******************************************************************************************************************************************************/
/*! \brief          set a given column on matrix
 *  \details        this function sets a column on the LED matrix with the given values 
 *                  column reaches from zero to number of modules multiplied by eight - 1
 *
 *  \param[in]      Column      column on matrix which contains eight LEDs
 *  \param[in]      Value       led value in column each bit means one led
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setColumnLL(byte Column, byte Value)
{
    if(Column < MAXMATRIX_NUMBER_OF_COLUMNS) {
        int Module = Column / MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
        int ModuleColumn = Column % MAXMATRIX_COLUMN_NUMBER_OF_MODULE;
        startTransmission();
        for(int i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++)
        {
            if (i == Module) {
#if(MAXMATRIX_USE_SPI == STD_ON)
                SPI.transfer(ModuleColumn + 1);
                SPI.transfer(Value);
#else
                shiftOut(DataInPin, ClockPin, MSBFIRST,  ModuleColumn + 1);
                shiftOut(DataInPin, ClockPin, MSBFIRST, Value);
#endif
            } else {
#if(MAXMATRIX_USE_SPI == STD_ON)
                SPI.transfer(MAX7219_REG_NO_OP_ADDRESS);
                SPI.transfer(0);
#else
                shiftOut(DataInPin, ClockPin, MSBFIRST, MAX7219_REG_NO_OP_ADDRESS);
                shiftOut(DataInPin, ClockPin, MSBFIRST, 0);
#endif
            }
        }
        stopTransmission();
        MatrixBuffer[Column] = Value;
        return E_OK;
    } else {
        return E_NOT_OK;
    }
} /* setColumnLL */


/******************************************************************************************************************************************************
  setColumnOnAllModulesLL()
******************************************************************************************************************************************************/
/*! \brief          set a given column on all matrix modules
 *  \details        this function sets a column on all LED matrix modules with the given values
 *                  column reaches from zero to column number of a module
 *
 *  \param[in]      Column      column on matrix which contains eight LEDs
 *  \param[in]      Value       led value in column each bit means one LED
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrix::setColumnOnAllModulesLL(byte Column, byte Value)
{
    if(Column < MAXMATRIX_COLUMN_NUMBER_OF_MODULE) {
        startTransmission();
        for(int i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++)
        {
#if(MAXMATRIX_USE_SPI == STD_ON)
            SPI.transfer(Column + 1);
            SPI.transfer(Value);
#else
            shiftOut(DataInPin, ClockPin, MSBFIRST, Column + 1);
            shiftOut(DataInPin, ClockPin, MSBFIRST, Value);
#endif
            MatrixBuffer[(i * MAXMATRIX_COLUMN_NUMBER_OF_MODULE) + Column] = Value;
        }
        stopTransmission();
        return E_OK;
    } else {
        return E_NOT_OK;
    }
} /* setColumnOnAllModulesLL */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
