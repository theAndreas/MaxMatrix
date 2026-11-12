// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as the viewer sees it.
 *
 * Columns, rows and dots in the viewer's coordinates, on top of the matrix as
 * it is wired. Drawing only changes the buffer; Matrix decides when it is
 * sent.
 */

#ifndef MAXMATRIX_SRC_MATRIX_PANEL_H_
#define MAXMATRIX_SRC_MATRIX_PANEL_H_

#include "Arduino.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "Wired.h"

namespace max_matrix {

/*! \brief How the whole matrix is turned: by 180 degrees for a board mounted
 * upside down. */
enum class MatrixOrientation : uint8_t {
  k0,  /*!< The board as it is meant to be mounted. */
  k180 /*!< The board upside down: the module at the input on the left. */
};

namespace internal {

/* the three properties of a ModuleType, one bit each of its value */
/*! DR1 of MD_MAX72XX's names: the digits of the MAX7219 are the rows. */
constexpr uint8_t kModuleTypeDigitsAsRows{0x04};
/*! CR1 of MD_MAX72XX's names: the columns in reverse order. */
constexpr uint8_t kModuleTypeReversedColumns{0x02};
/*! RR1 of MD_MAX72XX's names: the rows in reverse order. */
constexpr uint8_t kModuleTypeReversedRows{0x01};

}  // namespace internal

/*!
 * \brief How the LEDs of a module are wired to its MAX7219, named as MD_MAX72XX
 *        names its hardware types.
 *
 * Three properties tell the eight wirings apart, seen with the module's input
 * on the right, its columns counted from the right and its rows from the top:
 * DR1 if a digit register drives a row of LEDs and its bits the columns, DR0 if
 * a digit drives a column; CR1 if the bits run the other way, bit 7 - n for n;
 * RR1 if the digits do, digit 7 - n for n. Four of the eight have a name of
 * their own. Four are mirror images of the others, Parola among them. The value
 * holds the three properties as bits: DR in bit 2, CR in bit 1, RR in bit 0.
 */
enum class ModuleType : uint8_t {
  kDr0Cr0Rr0 = 0, /*!< Digits as columns. */
  /*! Digits as columns, in reverse; the buffer is the picture as the viewer
     sees it. */
  kDr0Cr0Rr1 = 1,
  kDr0Cr1Rr0 = 2, /*!< Digits as columns, their bits in reverse. */
  kDr0Cr1Rr1 = 3, /*!< Digits as columns, both in reverse. */
  kDr1Cr0Rr0 = 4, /*!< Digits as rows. */
  kDr1Cr0Rr1 = 5, /*!< Digits as rows, in reverse. */
  kDr1Cr1Rr0 = 6, /*!< Digits as rows, their bits in reverse. */
  kDr1Cr1Rr1 = 7, /*!< Digits as rows, both in reverse. */
  /*! The generic modules, often with the MAX7219 in a DIP package
     (`GENERIC_HW`). */
  kGeneric = kDr0Cr1Rr0,
  kFc16 =
      kDr1Cr0Rr0, /*!< FC-16, the common boards of four modules (`FC16_HW`). */
  kParola = kDr1Cr1Rr0,   /*!< The Parola modules (`PAROLA_HW`). */
  kIcstation = kDr1Cr1Rr1 /*!< The ICStation modules (`ICSTATION_HW`). */
};

/*! \brief What comes in at the edge a shift leaves empty. */
enum class ShiftFill : uint8_t {
  kZero,   /*!< Every LED of the new column or row off. */
  kRotate, /*!< The column or row that falls out at the other edge. */
  kKeep    /*!< The column or row keeps what it held. */
};

namespace internal {

/*!
 * \brief The matrix as the viewer sees it: columns, rows and dots in the
 *        viewer's coordinates.
 *
 * It turns every coordinate by the matrix orientation and the module type into
 * the buffer of Wired. A sketch reaches it through Matrix, which
 * takes most of its functions over.
 */
class Panel {
 private:
  Wired wired_;
  ModuleType module_type_{ModuleType::kDr0Cr0Rr1};
  MatrixOrientation matrix_orientation_{MatrixOrientation::k0};

  /* where a dot lies in the buffer: the buffer column and its bit */
  struct BufferDot {
    byte column;
    byte bit;
  };

  static byte ReverseByte(byte input);
  BufferDot ConvertDotToBuffer(byte column, byte row) const;
  byte GetLine(byte column, byte row, bool vertical);
  void SetLine(byte column, byte row, bool vertical, byte value);
  void SetFarDot(int32_t column, int32_t row, bool value);
  void FillArea(int32_t left, int32_t top, int32_t right, int32_t bottom,
                bool value);
  void ChangeMapping(MatrixOrientation matrix_orientation,
                     ModuleType module_type);
  byte GetFill(ShiftFill fill, byte rotated_column, byte kept_column);
  void ShiftRows(bool up, ShiftFill fill);

