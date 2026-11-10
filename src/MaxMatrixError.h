// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Why a call did not do what it was asked.
 *
 * The error a function of the library returns, alone or together with the value
 * of a getter. What was weighed against it, error domains among them, is in
 * docu/ErrorHandling.md.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIXERROR_H_
#define MAXMATRIX_SRC_MAXMATRIXERROR_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"

/*!
 * \brief Passes on what `Call` returns, after reporting its error, if it has
 *        one, under `Api`.
 *
 * With `MAXMATRIX_DEV_ERROR_DETECT` at `STD_OFF` it is the bare call: no
 * report, no name in flash, no `Serial`.
 *
 * \param Api The name of the facade function, as a string literal.
 * \param Call The call of the part below, which returns a MaxMatrixStatus or a
 *             MaxMatrixResult.
 *
 * \see MaxMatrixReportError()
 */
#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
#define MAXMATRIX_REPORT_ERROR(Api, Call) MaxMatrixReportIfError(F(Api), Call)
#else
#define MAXMATRIX_REPORT_ERROR(Api, Call) (Call)
#endif

/*! \brief Why a call did not do what it was asked; one byte, so that it returns
 * in a register. */
enum class MaxMatrixError : uint8_t {
  kOk = 0, /*!< No error. */
  /*! A coordinate, module, intensity or number beyond its range. */
  kOutOfRange,
  kUnknownChar, /*!< The font has no char for it. */
  kNoText,      /*!< The text is null, or the text of a command empty. */
  /*! The value of a command is empty or holds anything but digits. */
  kNotANumber,
  kUnknownCommand, /*!< No command has this name. */
  /*! A command line did not fit into `MAXMATRIX_COMMAND_LINE_LENGTH`. */
  kLineTooLong,
  /*! The font is null, does not start as MaxMatrixFont says, or its chars do
     not fill its array. */
  kNotAFont
};

/*!
 * \brief What a function that can fail returns when it has no value to return.
 *
 * A sketch asks IsOk(), or compares with an error:
 *
 * \code{.cpp}
 * if(matrix.SetText(text) == MaxMatrixError::kUnknownChar) { ... }
 * \endcode
 *
 * \note It has no conversion to `bool`, on purpose: with `stdReturnType`,
 *       `E_OK` was 0, so `if(matrix.SetText(...))` was true on an error.
 */
class MaxMatrixStatus {
 private:
  // A sketch reaches the error only through this class, so that it could carry
  // more than the enum one day without a sketch changing.
  MaxMatrixError error_;

 public:
  /*!
   * \brief Makes a status of `error`, ok when none is given.
   *
   * Implicit, so that a function returns an error as it is.
   *
   * \param[in] error The error, or MaxMatrixError::kOk.
   */
  MaxMatrixStatus(  // NOLINT(runtime/explicit)
      MaxMatrixError error = MaxMatrixError::kOk)
      : error_(error) {}

  /*! \brief Returns why the call failed, or MaxMatrixError::kOk. */
  MaxMatrixError GetError() const { return error_; }

  /*!
   * \brief Returns the error in a few words, in flash, for `Serial.print()`.
   *
   * `"out of range"` for MaxMatrixError::kOutOfRange. `Serial.print()` reads a
   * `__FlashStringHelper` from flash, so the text needs no RAM. A sketch that
   * never calls it pays nothing, as the linker drops it with its texts.
   *
   * \return The text; `"unknown error"` for a byte cast to the enum that is
   *         none of its values.
   */
  __FlashStringHelper const* GetErrorText() const;

  /*! \brief Returns whether the call did what it was asked. */
  bool IsOk() const { return error_ == MaxMatrixError::kOk; }

  /*!
   * \brief Returns whether the call failed for this reason.
   *
   * \param[in] other The error to compare with.
   */
  bool operator==(MaxMatrixError other) const { return error_ == other; }

  /*!
   * \brief Returns whether the call did not fail for this reason.
   *
   * \param[in] other The error to compare with.
   */
  bool operator!=(MaxMatrixError other) const { return error_ != other; }
};

