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
/**     \file       MaxMatrixText.cpp
 *      \brief      Text on the matrix
 *
 *      \details    Chars and strings on the matrix, standing or shifting in from the right.
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_TEXT_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixText.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrixText
******************************************************************************************************************************************************/
/*! \brief          MaxMatrixText constructor
 *  \details        Instantiation of the text on a matrix
 *
 *  \return         -
 *****************************************************************************************************************************************************/
MaxMatrixText::MaxMatrixText()
{
    String = NULL;
    SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_READY;
    SpaceBetweenChars = MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE;
    for (int i = 0; i < MAXMATRIX_SPRITE_TABLE_NUMBER_OF_COLUMNS; i++) SpriteBuffer[i] = 0;
} /* MaxMatrixText */


/******************************************************************************************************************************************************
  setChar()
******************************************************************************************************************************************************/
/*! \brief          prints a char on the matrix
 *  \details        
 *                  
 *  \param[in]      Panel       the matrix the text is drawn on
 *  \param[in]      X           x coordinate
 *  \param[in]      Y           y coordinate
 *  \param[in]      Char        char to print on matrix
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixText::setChar(MaxMatrixPanel& Panel, int X, int Y, char Char)
{   
    spriteIndexType SpriteIndex;
    spriteType Sprite;
    
    if(E_OK == MaxMatrixFont::convertCharToSprite(Char, &SpriteIndex)) {
        if(E_OK == MaxMatrixFont::getSprite(SpriteIndex, &Sprite)) {
            setSprite(Panel, X, Y, Sprite);
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
stdReturnType MaxMatrixText::setCharWithShift(char Char)
{
    spriteIndexType SpriteIndex;
    
    if(E_OK == MaxMatrixFont::convertCharToSprite(Char, &SpriteIndex)) {
        if(E_OK == MaxMatrixFont::getSprite(SpriteIndex, &SpriteBuffer)) {
            SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_RUNNING;
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
 *  \param[in]      Panel       the matrix the text is drawn on
 *  \param[in]      String        string to print on matrix
 *  \return         E_OK
 *                  E_NOT_OK
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixText::setText(MaxMatrixPanel& Panel, const char* String)
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
            String += MaxMatrixFont::decodeChar(String, &Char);
            if(E_NOT_OK == MaxMatrixFont::convertCharToSprite(Char, &SpriteIndex)) { return E_NOT_OK; }
            else if(E_NOT_OK == MaxMatrixFont::getSprite(SpriteIndex, &Sprite)) { return E_NOT_OK; }
            else {
                setSprite(Panel, CharColumn, 0, Sprite);
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
stdReturnType MaxMatrixText::setTextWithShift(char* sString)
{
    if(sString != NULL) {
        SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_READY;
        String = sString;
        return E_OK;
    } else return E_NOT_OK;
} /* setTextWithShift */


