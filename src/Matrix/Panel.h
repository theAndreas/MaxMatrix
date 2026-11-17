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
#include "MaxMatrixAttributes.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "Utility/Byte.h"
#include "Utility/span.h"
#include "Wired.h"

namespace max_matrix {

/*! \brief How the whole matrix is turned: by 180 degrees for a board mounted
 * upside down. */
enum class MatrixOrientation : uint8_t {
  k0,  /*!< The board as it is meant to be mounted. */
  k180 /*!< The board upside down: the module at the input on the left. */
};

/*!
 * \brief A module of the chain, from 0 at the left: `max_matrix::Module{1}`.
 *
 * A type of its own beside the column or row within it, so that a sketch
 * cannot swap the two unnoticed (CodingStyle.md FN-16); explicit, as a bare
 * number would be a column again.
 */
struct Module {
  /*!
   * \brief Names the module.
   *
   * \param[in] index The module, from 0 at the left.
   */
  constexpr explicit Module(uint8_t index) : number{index} {}

  uint8_t number; /*!< The module, from 0 at the left. */
};

/*!
 * \brief A column of the panel, from 0 at the left, across all modules:
 *        `max_matrix::X{3}`.
 *
 * A type of its own beside Y, so that a sketch cannot swap the column and the
 * row of a Point unnoticed (CodingStyle.md FN-16); explicit, as a bare number
 * would be either again.
 */
struct X {
  /*!
   * \brief Names the column.
   *
   * \param[in] column The column, negative to the left of the panel.
   */
  constexpr explicit X(int16_t column) : number{column} {}

  int16_t number; /*!< The column, negative to the left of the panel. */
};

/*!
 * \brief A row of the panel, from 0 at the top: `max_matrix::Y{2}`.
 *
 * A type of its own beside X, as X says.
 */
struct Y {
  /*!
   * \brief Names the row.
   *
   * \param[in] row The row, negative above the panel.
   */
  constexpr explicit Y(int16_t row) : number{row} {}

  int16_t number; /*!< The row, negative above the panel. */
};

/*!
 * \brief A dot of the panel, its column and row as the viewer sees them:
 *        `max_matrix::Point{max_matrix::X{3}, max_matrix::Y{2}}`.
 *
 * The column comes first, and each takes its type, so that a point with the
 * two swapped does not compile.
 */
struct Point {
  // A constructor rather than an aggregate: avr-gcc 7.3 kept constant points
  // built as an aggregate in RAM, 8 bytes in Shapes.
  /*!
   * \brief Names the dot.
   *
   * \param[in] column The column, negative to the left of the panel.
   * \param[in] row The row, negative above the panel.
   */
  constexpr Point(X column, Y row) : x{column}, y{row} {}

  X x; /*!< The column, negative to the left of the panel. */
  Y y; /*!< The row, negative above the panel. */
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
 public:
  /*!
   * \brief Makes the panel of the chain at these pins, of modules
   *        ModuleType::kDr0Cr0Rr1 in matrix orientation 0.
   *
   * \param[in] data_in_pin The pin DIN of the chain is connected to.
   * \param[in] chip_select_pin The pin CS of the chain is connected to.
   * \param[in] clock_pin The pin CLK of the chain is connected to.
   * \param[out] buffer The buffer, `BufferSize()` of the modules in the chain,
   *                    1 to `kMaxNumberOfModules`; it has to live as long as
   *                    the panel.
   */
  Panel(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
        ClockPin clock_pin, span<Byte> buffer);

  /*! \brief Returns the modules of the chain. */
  MAXMATRIX_NODISCARD uint8_t GetNumberOfModules() const {
    return wired_.GetNumberOfModules();
  }

  /*! \brief Returns the columns of the panel, eight per module. */
  MAXMATRIX_NODISCARD uint8_t GetNumberOfColumns() const {
    return wired_.GetNumberOfColumns();
  }

  /*! \brief Returns the matrix as it is wired, below this one. */
  Wired& GetWired() { return wired_; }

  /*! \copydoc GetWired() */
  MAXMATRIX_NODISCARD Wired const& GetWired() const { return wired_; }

  /*! \brief Returns how the LEDs of every module are wired. */
  MAXMATRIX_NODISCARD ModuleType GetModuleType() const { return module_type_; }

  /*! \brief Returns how the whole matrix is turned. */
  MAXMATRIX_NODISCARD MatrixOrientation GetMatrixOrientation() const {
    return matrix_orientation_;
  }

