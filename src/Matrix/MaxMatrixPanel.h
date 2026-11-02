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
/**     \file       MaxMatrixPanel.h
 *      \brief      The matrix as the viewer sees it
 *
 *      \details    Columns, rows and dots in the viewer's coordinates, on top of the matrix as it is wired. Drawing only
 *                  changes the buffer; MaxMatrix decides when it is sent.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_PANEL_H_
#define _MAXMATRIX_PANEL_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixWired.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* Type which describes the orientation of the whole Matrix, 180 for a board mounted upside down */
typedef enum {
    MAXMATRIX_MATRIX_ORIENTATION_0,
    MAXMATRIX_MATRIX_ORIENTATION_180
} MaxMatrixMatrixOrientationType;

/* Type which describes the orientation of every Module in its place */
typedef enum {
    MAXMATRIX_MODULE_ORIENTATION_0,
    MAXMATRIX_MODULE_ORIENTATION_90,
    MAXMATRIX_MODULE_ORIENTATION_180,
    MAXMATRIX_MODULE_ORIENTATION_270
} MaxMatrixModuleOrientationType;

/* row array type */
typedef byte rowType[MAXMATRIX_NUMBER_OF_MODULES];


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixPanel
 *****************************************************************************************************************************************************/
class MaxMatrixPanel
{
  private:
    MaxMatrixWired Wired;
    MaxMatrixModuleOrientationType ModuleOrientation;
    MaxMatrixMatrixOrientationType MatrixOrientation;

    // functions
    byte reverseByte(byte);
    void convertDotToBuffer(byte, byte, byte*, byte*);
    byte getLine(byte, byte, bool);
    void setLine(byte, byte, bool, byte);
    void getPicture(byte*);
    void setPicture(const byte*);

  public:
    MaxMatrixPanel(byte, byte, byte);

    // get methods
    MaxMatrixWired& getWired() { return Wired; }
    MaxMatrixModuleOrientationType getModuleOrientation() { return ModuleOrientation; }
    MaxMatrixMatrixOrientationType getMatrixOrientation() { return MatrixOrientation; }

    stdReturnType getColumn(byte, byte*);
    stdReturnType getColumn(byte, byte, byte*);
    stdReturnType getRow(byte, rowType);
    stdReturnType getRow(byte, byte, byte*);
    stdReturnType getDot(byte, byte, bool*);

    // set methods
    void setColumn(int16_t, byte);
    void setColumn(byte, byte, byte);
    void setRow(int16_t, const rowType);
    void setRow(byte, byte, byte);
    void setDot(int16_t, int16_t, bool);
    void setModuleOrientation(MaxMatrixModuleOrientationType);
    void setMatrixOrientation(MaxMatrixMatrixOrientationType);

    // methods
    void shiftLeft(bool = false, bool = true);
    void shiftRight(bool = false, bool = true);
    void shiftUp(bool = false);
    void shiftDown(bool = false);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
