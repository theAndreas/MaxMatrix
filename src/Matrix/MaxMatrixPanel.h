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

/* the three properties of a MaxMatrixModuleType, one bit each of its value */
#define MAXMATRIX_MODULE_TYPE_DIGITS_AS_ROWS     0x04u
#define MAXMATRIX_MODULE_TYPE_REVERSED_COLUMNS   0x02u
#define MAXMATRIX_MODULE_TYPE_REVERSED_ROWS      0x01u

/*!
 * \brief How the LEDs of a module are wired to its MAX7219, named as MD_MAX72XX names its hardware
 *        types.
 *
 * Three properties tell the eight wirings apart, seen with the module's input on the right, its
 * columns counted from the right and its rows from the top: DR1 if a digit register drives a row
 * of LEDs and its bits the columns, DR0 if a digit drives a column; CR1 if the bits run the other
 * way, bit 7 - n for n; RR1 if the digits do, digit 7 - n for n. Four of the eight have a name of
 * their own. Four are mirror images of the others, Parola among them. The value holds the three
 * properties as bits: DR in bit 2, CR in bit 1, RR in bit 0.
 */
enum class MaxMatrixModuleType : uint8_t {
    kDr0Cr0Rr0 = 0, /*!< Digits as columns. */
    kDr0Cr0Rr1 = 1, /*!< Digits as columns, in reverse; the buffer is the picture as the viewer sees it. */
    kDr0Cr1Rr0 = 2, /*!< Digits as columns, their bits in reverse. */
    kDr0Cr1Rr1 = 3, /*!< Digits as columns, both in reverse. */
    kDr1Cr0Rr0 = 4, /*!< Digits as rows. */
    kDr1Cr0Rr1 = 5, /*!< Digits as rows, in reverse. */
    kDr1Cr1Rr0 = 6, /*!< Digits as rows, their bits in reverse. */
    kDr1Cr1Rr1 = 7, /*!< Digits as rows, both in reverse. */
    kGeneric = kDr0Cr1Rr0,  /*!< The generic modules, often with the MAX7219 in a DIP package (`GENERIC_HW`). */
    kFc16 = kDr1Cr0Rr0,     /*!< FC-16, the common boards of four modules (`FC16_HW`). */
    kParola = kDr1Cr1Rr0,   /*!< The Parola modules (`PAROLA_HW`). */
    kIcstation = kDr1Cr1Rr1 /*!< The ICStation modules (`ICSTATION_HW`). */
};

/*!
 * \brief The matrix as the viewer sees it: columns, rows and dots in the viewer's coordinates.
 *
 * It turns every coordinate by the matrix orientation and the module type into the buffer of
 * MaxMatrixWired. A sketch reaches it through MaxMatrix, which takes most of its functions over.
 */
class MaxMatrixPanel
{
  private:
    MaxMatrixWired wired_;
    MaxMatrixModuleType module_type_;
    MaxMatrixMatrixOrientation matrix_orientation_;

    static byte ReverseByte(byte input);
    void ConvertDotToBuffer(byte column, byte row, byte* buffer_column, byte* buffer_bit);
    byte GetLine(byte column, byte row, bool vertical);
    void SetLine(byte column, byte row, bool vertical, byte value);
    void SetFarDot(int32_t column, int32_t row, bool value);
    void FillArea(int32_t left, int32_t top, int32_t right, int32_t bottom, bool value);
    void ChangeMapping(MaxMatrixMatrixOrientation matrix_orientation, MaxMatrixModuleType module_type);

  public:
    /*!
     * \brief Makes the panel of the chain at these pins, of modules MaxMatrixModuleType::kDr0Cr0Rr1
     *        in matrix orientation 0.
     *
     * \param[in] data_in_pin The pin DIN of the chain is connected to.
     * \param[in] chip_select_pin The pin CS of the chain is connected to.
     * \param[in] clock_pin The pin CLK of the chain is connected to.
     * \param[out] buffer The buffer, `MAXMATRIX_BUFFER_SIZE(number_of_modules)` bytes, which has to
     *                    live as long as the panel.
     * \param[in] number_of_modules The modules in the chain, 1 to
     *                              `MAXMATRIX_MAX_NUMBER_OF_MODULES`.
     */
    MaxMatrixPanel(byte data_in_pin, byte chip_select_pin, byte clock_pin, byte* buffer, byte number_of_modules);

    /*! \brief Returns the modules of the chain. */
    byte GetNumberOfModules() const { return wired_.GetChain().GetNumberOfModules(); }

    /*! \brief Returns the columns of the panel, eight per module. */
    byte GetNumberOfColumns() const { return wired_.GetNumberOfColumns(); }

    /*! \brief Returns the matrix as it is wired, below this one. */
    MaxMatrixWired& GetWired() { return wired_; }

