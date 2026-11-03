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
#include "MaxMatrixError.h"
#include "MaxMatrixWired.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/*! how the whole matrix is turned, 180 for a board mounted upside down */
typedef enum : uint8_t {
    MAXMATRIX_MATRIX_ORIENTATION_0,         /*!< the board as it is meant to be mounted */
    MAXMATRIX_MATRIX_ORIENTATION_180        /*!< the board upside down: the module at the input on the left */
} MaxMatrixMatrixOrientationType;

/*! how every module is turned in its place, by how its LEDs are wired; the README says which fits which module */
typedef enum : uint8_t {
    MAXMATRIX_MODULE_ORIENTATION_0,         /*!< as wired */
    MAXMATRIX_MODULE_ORIENTATION_90,        /*!< turned by 90 degrees */
    MAXMATRIX_MODULE_ORIENTATION_180,       /*!< turned by 180 degrees */
    MAXMATRIX_MODULE_ORIENTATION_270        /*!< turned by 270 degrees, as on an FC-16 board */
} MaxMatrixModuleOrientationType;

/*! a row of the whole matrix, one byte per module from the left, bit 7 of each the leftmost column of its module */
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
    static byte reverseByte(byte);
    void convertDotToBuffer(byte, byte, byte*, byte*);
    byte getLine(byte, byte, bool);
    void setLine(byte, byte, bool, byte);
    void getPicture(byte*);
    void setPicture(byte const*);

  public:
    MaxMatrixPanel(byte, byte, byte);

    // get methods
    MaxMatrixWired& getWired() { return Wired; }
    MaxMatrixWired const& getWired() const { return Wired; }
    MaxMatrixModuleOrientationType getModuleOrientation() const { return ModuleOrientation; }
    MaxMatrixMatrixOrientationType getMatrixOrientation() const { return MatrixOrientation; }

    MaxMatrixResult<byte> getColumn(int16_t);
    MaxMatrixResult<byte> getColumn(byte, byte);
    MaxMatrixStatus getRow(int16_t, rowType&);
    MaxMatrixResult<byte> getRow(byte, byte);
    MaxMatrixResult<bool> getDot(int16_t, int16_t);

    // set methods
    void setColumn(int16_t, byte);
    void setColumn(byte, byte, byte);
    void setRow(int16_t, rowType const);
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