 public:
  /*!
   * \brief Makes the panel of the chain at these pins, of modules
   *        ModuleType::kDr0Cr0Rr1 in matrix orientation 0.
   *
   * \param[in] data_in_pin The pin DIN of the chain is connected to.
   * \param[in] chip_select_pin The pin CS of the chain is connected to.
   * \param[in] clock_pin The pin CLK of the chain is connected to.
   * \param[out] buffer The buffer, `BufferSize(number_of_modules)` bytes, which
   *                    has to live as long as the panel.
   * \param[in] number_of_modules The modules in the chain, 1 to
   *                              `kMaxNumberOfModules`.
   */
  Panel(byte data_in_pin, byte chip_select_pin, byte clock_pin, byte* buffer,
        byte number_of_modules);

  /*! \brief Returns the modules of the chain. */
  byte GetNumberOfModules() const {
    return wired_.GetChain().GetNumberOfModules();
  }

  /*! \brief Returns the columns of the panel, eight per module. */
  byte GetNumberOfColumns() const { return wired_.GetNumberOfColumns(); }

  /*! \brief Returns the matrix as it is wired, below this one. */
  Wired& GetWired() { return wired_; }

  /*! \copydoc GetWired() */
  Wired const& GetWired() const { return wired_; }

  /*! \brief Returns how the LEDs of every module are wired. */
  ModuleType GetModuleType() const { return module_type_; }

  /*! \brief Returns how the whole matrix is turned. */
  MatrixOrientation GetMatrixOrientation() const { return matrix_orientation_; }

  /*!
   * \brief Returns a column of the matrix.
   *
   * \param[in] column The column, from 0 at the left to eight times the
   *                   modules.
   * \return The column, bit 0 the top row, or Error::kOutOfRange if it lies off
   *         the panel.
   */
  Result<byte> GetColumn(int16_t column);

  /*!
   * \brief Returns a column of a module.
   *
   * \param[in] module The module, from 0 at the left.
   * \param[in] column The column within the module, 0 to 7.
   * \return The column, bit 0 the top row, or Error::kOutOfRange if the module
   *         or the column does not exist.
   */
  Result<byte> GetColumn(byte module, byte column);

  /*!
   * \brief Gives a row of the matrix, one byte per module.
   *
   * \param[in] row The row, from 0 at the top.
   * \param[out] values The row, bit 7 of each byte the leftmost column of its
   *                    module; left as it was on an error.
   * \param[in] number_of_values The bytes `values` has room for.
   * \return Error::kOk, or Error::kOutOfRange if the row lies off the panel or
   *         `values` has room for fewer bytes than there are modules.
   */
  Status GetRow(int16_t row, byte* values, byte number_of_values);

  /*!
   * \brief Returns a row of a module.
   *
   * The leftmost column is in bit 7, so that a row written as a binary literal
   * looks like the dots it stands for.
   *
   * \param[in] module The module, from 0 at the left.
   * \param[in] row The row within the module, 0 to 7.
   * \return The row, bit 7 the leftmost column, or Error::kOutOfRange if the
   *         module or the row does not exist.
   */
  Result<byte> GetRow(byte module, byte row);

  /*!
   * \brief Returns whether a dot is lit.
   *
   * \param[in] column The column, negative to the left of the panel.
   * \param[in] row The row, negative above the panel.
   * \return Whether the dot is lit, or Error::kOutOfRange if it lies off the
   *         panel.
   */
  Result<bool> GetDot(int16_t column, int16_t row);

  /*!
   * \brief Sets a column of the matrix.
   *
   * A column off the panel is left out, as for SetDot().
   *
   * \param[in] column The column, from 0 at the left to eight times the
   *                   modules.
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
   * A row off the panel is left out, as for SetDot(), and so are the modules
   * beyond the bytes given, or the bytes beyond the modules.
   *
   * \param[in] row The row, from 0 at the top.
   * \param[in] values The row, bit 7 of each byte the leftmost column of its
   *                   module.
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
   * A dot off the panel is left out rather than reported: a sprite, a line or a
   * text that lies partly off the panel is drawn all the time while something
   * moves in or out, and a sketch would only have to clip it itself. Adafruit
   * GFX's `drawPixel()` and Rust's embedded-graphics do the same.
   *
   * \param[in] column The column, negative to the left of the panel.
   * \param[in] row The row, negative above the panel.
   * \param[in] value True to light it.
   */
  void SetDot(int16_t column, int16_t row, bool value);