    /*! \copydoc GetWired() */
    MaxMatrixWired const& GetWired() const { return wired_; }

    /*! \brief Returns how the LEDs of every module are wired. */
    MaxMatrixModuleType GetModuleType() const { return module_type_; }

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
     * \param[out] values The row, bit 7 of each byte the leftmost column of its module; left as it
     *                    was on an error.
     * \param[in] number_of_values The bytes `values` has room for.
     * \return MaxMatrixError::kOk, or MaxMatrixError::kOutOfRange if the row lies off the panel or
     *         `values` has room for fewer bytes than there are modules.
     */
    MaxMatrixStatus GetRow(int16_t row, byte* values, byte number_of_values);

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
     * A row off the panel is left out, as for SetDot(), and so are the modules beyond the bytes
     * given, or the bytes beyond the modules.
     *
     * \param[in] row The row, from 0 at the top.
     * \param[in] values The row, bit 7 of each byte the leftmost column of its module.
     * \param[in] number_of_values The bytes in `values`.
     */
    void SetRow(int16_t row, byte const* values, byte number_of_values);

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
     * \brief Draws a straight line from one dot to another, both ends included.
     *
     * What lies off the panel is left out, as for SetDot(); the ends may lie off it.
     *
     * \param[in] x0 The column of the first end.
     * \param[in] y0 The row of the first end.
     * \param[in] x1 The column of the second end.
     * \param[in] y1 The row of the second end.
     * \param[in] value True to light the line, false to turn it off.
     */
    void DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool value);

    /*!
     * \brief Draws the outline of a rectangle.
     *
     * What lies off the panel is left out, as for SetDot(). A width or height of 0 or less draws
     * nothing.
     *
     * \param[in] x The column of the left edge.
     * \param[in] y The row of the top edge.
     * \param[in] width The columns it spans.
     * \param[in] height The rows it spans.
     * \param[in] value True to light the outline, false to turn it off.
     */
    void DrawRect(int16_t x, int16_t y, int16_t width, int16_t height, bool value);

    /*!
     * \brief Draws a rectangle with its inside.
     *
     * What lies off the panel is left out, as for SetDot(). A width or height of 0 or less draws
     * nothing.
     *
     * \param[in] x The column of the left edge.
     * \param[in] y The row of the top edge.
     * \param[in] width The columns it spans.
     * \param[in] height The rows it spans.
     * \param[in] value True to light it, false to turn it off.
     */
    void FillRect(int16_t x, int16_t y, int16_t width, int16_t height, bool value);

    /*!
     * \brief Draws the outline of a circle.
     *
     * What lies off the panel is left out, as for SetDot(). A radius of 0 draws the centre only, a
     * negative one nothing.
     *
     * \param[in] x The column of the centre.
     * \param[in] y The row of the centre.
     * \param[in] radius The dots from the centre to the outline; 3 fills the height of the panel.
     * \param[in] value True to light the outline, false to turn it off.
     */
    void DrawCircle(int16_t x, int16_t y, int16_t radius, bool value);

    /*!
     * \brief Draws a picture from flash with its top left corner at `x`, `y`.
     *
     * The picture is a column per byte, bit 0 the top row, as a glyph of a font: a 0 bit turns its
     * dot off. Rows below `height` are left as they are, and what lies off the panel is left out.
     *
     * \param[in] x The column of the left edge; it may lie off the panel.
     * \param[in] y The row of the top edge; it may lie off the panel.
     * \param[in] bitmap The columns, `width` bytes in `PROGMEM`.
     * \param[in] width The columns of the picture.
     * \param[in] height The rows of the picture, 1 to 8.
     */
    void DrawBitmap(int16_t x, int16_t y, byte const* bitmap, byte width, byte height = MAXMATRIX_ROW_NUMBER_OF_MODULE);

    /*!
     * \brief Draws one column of a picture, `height` rows from `y` on.
     *
     * What lies off the panel is left out; rows below `height` are left as they are.
     *
     * \param[in] x The column; it may lie off the panel.
     * \param[in] y The row of bit 0; it may lie off the panel.
     * \param[in] height The rows to draw, 1 to 8.
     * \param[in] value The column, bit 0 at `y`.
     */
    void DrawColumn(int16_t x, int16_t y, byte height, byte value);

    /*!
     * \brief Sets how the LEDs of every module are wired.
     *
     * The picture stays as the viewer sees it; the buffer is rewritten along.
     *
     * \param[in] module_type The type of the modules, as MD_MAX72XX names it.
     */
    void SetModuleType(MaxMatrixModuleType module_type);

    /*!
     * \brief Sets how the whole matrix is turned.
     *
     * By 180 degrees for a board mounted upside down, with the module at the input then on the
     * left; in addition to the module type. The picture stays as the viewer sees it; the
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