/*!
 * \brief What a getter of a single value returns: the value, or why there is
 *        none.
 *
 * Instead of the value through a pointer beside a status. After an error the
 * value is 0, or what `ValueType()` is, never undefined. An array cannot be
 * returned, so GetRow() of the whole panel keeps its parameter and returns a
 * MaxMatrixStatus.
 *
 * \tparam ValueType The type of the value.
 */
template <typename ValueType>
class MaxMatrixResult {
 private:
  // Side by side rather than in a union: a byte and its error still return in
  // registers on the AVR, and the value is never left undefined.
  ValueType value_;
  MaxMatrixError error_;

 public:
  /*!
   * \brief Makes a result that holds `value`.
   *
   * Implicit, so that a function returns its value as it is.
   *
   * \param[in] value The value.
   */
  MaxMatrixResult(ValueType value)  // NOLINT(runtime/explicit)
      : value_(value), error_(MaxMatrixError::kOk) {}

  /*!
   * \brief Makes a result that holds why there is no value.
   *
   * Implicit, so that a function returns its error as it is.
   *
   * \param[in] error The error.
   */
  MaxMatrixResult(MaxMatrixError error)  // NOLINT(runtime/explicit)
      : value_(), error_(error) {}

  /*! \brief Returns the value; 0, or what `ValueType()` is, after an error. */
  ValueType GetValue() const { return value_; }

  /*! \brief Returns why there is no value, or MaxMatrixError::kOk. */
  MaxMatrixError GetError() const { return error_; }

  /*! \copydoc MaxMatrixStatus::GetErrorText() */
  __FlashStringHelper const* GetErrorText() const {
    return MaxMatrixStatus(error_).GetErrorText();
  }

  /*! \brief Returns whether there is a value. */
  bool IsOk() const { return error_ == MaxMatrixError::kOk; }

  /*! \copydoc MaxMatrixStatus::operator==(MaxMatrixError) const */
  bool operator==(MaxMatrixError other) const { return error_ == other; }

  /*! \copydoc MaxMatrixStatus::operator!=(MaxMatrixError) const */
  bool operator!=(MaxMatrixError other) const { return error_ != other; }
};

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/*!
 * \brief Takes the report of a function of MaxMatrix that failed.
 *
 * With `MAXMATRIX_DEV_ERROR_DETECT` at `STD_ON`, as the DET of AUTOSAR Classic,
 * every function of MaxMatrix that fails reports its name and the error here,
 * so that a sketch which checks no return value still learns where it went
 * wrong. The default prints with MaxMatrixPrintError() on `Serial`. It is weak:
 * a sketch whose `Serial` carries something else, or which wants to halt or
 * blink instead, defines its own, and the default is left out.
 *
 * \param[in] api The name of the function that failed, in flash.
 * \param[in] error Why it failed.
 * \pre For the default, the sketch has called `Serial.begin()`.
 */
void MaxMatrixReportError(__FlashStringHelper const* api, MaxMatrixError error);

/*!
 * \brief Prints a report on `Serial`, as the default MaxMatrixReportError()
 *        does.
 *
 * One line, `MaxMatrix SetText: unknown char`; for a report of a sketch's own
 * that prints as well.
 *
 * \param[in] api The name of the function that failed, in flash.
 * \param[in] error Why it failed.
 * \pre The sketch has called `Serial.begin()`.
 */
void MaxMatrixPrintError(__FlashStringHelper const* api, MaxMatrixError error);

/*! \cond */
// Passes a MaxMatrixStatus or MaxMatrixResult<T> on, after reporting its error,
// if it has one; what MAXMATRIX_REPORT_ERROR() calls.
template <typename ResultType>
ResultType MaxMatrixReportIfError(__FlashStringHelper const* api,
                                  ResultType result) {
  if (!result.IsOk()) MaxMatrixReportError(api, result.GetError());
  return result;
}
/*! \endcond */
#endif

#endif  // MAXMATRIX_SRC_MAXMATRIXERROR_H_
