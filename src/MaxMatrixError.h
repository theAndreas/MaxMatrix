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
#ifndef MAXMATRIX_SRC_MAXMATRIXERROR_H_
#define MAXMATRIX_SRC_MAXMATRIXERROR_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixConfig.h"


/******************************************************************************************************************************************************
 *  GLOBAL FUNCTION MACROS
 *****************************************************************************************************************************************************/
/*! passes on what Call returns, after reporting its error, if it has one, under Api: the name of the facade function, as
 * a string literal. See maxMatrixReportError() below. */
#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
#define MAXMATRIX_REPORT_ERROR(Api, Call)                           MaxMatrixReportIfError(F(Api), Call)
#else
#define MAXMATRIX_REPORT_ERROR(Api, Call)                           (Call)
#endif


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/*! why a call did not do what it was asked; one byte, so that it returns in a register */
enum class MaxMatrixError : uint8_t {
    kOk = 0,                     /*!< no error */
    kOutOfRange,               /*!< a coordinate, module, sprite index, intensity or number beyond its range */
    kUnknownChar,               /*!< the font has no sprite for the char */
    kNoText,                    /*!< the text is NULL, or the text of a command empty */
    kNotANumber,               /*!< the value of a command is empty or holds anything but digits */
    kUnknownCommand,            /*!< no command has this name */
    kLineTooLong               /*!< a command line did not fit into MAXMATRIX_COMMAND_LINE_LENGTH */
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixStatus
 *****************************************************************************************************************************************************/
/*! What a function that can fail returns when it has no value to return. It has no conversion to bool on purpose: with
 * stdReturnType, E_OK was 0, so if(Matrix.setText(…)) was true on an error. A sketch asks isOk() or compares with an
 * error, and reaches the error only through this class, so that the class could carry more than the enum one day
 * without a sketch changing. */
class MaxMatrixStatus
{
  private:
    MaxMatrixError error_;

  public:
    /*! \brief a status of the error, MaxMatrixError::kOk when none is given; a function returns an error as it is */
    MaxMatrixStatus(MaxMatrixError error = MaxMatrixError::kOk) : error_(error) {}

    // get methods
    /*! \brief why the call failed, or MaxMatrixError::kOk */
    MaxMatrixError GetError() const { return error_; }
    __FlashStringHelper const* GetErrorText() const;

    // methods
    /*! \brief whether the call did what it was asked */
    bool IsOk() const { return error_ == MaxMatrixError::kOk; }
    /*! \brief whether the call failed for this reason */
    bool operator==(MaxMatrixError other) const { return error_ == other; }
    /*! \brief whether the call did not fail for this reason */
    bool operator!=(MaxMatrixError other) const { return error_ != other; }
};


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixResult
 *****************************************************************************************************************************************************/
/*! What a getter of a single value returns: the value, or why there is none, instead of the value through a pointer
 * beside a status. The two lie side by side rather than in a union: a byte and its error still return in registers on
 * the AVR, and the value is never left undefined, as it is 0 after an error. An array cannot be returned, so getRow()
 * of the whole panel and getSprite() keep their parameter and return a MaxMatrixStatus. */
template<typename ValueType>
class MaxMatrixResult
{
  private:
    ValueType value_;
    MaxMatrixError error_;

  public:
    /*! \brief a result that holds a value; a function returns the value as it is */
    MaxMatrixResult(ValueType value) : value_(value), error_(MaxMatrixError::kOk) {}
    /*! \brief a result that holds why there is no value; a function returns the error as it is */
    MaxMatrixResult(MaxMatrixError error) : value_(), error_(error) {}

    // get methods
    /*! \brief the value; 0, or what ValueType() is, after an error */
    ValueType GetValue() const { return value_; }
    /*! \brief why there is no value, or MaxMatrixError::kOk */
    MaxMatrixError GetError() const { return error_; }
    /*! \copydoc MaxMatrixStatus::getErrorText() */
    __FlashStringHelper const* GetErrorText() const { return MaxMatrixStatus(error_).GetErrorText(); }

    // methods
    /*! \brief whether there is a value */
    bool IsOk() const { return error_ == MaxMatrixError::kOk; }
    /*! \brief whether the call failed for this reason */
    bool operator==(MaxMatrixError other) const { return error_ == other; }
    /*! \brief whether the call did not fail for this reason */
    bool operator!=(MaxMatrixError other) const { return error_ != other; }
};


/******************************************************************************************************************************************************
 *  GLOBAL FUNCTION PROTOTYPES
 *****************************************************************************************************************************************************/
/* With MAXMATRIX_DEV_ERROR_DETECT, as the DET of AUTOSAR Classic, every function of MaxMatrix that fails reports its name
 * and the error to maxMatrixReportError(), so that a sketch which checks no return value still learns where it went
 * wrong. The default is weak and prints with maxMatrixPrintError(); a sketch replaces it by defining its own. Switched
 * off, MAXMATRIX_REPORT_ERROR() is the bare call: no report, no name in flash, no Serial. */
#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
void MaxMatrixReportError(__FlashStringHelper const* api, MaxMatrixError error);
void MaxMatrixPrintError(__FlashStringHelper const* api, MaxMatrixError error);

/* passes a MaxMatrixStatus or MaxMatrixResult<T> on, after reporting its error, if it has one */
template<typename ResultType>
ResultType MaxMatrixReportIfError(__FlashStringHelper const* api, ResultType result)
{
    if(!result.IsOk()) MaxMatrixReportError(api, result.GetError());
    return result;
}
#endif

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
