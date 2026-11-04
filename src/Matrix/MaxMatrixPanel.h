// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as the viewer sees it.
 *
 * Columns, rows and dots in the viewer's coordinates, on top of the matrix as it is wired. Drawing
 * only changes the buffer; MaxMatrix decides when it is sent.
 */

#ifndef MAXMATRIX_SRC_MATRIX_MAXMATRIXPANEL_H_
#define MAXMATRIX_SRC_MATRIX_MAXMATRIXPANEL_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "MaxMatrixWired.h"

/*! \brief How the whole matrix is turned: by 180 degrees for a board mounted upside down. */
enum class MaxMatrixMatrixOrientation : uint8_t {
    k0,   /*!< The board as it is meant to be mounted. */
    k180  /*!< The board upside down: the module at the input on the left. */
};

/*!
 * \brief How every module is turned in its place, by how its LEDs are wired.
 *
 * The README says which orientation fits which module.
 */
enum class MaxMatrixModuleOrientation : uint8_t {
    k0,   /*!< As wired. */
    k90,  /*!< Turned by 90 degrees. */
    k180, /*!< Turned by 180 degrees. */
    k270  /*!< Turned by 270 degrees, as on an FC-16 board. */
};

/*!
 * \brief A row of the whole matrix: one byte per module from the left, bit 7 of each the leftmost
 *        column of its module.
 */
typedef byte MaxMatrixRow[MAXMATRIX_NUMBER_OF_MODULES];

/*!
 * \brief The matrix as the viewer sees it: columns, rows and dots in the viewer's coordinates.
 *
 * It turns every coordinate by the matrix and the module orientation into the buffer of
 * MaxMatrixWired. A sketch reaches it through MaxMatrix, which takes most of its functions over.
 */
class MaxMatrixPanel
{
  private:
    MaxMatrixWired wired_;
    MaxMatrixModuleOrientation module_orientation_;
    MaxMatrixMatrixOrientation matrix_orientation_;

    static byte ReverseByte(byte input);
    void ConvertDotToBuffer(byte column, byte row, byte* buffer_column, byte* buffer_bit);
    byte GetLine(byte column, byte row, bool vertical);
    void SetLine(byte column, byte row, bool vertical, byte value);
    void GetPicture(byte* picture);
    void SetPicture(byte const* picture);

  public:
    /*!
     * \brief Makes the panel of the chain at these pins, in orientation 0.
     *
     * \param[in] data_in_pin The pin DIN of the chain is connected to.
     * \param[in] chip_select_pin The pin CS of the chain is connected to.
     * \param[in] clock_pin The pin CLK of the chain is connected to.
     */
    MaxMatrixPanel(byte data_in_pin, byte chip_select_pin, byte clock_pin);

    /*! \brief Returns the matrix as it is wired, below this one. */
    MaxMatrixWired& GetWired() { return wired_; }

    /*! \copydoc GetWired() */
    MaxMatrixWired const& GetWired() const { return wired_; }

    /*! \brief Returns how every module is turned in its place. */
    MaxMatrixModuleOrientation GetModuleOrientation() const { return module_orientation_; }

    /*! \brief Returns how the whole matrix is turned. */
    MaxMatrixMatrixOrientation GetMatrixOrientation() const { return matrix_orientation_; }

    /*!
     * \brief Returns a column of the matrix.
     *
     * \param[in] column The column, from 0 at the left to eight times the modules.
     * \return The column, bit 0 the top row, or MaxMatrixError::kOutOfRange if it lies off the
     *         panel.
     */
    MaxMatrixResult<byte> GetColumn(int16_t column);

    /*!
     * \brief Returns a column of a module.
     *
     * \param[in] module The module, from 0 at the left.
     * \param[in] column The column within the module, 0 to 7.
     * \return The column, bit 0 the top row, or MaxMatrixError::kOutOfRange if the module or the
     *         column does not exist.
     */
    MaxMatrixResult<byte> GetColumn(byte module, byte column);

    /*!
     * \brief Gives a row of the matrix, one byte per module.
     *
     * \param[in] row The row, from 0 at the top.
     * \param[out] value The row, bit 7 of each byte the leftmost column of its module; left as it
     *                   was on an error.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kOutOfRange if the row lies off the panel.
     */
    MaxMatrixStatus GetRow(int16_t row, MaxMatrixRow& value);

