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
#ifndef MAXMATRIX_SRC_MATRIX_MAXMATRIXPANEL_H_
#define MAXMATRIX_SRC_MATRIX_MAXMATRIXPANEL_H_

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
enum class MaxMatrixMatrixOrientation : uint8_t {
    k0,         /*!< the board as it is meant to be mounted */
    k180        /*!< the board upside down: the module at the input on the left */
};

/*! how every module is turned in its place, by how its LEDs are wired; the README says which fits which module */
enum class MaxMatrixModuleOrientation : uint8_t {
    k0,         /*!< as wired */
    k90,        /*!< turned by 90 degrees */
    k180,       /*!< turned by 180 degrees */
    k270        /*!< turned by 270 degrees, as on an FC-16 board */
};

/*! a row of the whole matrix, one byte per module from the left, bit 7 of each the leftmost column of its module */
typedef byte MaxMatrixRow[MAXMATRIX_NUMBER_OF_MODULES];


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixPanel
 *****************************************************************************************************************************************************/
class MaxMatrixPanel
{
  private:
    MaxMatrixWired wired_;
    MaxMatrixModuleOrientation module_orientation_;
    MaxMatrixMatrixOrientation matrix_orientation_;

    // functions
    static byte ReverseByte(byte);
    void ConvertDotToBuffer(byte, byte, byte*, byte*);
    byte GetLine(byte, byte, bool);
    void SetLine(byte, byte, bool, byte);
    void GetPicture(byte*);
    void SetPicture(byte const*);

  public:
    MaxMatrixPanel(byte, byte, byte);

    // get methods
    MaxMatrixWired& GetWired() { return wired_; }
    MaxMatrixWired const& GetWired() const { return wired_; }
    MaxMatrixModuleOrientation GetModuleOrientation() const { return module_orientation_; }
    MaxMatrixMatrixOrientation GetMatrixOrientation() const { return matrix_orientation_; }

    MaxMatrixResult<byte> GetColumn(int16_t);
    MaxMatrixResult<byte> GetColumn(byte, byte);
    MaxMatrixStatus GetRow(int16_t, MaxMatrixRow&);
    MaxMatrixResult<byte> GetRow(byte, byte);
    MaxMatrixResult<bool> GetDot(int16_t, int16_t);

    // set methods
    void SetColumn(int16_t, byte);
    void SetColumn(byte, byte, byte);
    void SetRow(int16_t, MaxMatrixRow const);
    void SetRow(byte, byte, byte);
    void SetDot(int16_t, int16_t, bool);
    void SetModuleOrientation(MaxMatrixModuleOrientation);
    void SetMatrixOrientation(MaxMatrixMatrixOrientation);

    // methods
    void ShiftLeft(bool = false, bool = true);
    void ShiftRight(bool = false, bool = true);
    void ShiftUp(bool = false);
    void ShiftDown(bool = false);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