  /*!
   * \brief Returns a column of the matrix.
   *
   * \param[in] x The column, from 0 at the left to eight times the modules.
   * \return The column, bit 0 the top row, or Error::kOutOfRange if it lies off
   *         the panel.
   */
  MAXMATRIX_NODISCARD Result<Byte> GetColumn(X x) const;

  /*!
   * \brief Returns a column of a module.
   *
   * \param[in] module The module, from 0 at the left.
   * \param[in] column The column within the module, 0 to 7.
   * \return The column, bit 0 the top row, or Error::kOutOfRange if the module
   *         or the column does not exist.
   */
  MAXMATRIX_NODISCARD Result<Byte> GetColumn(Module module,
                                             uint8_t column) const;

  /*!
   * \brief Gives a row of the matrix, one byte per module.
   *
   * \param[in] y The row, from 0 at the top.
   * \param[out] values The row, bit 7 of each byte the leftmost column of its
   *                    module; left as it was on an error.
   * \return Error::kOk, or Error::kOutOfRange if the row lies off the panel or
   *         `values` has room for fewer bytes than there are modules.
   */
  MAXMATRIX_NODISCARD Status GetRow(Y y, span<Byte> values) const;

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
  MAXMATRIX_NODISCARD Result<Byte> GetRow(Module module, uint8_t row) const;

  /*!
   * \brief Returns whether a dot is lit.
   *
   * \param[in] dot The dot.
   * \return Whether the dot is lit, or Error::kOutOfRange if it lies off the
   *         panel.
   */
  MAXMATRIX_NODISCARD Result<bool> GetDot(Point dot) const;

  /*!
   * \brief Sets a column of the matrix.
   *
   * A column off the panel is left out, as for SetDot().
   *
   * \param[in] x The column, from 0 at the left to eight times the modules.
   * \param[in] value The column, bit 0 the top row.
   */
  void SetColumn(X x, Byte value);

  /*!
   * \brief Sets a column of a module.
   *
   * A module or a column that does not exist is left out.
   *
   * \param[in] module The module, from 0 at the left.
   * \param[in] column The column within the module, 0 to 7.
   * \param[in] value The column, bit 0 the top row.
   */
  void SetColumn(Module module, uint8_t column, Byte value);

  /*!
   * \brief Sets a row of the matrix, one byte per module.
   *
   * A row off the panel is left out, as for SetDot(), and so are the modules
   * beyond the bytes given, or the bytes beyond the modules.
   *
   * \param[in] y The row, from 0 at the top.
   * \param[in] values The row, bit 7 of each byte the leftmost column of its
   *                   module.
   */
  void SetRow(Y y, span<Byte const> values);

  /*!
   * \brief Sets a row of a module.
   *
   * A module or a row that does not exist is left out.
   *
   * \param[in] module The module, from 0 at the left.
   * \param[in] row The row within the module, 0 to 7.
   * \param[in] value The row, bit 7 the leftmost column.
   */
  void SetRow(Module module, uint8_t row, Byte value);

  /*!
   * \brief Lights a dot or turns it off.
   *
   * A dot off the panel is left out rather than reported: a sprite, a line or a
   * text that lies partly off the panel is drawn all the time while something
   * moves in or out, and a sketch would only have to clip it itself. Adafruit
   * GFX's `drawPixel()` and Rust's embedded-graphics do the same.
   *
   * \param[in] dot The dot; it may lie off the panel.
   * \param[in] value True to light it.
   */
  void SetDot(Point dot, bool value) {
    SetDotAt(dot.x.number, dot.y.number, value);
  }

  /*!
   * \brief Draws a straight line from one dot to another, both ends included.
   *
   * What lies off the panel is left out, as for SetDot(); the ends may lie off
   * it.
   *
   * \param[in] from The first end.
   * \param[in] to The second end; swapped with `from`, the line is the same.
   * \param[in] value True to light the line, false to turn it off.
   */
  void DrawLine(Point from, Point to, bool value);

  /*!
   * \brief Draws the outline of a rectangle, given by two opposite corners.
   *
   * Both corners are part of the outline, and either may come first: any two
   * opposite corners give the same rectangle. A rectangle of one row or column
   * is a line. What lies off the panel is left out, as for SetDot().
   *
   * \param[in] corner A corner.
   * \param[in] opposite_corner The corner opposite it.
   * \param[in] value True to light the outline, false to turn it off.
   */
  void DrawRect(Point corner, Point opposite_corner, bool value);

  /*!
   * \brief Draws a rectangle with its inside, given by two opposite corners.
   *
   * Both corners are part of it, and either may come first, as for
   * DrawRect(). What lies off the panel is left out, as for SetDot().
   *
   * \param[in] corner A corner.
   * \param[in] opposite_corner The corner opposite it.
   * \param[in] value True to light it, false to turn it off.
   */
  void FillRect(Point corner, Point opposite_corner, bool value);