    /*!
     * \brief Returns a row of a module.
     *
     * The leftmost column is in bit 7, so that a row written as a binary literal looks like the
     * dots it stands for.
     *
     * \param[in] module The module, from 0 at the left.
     * \param[in] row The row within the module, 0 to 7.
     * \return The row, bit 7 the leftmost column, or MaxMatrixError::kOutOfRange if the module or
     *         the row does not exist.
     */
    MaxMatrixResult<byte> GetRow(byte module, byte row);

    /*!
     * \brief Returns whether a dot is lit.
     *
     * \param[in] column The column, negative to the left of the panel.
     * \param[in] row The row, negative above the panel.
     * \return Whether the dot is lit, or MaxMatrixError::kOutOfRange if it lies off the panel.
     */
    MaxMatrixResult<bool> GetDot(int16_t column, int16_t row);

    /*!
     * \brief Sets a column of the matrix.
     *
     * A column off the panel is left out, as for SetDot().
     *
     * \param[in] column The column, from 0 at the left to eight times the modules.
     * \param[in] value The column, bit 0 the top row.
     */
    void SetColumn(int16_t column, byte value);

    /*!
     * \brief Sets a column of a module.
     *
     * A module or a column that does not exist is left out.
     *
     * \param[in] module The module, from 0 at the left.
     * \param[in] column The column within the module, 0 to 7.
     * \param[in] value The column, bit 0 the top row.
     */
    void SetColumn(byte module, byte column, byte value);

    /*!
     * \brief Sets a row of the matrix, one byte per module.
     *
     * A row off the panel is left out, as for SetDot().
     *
     * \param[in] row The row, from 0 at the top.
     * \param[in] value The row, bit 7 of each byte the leftmost column of its module.
     */
    void SetRow(int16_t row, MaxMatrixRow const value);

    /*!
     * \brief Sets a row of a module.
     *
     * A module or a row that does not exist is left out.
     *
     * \param[in] module The module, from 0 at the left.
     * \param[in] row The row within the module, 0 to 7.
     * \param[in] value The row, bit 7 the leftmost column.
     */
    void SetRow(byte module, byte row, byte value);

    /*!
     * \brief Lights a dot or turns it off.
     *
     * A dot off the panel is left out rather than reported: a sprite, a line or a text that lies
     * partly off the panel is drawn all the time while something moves in or out, and a sketch
     * would only have to clip it itself. Adafruit GFX's `drawPixel()` and Rust's
     * embedded-graphics do the same.
     *
     * \param[in] column The column, negative to the left of the panel.
     * \param[in] row The row, negative above the panel.
     * \param[in] value True to light it.
     */
    void SetDot(int16_t column, int16_t row, bool value);

    /*!
     * \brief Sets how every module is turned in its place.
     *
     * The picture stays as the viewer sees it; the buffer is turned along.
     *
     * \param[in] module_orientation 0, 90, 180 or 270 degrees, as the LEDs of a module are wired.
     */
    void SetModuleOrientation(MaxMatrixModuleOrientation module_orientation);

    /*!
     * \brief Sets how the whole matrix is turned.
     *
     * By 180 degrees for a board mounted upside down, with the module at the input then on the
     * left; in addition to the module orientation. The picture stays as the viewer sees it; the
     * buffer is turned along.
     *
     * \param[in] matrix_orientation 0 or 180 degrees.
     */
    void SetMatrixOrientation(MaxMatrixMatrixOrientation matrix_orientation);

    /*!
     * \brief Shifts the whole picture one column to the left.
     *
     * \param[in] rotate True to bring the leftmost column in again at the right.
     * \param[in] fill_with_zero True to turn the new column at the right off; false keeps what it
     *                           held. Without effect when `rotate` is true.
     */
    void ShiftLeft(bool rotate = false, bool fill_with_zero = true);

    /*!
     * \brief Shifts the whole picture one column to the right.
     *
     * \param[in] rotate True to bring the rightmost column in again at the left.
     * \param[in] fill_with_zero True to turn the new column at the left off; false keeps what it
     *                           held. Without effect when `rotate` is true.
     */
    void ShiftRight(bool rotate = false, bool fill_with_zero = true);

    /*!
     * \brief Shifts the whole picture one row up.
     *
     * \param[in] rotate True to bring the top row in again at the bottom; false turns the new row
     *                   off.
     */
    void ShiftUp(bool rotate = false);

    /*!
     * \brief Shifts the whole picture one row down.
     *
     * \param[in] rotate True to bring the bottom row in again at the top; false turns the new row
     *                   off.
     */
    void ShiftDown(bool rotate = false);
};

#endif
