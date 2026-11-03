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
#include "MaxMatrixConfig.h"


/******************************************************************************************************************************************************
 *  GLOBAL FUNCTION MACROS
 *****************************************************************************************************************************************************/
/* passes on what Call returns, after reporting its error, if it has one, under Api: the name of the facade function, as
 * a string literal. See maxMatrixReportError() below. */
#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
#define MAXMATRIX_REPORT_ERROR(Api, Call)                           maxMatrixReportIfError(F(Api), Call)
#else
#define MAXMATRIX_REPORT_ERROR(Api, Call)                           (Call)
#endif


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* why a call did not do what it was asked; one byte, so that it returns in a register */
typedef enum : uint8_t {
    MAXMATRIX_E_OK = 0,
    MAXMATRIX_E_OUT_OF_RANGE,               /* a coordinate, module, sprite index, intensity or number beyond its range */
    MAXMATRIX_E_UNKNOWN_CHAR,               /* the font has no sprite for the char */
    MAXMATRIX_E_NO_TEXT,                    /* the text is NULL, or the text of a command empty */
    MAXMATRIX_E_NOT_A_NUMBER,               /* the value of a command is empty or holds anything but digits */
    MAXMATRIX_E_UNKNOWN_COMMAND,            /* no command has this name */
    MAXMATRIX_E_LINE_TOO_LONG               /* a command line did not fit into MAXMATRIX_COMMAND_LINE_LENGTH */
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
    __FlashStringHelper const* getErrorText() const;

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
    __FlashStringHelper const* getErrorText() const { return MaxMatrixStatus(Error).getErrorText(); }

    // methods
    bool isOk() const { return Error == MAXMATRIX_E_OK; }
    bool operator==(MaxMatrixErrorType Other) const { return Error == Other; }
    bool operator!=(MaxMatrixErrorType Other) const { return Error != Other; }
};


/******************************************************************************************************************************************************
 *  GLOBAL FUNCTION PROTOTYPES
 *****************************************************************************************************************************************************/
/* With MAXMATRIX_DEV_ERROR_DETECT, as the DET of AUTOSAR Classic, every function of MaxMatrix that fails reports its name
 * and the error to maxMatrixReportError(), so that a sketch which checks no return value still learns where it went
 * wrong. The default is weak and prints with maxMatrixPrintError(); a sketch replaces it by defining its own. Switched
 * off, MAXMATRIX_REPORT_ERROR() is the bare call: no report, no name in flash, no Serial. */
#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
void maxMatrixReportError(__FlashStringHelper const* Api, MaxMatrixErrorType Error);
void maxMatrixPrintError(__FlashStringHelper const* Api, MaxMatrixErrorType Error);

/* passes a MaxMatrixStatus or MaxMatrixResult<T> on, after reporting its error, if it has one */
template<typename ResultType>
ResultType maxMatrixReportIfError(__FlashStringHelper const* Api, ResultType Result)
{
    if(!Result.isOk()) maxMatrixReportError(Api, Result.getError());
    return Result;
}
#endif

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