  /*!
   * \brief Draws the outline of a circle.
   *
   * What lies off the panel is left out, as for SetDot(). A radius of 0 draws
   * the centre only, a negative one nothing.
   *
   * \param[in] centre The centre.
   * \param[in] radius The dots from the centre to the outline; 3 fills the
   *                   height of the panel.
   * \param[in] value True to light the outline, false to turn it off.
   */
  void DrawCircle(Point centre, int16_t radius, bool value);

  /*!
   * \brief Draws a picture from flash with its top left corner at a dot.
   *
   * The picture is a column per byte, bit 0 the top row, as a glyph of a font:
   * a 0 bit turns its dot off. Rows below `height` are left as they are, and
   * what lies off the panel is left out.
   *
   * The height goes before the bitmap, so that a width after it, as the
   * columns were once given, does not compile: the bitmap brings its width.
   *
   * \param[in] top_left The top left corner; it may lie off the panel.
   * \param[in] height The rows of the picture, 1 to 8.
   * \param[in] bitmap The columns, in `PROGMEM`; as many as it holds.
   */
  void DrawBitmap(Point top_left, uint8_t height, span<uint8_t const> bitmap);

  /*!
   * \brief Draws one column of a picture, `height` rows from `top` down.
   *
   * What lies off the panel is left out; rows below `height` are left as they
   * are.
   *
   * \param[in] top The dot of bit 0; it may lie off the panel.
   * \param[in] height The rows to draw, 1 to 8.
   * \param[in] value The column, bit 0 at `top`.
   */
  void DrawColumn(Point top, uint8_t height, Byte value);

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
  Byte ShiftLeft(Byte shifted_in);

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
  Byte ShiftRight(Byte shifted_in);

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

 private:
  /* where a dot lies in the buffer: the buffer column and its bit */
  struct BufferDot {
    uint8_t column;
    uint8_t bit;
  };

  /*
   * Whether a column, a row or a dot lies on the panel, or a column or row
   * within a module. Templates, as SetFarDot() checks its sums as int32_t and
   * the rest int16_t, which is cheaper on the Uno.
   */
  template <typename Coordinate>
  MAXMATRIX_NODISCARD bool HasColumn(Coordinate column) const {
    return (column >= 0) && (column < GetNumberOfColumns());
  }
  template <typename Coordinate>
  MAXMATRIX_NODISCARD static constexpr bool HasRow(Coordinate row) {
    return (row >= 0) && (row < kRowNumberOfModule);
  }
  template <typename Coordinate>
  MAXMATRIX_NODISCARD bool HasDot(Coordinate column, Coordinate row) const {
    return HasColumn(column) && HasRow(row);
  }
  MAXMATRIX_NODISCARD bool HasModuleColumn(Module module,
                                           uint8_t column) const {
    return (module.number < GetNumberOfModules()) &&
           (column < kColumnNumberOfModule);
  }
  MAXMATRIX_NODISCARD bool HasModuleRow(Module module, uint8_t row) const {
    return (module.number < GetNumberOfModules()) && (row < kRowNumberOfModule);
  }

  static Byte ReverseByte(Byte input);
  MAXMATRIX_NODISCARD BufferDot ConvertDotToBuffer(uint8_t column,
                                                   uint8_t row) const;
  MAXMATRIX_NODISCARD Byte GetLine(uint8_t column, uint8_t row,
                                   bool vertical) const;
  void SetLine(uint8_t column, uint8_t row, bool vertical, Byte value);
  /*
   * SetDot() with its point unpacked, inline, before the call: avr-gcc 7.3
   * passes a Point to a function it does not inline as one value of four
   * bytes, and every caller that built one paid for it, 88 bytes of flash in
   * Basics and 30 in SerialControl. Only SetDot() calls it.
   */
  void SetDotAt(int16_t column, int16_t row, bool value);
  void SetFarDot(int32_t column, int32_t row, bool value);
  void ChangeMapping(MatrixOrientation matrix_orientation,
                     ModuleType module_type);
  Byte GetFill(ShiftFill fill, uint8_t rotated_column, uint8_t kept_column);
  void ShiftRows(bool up, ShiftFill fill);

  Wired wired_;
  ModuleType module_type_{ModuleType::kDr0Cr0Rr1};
  MatrixOrientation matrix_orientation_{MatrixOrientation::k0};
};

}  // namespace internal
}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MATRIX_PANEL_H_