/******************************************************************************************************************************************************
  setSprite()
******************************************************************************************************************************************************/
/*! \brief          print sprite on matrix
 *  \details        this function prints an sprite on the matrix located to the given coordinates
 *                  
 *  \param[in]      Panel       the matrix the text is drawn on
 *  \param[in]      X           x coordinate
 *  \param[in]      Y           y coordinate
 *  \param[in]      Sprite      sprite from sprite table (width, height, value column 1 ... 5)
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixText::setSprite(MaxMatrixPanel& Panel, int X, int Y, const spriteType Sprite)
{
    int SpriteWidth = Sprite[ASCII_TABLE_SPRITE_WIDTH];
    int SpriteHeight = Sprite[ASCII_TABLE_SPRITE_HEIGHT];
    
    /* if height of Sprite = height of Matrix set whole column */
    if(SpriteHeight == MAXMATRIX_ROW_NUMBER_OF_MODULE && Y == 0) {
        for(int SpriteColumn = 0; SpriteColumn < SpriteWidth; SpriteColumn++)
        {
            int Column = X + SpriteColumn;
            if(Column >= 0 && Column < MAXMATRIX_NUMBER_OF_COLUMNS)
                Panel.setColumn(Column, Sprite[SpriteColumn + ASCII_TABLE_SPRITE_COLUMN1]);
        } 
    } else { /* otherwise we have to set every single dot */
        for(int SpriteColumn = 0; SpriteColumn < SpriteWidth; SpriteColumn++)
        {
            for(int SpriteRow = 0; SpriteRow < SpriteHeight; SpriteRow++)
            {
                int Column = X + SpriteColumn;
                int Row = Y + SpriteRow;
                if(Column >= 0 && Column < MAXMATRIX_NUMBER_OF_COLUMNS && Row >= 0 && Row < MAXMATRIX_ROW_NUMBER_OF_MODULE)
                    Panel.setDot(Column, Row, bitRead(Sprite[SpriteColumn + ASCII_TABLE_SPRITE_COLUMN1], SpriteRow));
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
stdReturnType MaxMatrixText::setSpaceBetweenChars(byte sSpaceBetweenChars)
{
    /* every byte value is a valid number of spaces */
    SpaceBetweenChars = sSpaceBetweenChars;
    return E_OK;
} /* setSpaceBetweenChars */


/******************************************************************************************************************************************************
  charShiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts char one column to the left
 *  \details        this function has to be called periodically until it returns false
 *  \param[in]      Panel       the matrix the text is drawn on
 *  \return         true while the char or the space after it is still shifting
 *  \pre            has to be called from MaxMatrix::shiftTask() or stringShiftTask()
 *****************************************************************************************************************************************************/
bool MaxMatrixText::charShiftTask(MaxMatrixPanel& Panel)
{
    Panel.shiftLeft(false, true);
    SpriteShiftCounter++;

    /* if sprite is completed set only spaces */
    if(SpriteShiftCounter <= SpriteBuffer[ASCII_TABLE_SPRITE_WIDTH]) {
        setSprite(Panel, MAXMATRIX_NUMBER_OF_COLUMNS - SpriteShiftCounter, 0, SpriteBuffer);
    }
    /* if sprite and spaces are completed task has finished */
    else if(SpriteShiftCounter == SpriteBuffer[ASCII_TABLE_SPRITE_WIDTH] + SpaceBetweenChars + 1) {
        SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_READY;
        return false;
    }
    return true;
} /* charShiftTask */


/******************************************************************************************************************************************************
  stringShiftTask()
******************************************************************************************************************************************************/
/*! \brief          shifts string one column to the left
 *  \details        this function has to be called periodically until it returns false
 *  \param[in]      Panel       the matrix the text is drawn on
 *  \return         true while the string is still shifting
 *  \pre            has to be called from MaxMatrix::shiftTask()
 *****************************************************************************************************************************************************/
bool MaxMatrixText::stringShiftTask(MaxMatrixPanel& Panel)
{
    spriteIndexType SpriteIndex;
    char Char;

    /* has char shift task finished shifting sprite or char */
    if(SpriteShiftCounter == MAXMATRIX_SPRITE_SHIFT_STATE_READY)
    {
        /* Unknown chars are skipped within this call: a call spent on one sent nothing, and the text
         * stood still for a step. So read on to the next known char or to the end of the string. */
        while(*String != STD_NULL_CHARACTER && SpriteShiftCounter == MAXMATRIX_SPRITE_SHIFT_STATE_READY) {
            String += MaxMatrixFont::decodeChar(String, &Char);
            if(E_OK == MaxMatrixFont::convertCharToSprite(Char, &SpriteIndex) && E_OK == MaxMatrixFont::getSprite(SpriteIndex, &SpriteBuffer)) {
                SpriteShiftCounter = MAXMATRIX_SPRITE_SHIFT_STATE_RUNNING;
            }
        }
        if(SpriteShiftCounter == MAXMATRIX_SPRITE_SHIFT_STATE_RUNNING) {
            charShiftTask(Panel);
        } else { /* end of string reached, task has finished */
            Panel.shiftLeft(false, true);
            return false;
        }
    } else {
        /* go on shifting Sprite; the end of a char is not the end of the string */
        charShiftTask(Panel);
    }
    return true;
} /* stringShiftTask */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