  /*!
   * \brief Draws a straight line from one dot to another, both ends included.
   *
   * What lies off the panel is left out, as for SetDot(); the ends may lie off
   * it.
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
   * What lies off the panel is left out, as for SetDot(). A width or height of
   * 0 or less draws nothing.
   *
   * \param[in] x The column of the left edge.
   * \param[in] y The row of the top edge.
   * \param[in] width The columns it spans.
   * \param[in] height The rows it spans.
   * \param[in] value True to light the outline, false to turn it off.
   */
  void DrawRect(int16_t x, int16_t y, int16_t width, int16_t height,
                bool value);

  /*!
   * \brief Draws a rectangle with its inside.
   *
   * What lies off the panel is left out, as for SetDot(). A width or height of
   * 0 or less draws nothing.
   *
   * \param[in] x The column of the left edge.
   * \param[in] y The row of the top edge.
   * \param[in] width The columns it spans.
   * \param[in] height The rows it spans.
   * \param[in] value True to light it, false to turn it off.
   */
  void FillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                bool value);

  /*!
   * \brief Draws the outline of a circle.
   *
   * What lies off the panel is left out, as for SetDot(). A radius of 0 draws
   * the centre only, a negative one nothing.
   *
   * \param[in] x The column of the centre.
   * \param[in] y The row of the centre.
   * \param[in] radius The dots from the centre to the outline; 3 fills the
   *                   height of the panel.
   * \param[in] value True to light the outline, false to turn it off.
   */
  void DrawCircle(int16_t x, int16_t y, int16_t radius, bool value);

  /*!
   * \brief Draws a picture from flash with its top left corner at `x`, `y`.
   *
   * The picture is a column per byte, bit 0 the top row, as a glyph of a font:
   * a 0 bit turns its dot off. Rows below `height` are left as they are, and
   * what lies off the panel is left out.
   *
   * \param[in] x The column of the left edge; it may lie off the panel.
   * \param[in] y The row of the top edge; it may lie off the panel.
   * \param[in] bitmap The columns, `width` bytes in `PROGMEM`.
   * \param[in] width The columns of the picture.
   * \param[in] height The rows of the picture, 1 to 8.
   */
  void DrawBitmap(int16_t x, int16_t y, byte const* bitmap, byte width,
                  byte height = kRowNumberOfModule);

  /*!
   * \brief Draws one column of a picture, `height` rows from `y` on.
   *
   * What lies off the panel is left out; rows below `height` are left as they
   * are.
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
  void SetModuleType(ModuleType module_type);

  /*!
   * \brief Sets how the whole matrix is turned.
   *
   * By 180 degrees for a board mounted upside down, with the module at the
   * input then on the left; in addition to the module type. The picture stays
   * as the viewer sees it; the buffer is turned along.
   *
   * \param[in] matrix_orientation 0 or 180 degrees.
   */
  void SetMatrixOrientation(MatrixOrientation matrix_orientation);

  /*!
   * \brief Shifts the whole picture one column to the left.
   *
   * \param[in] fill What comes in at the right: an empty column, the leftmost
   *                 one, or the rightmost one as it was.
   */
  void ShiftLeft(ShiftFill fill = ShiftFill::kZero);

  /*!
   * \brief Shifts the whole picture one column to the left, with a column of
   *        the sketch coming in at the right.
   *
   * A sketch that feeds a column on every step gets a band of any length, from
   * flash, a sensor or a calculation, and what falls out at the left, to keep
   * or pass on.
   *
   * \param[in] shifted_in The column that comes in, bit 0 the top row.
   * \return The column that fell out at the left, bit 0 the top row.
   */
  byte ShiftLeft(byte shifted_in);

  /*!
   * \brief Shifts the whole picture one column to the right.
   *
   * \param[in] fill What comes in at the left: an empty column, the rightmost
   *                 one, or the leftmost one as it was.
   */
  void ShiftRight(ShiftFill fill = ShiftFill::kZero);

  /*!
   * \brief Shifts the whole picture one column to the right, with a column of
   *        the sketch coming in at the left.
   *
   * \param[in] shifted_in The column that comes in, bit 0 the top row.
   * \return The column that fell out at the right, bit 0 the top row.
   */
  byte ShiftRight(byte shifted_in);

  /*!
   * \brief Shifts the whole picture one row up.
   *
   * \param[in] fill What comes in at the bottom: an empty row, the top one, or
   *                 the bottom one as it was.
   */
  void ShiftUp(ShiftFill fill = ShiftFill::kZero);

  /*!
   * \brief Shifts the whole picture one row down.
   *
   * \param[in] fill What comes in at the top: an empty row, the bottom one, or
   *                 the top one as it was.
   */
  void ShiftDown(ShiftFill fill = ShiftFill::kZero);
};

}  // namespace internal
}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MATRIX_PANEL_H_
