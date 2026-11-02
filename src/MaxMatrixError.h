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
/**     \file       MaxMatrixError.h
 *      \brief      Why a call did not do what it was asked
 *
 *      \details    The error a function of the library returns, alone or together with the value of a getter. What was
 *                  weighed against it, error domains among them, is in docu/ErrorHandling.md.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_ERROR_H_
#define _MAXMATRIX_ERROR_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* why a call did not do what it was asked; one byte, so that it returns in a register */
typedef enum : uint8_t {
    MAXMATRIX_E_OK = 0,
    MAXMATRIX_E_OUT_OF_RANGE,               /* a coordinate, module, sprite index, intensity or number beyond its range */
    MAXMATRIX_E_UNKNOWN_CHAR,               /* the font has no sprite for the char */
    MAXMATRIX_E_NO_TEXT,                    /* the text is NULL */
    MAXMATRIX_E_NOT_A_NUMBER                /* the value of a command is empty or holds anything but digits */
} MaxMatrixErrorType;


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixStatus
 *****************************************************************************************************************************************************/
/* What a function that can fail returns when it has no value to return. It has no conversion to bool on purpose: with
 * stdReturnType, E_OK was 0, so if(Matrix.setText(…)) was true on an error. A sketch asks isOk() or compares with an
 * error, and reaches the error only through this class, so that the class could carry more than the enum one day
 * without a sketch changing. */
class MaxMatrixStatus
{
  private:
    MaxMatrixErrorType Error;

  public:
    MaxMatrixStatus(MaxMatrixErrorType sError = MAXMATRIX_E_OK) : Error(sError) {}

    // get methods
    MaxMatrixErrorType getError() const { return Error; }

    // methods
    bool isOk() const { return Error == MAXMATRIX_E_OK; }
    bool operator==(MaxMatrixErrorType Other) const { return Error == Other; }
    bool operator!=(MaxMatrixErrorType Other) const { return Error != Other; }
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixResult
 *****************************************************************************************************************************************************/
/* What a getter of a single value returns: the value, or why there is none, instead of the value through a pointer
 * beside a status. The two lie side by side rather than in a union: a byte and its error still return in registers on
 * the AVR, and the value is never left undefined, as it is 0 after an error. An array cannot be returned, so getRow()
 * of the whole panel and getSprite() keep their parameter and return a MaxMatrixStatus. */
template<typename ValueType>
class MaxMatrixResult
{
  private:
    ValueType Value;
    MaxMatrixErrorType Error;

  public:
    MaxMatrixResult(ValueType sValue) : Value(sValue), Error(MAXMATRIX_E_OK) {}
    MaxMatrixResult(MaxMatrixErrorType sError) : Value(), Error(sError) {}

    // get methods
    ValueType getValue() const { return Value; }
    MaxMatrixErrorType getError() const { return Error; }

    // methods
    bool isOk() const { return Error == MAXMATRIX_E_OK; }
    bool operator==(MaxMatrixErrorType Other) const { return Error == Other; }
    bool operator!=(MaxMatrixErrorType Other) const { return Error != Other; }
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
