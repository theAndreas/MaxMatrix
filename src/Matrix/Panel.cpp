// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as the viewer sees it.
 *
 * Columns, rows and dots in the viewer's coordinates, mapped onto the buffer as
 * wired by the matrix orientation and the module type.
 */

#include "Panel.h"

#include <avr/pgmspace.h>
#include <stddef.h>
#include <stdint.h>

#include "Max7219/Chain.h"
#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "Utility/Bit.h"
#include "Utility/Byte.h"
#include "Utility/array.h"
#include "Utility/span.h"

namespace max_matrix {
namespace internal {

namespace {

/* a line of the panel is a byte, one dot per bit */
constexpr uint8_t kDotsPerLine{8};

/* the upper bits of each half, pair and neighbour of a byte, which
 * ReverseByte() swaps with the lower ones */
constexpr Byte kUpperHalf{0xF0};
constexpr Byte kUpperPairs{0xCC};
constexpr Byte kUpperBits{0xAA};

// Returns `value` with the bits under `upper` swapped with those `shift` places
// below them.
Byte SwapBits(Byte value, Byte upper, uint8_t shift) {
  Byte const high{value & upper};
  Byte const low{value & ~upper};
  return (high >> shift) | (low << shift);
}

// The lesser and the greater of two coordinates: avr-gcc has no <algorithm>,
// and the core's min() and max() are macros.
constexpr int16_t GetLesser(int16_t first, int16_t second) {
  return (first < second) ? first : second;
}

constexpr int16_t GetGreater(int16_t first, int16_t second) {
  return (first > second) ? first : second;
}

}  // namespace

Panel::Panel(DataInPin data_in_pin, ChipSelectPin chip_select_pin,
             ClockPin clock_pin, span<Byte> buffer)
    : wired_{data_in_pin, chip_select_pin, clock_pin, buffer} {}

Result<Byte> Panel::GetColumn(X x) const {
  int16_t const column{x.number};

  if (HasColumn(column)) {
    return GetLine(static_cast<uint8_t>(column), 0, true);
  }
  return Error::kOutOfRange;
}

Result<Byte> Panel::GetColumn(Module module, uint8_t column) const {
  if (HasModuleColumn(module, column)) {
    return GetColumn(X{static_cast<int16_t>(
        (module.number * kColumnNumberOfModule) + column)});
  }
  return Error::kOutOfRange;
}

void Panel::SetColumn(X x, Byte value) {
  int16_t const column{x.number};

  if (HasColumn(column)) {
    SetLine(static_cast<uint8_t>(column), 0, true, value);
  }
}

void Panel::SetColumn(Module module, uint8_t column, Byte value) {
  // The column is checked against the module, not the panel, or column 8 of
  // module 0 would be drawn on module 1.
  if (HasModuleColumn(module, column)) {
    SetColumn(X{static_cast<int16_t>((module.number * kColumnNumberOfModule) +
                                     column)},
              value);
  }
}

Status Panel::GetRow(Y y, span<Byte> values) const {
  int16_t const row{y.number};

  if (HasRow(row) && (values.size() >= GetNumberOfModules())) {
    uint8_t const modules{GetNumberOfModules()};
    for (uint8_t module{0}; module < modules; module++) {
      values[module] =
          GetRow(Module{module}, static_cast<uint8_t>(row)).GetValue();
    }
    return Error::kOk;
  }
  return Error::kOutOfRange;
}

Result<Byte> Panel::GetRow(Module module, uint8_t row) const {
  if (HasModuleRow(module, row)) {
    return ReverseByte(
        GetLine(module.number * kColumnNumberOfModule, row, false));
  }
  return Error::kOutOfRange;
}

void Panel::SetRow(Y y, span<Byte const> values) {
  int16_t const row{y.number};

  if (HasRow(row)) {
    uint8_t const modules{GetNumberOfModules()};
    for (uint8_t module{0}; (module < modules) && (module < values.size());
         module++) {
      SetRow(Module{module}, static_cast<uint8_t>(row), values[module]);
    }
  }
}

void Panel::SetRow(Module module, uint8_t row, Byte value) {
  if (HasModuleRow(module, row)) {
    SetLine(module.number * kColumnNumberOfModule, row, false,
            ReverseByte(value));
  }
}

Result<bool> Panel::GetDot(Point dot) const {
  int16_t const column{dot.x.number};
  int16_t const row{dot.y.number};

  if (HasDot(column, row)) {
    BufferDot const buffer_dot{ConvertDotToBuffer(static_cast<uint8_t>(column),
                                                  static_cast<uint8_t>(row))};
    return ReadBit(wired_.GetColumn(buffer_dot.column), buffer_dot.bit);
  }
  return Error::kOutOfRange;
}

void Panel::SetDotAt(int16_t column, int16_t row, bool value) {
  if (HasDot(column, row)) {
    BufferDot const buffer_dot{ConvertDotToBuffer(static_cast<uint8_t>(column),
                                                  static_cast<uint8_t>(row))};
    wired_.StoreBit(buffer_dot.column, buffer_dot.bit, value);
  }
}

/*
 * Bresenham's line, in all eight directions: `error` says how far the dot drawn
 * is off the true line, and each step goes along x, along y or both, whichever
 * keeps it small. The sums are int32_t, as the ends may lie further apart than
 * an int16_t holds, and an int is 16 bits on the Uno.
 */
void Panel::DrawLine(Point from, Point to, bool value) {
  int16_t const x0{from.x.number};
  int16_t const y0{from.y.number};
  int16_t const x1{to.x.number};
  int16_t const y1{to.y.number};
  int32_t const width{(x1 > x0) ? (static_cast<int32_t>(x1) - x0)
                                : (static_cast<int32_t>(x0) - x1)};
  int32_t const height{(y1 > y0) ? (static_cast<int32_t>(y0) - y1)
                                 : (static_cast<int32_t>(y1) - y0)};
  int8_t const step_x{static_cast<int8_t>((x0 < x1) ? 1 : -1)};
  int8_t const step_y{static_cast<int8_t>((y0 < y1) ? 1 : -1)};
  int32_t x{x0};
  int32_t y{y0};
  int32_t error{width + height};

  for (;;) {
    SetFarDot(x, y, value);
    if ((x == x1) && (y == y1)) {
      break;
    }
    int32_t const twice{2 * error};
    if (twice >= height) {
      error += height;
      x += step_x;
    }
    if (twice <= width) {
      error += width;
      y += step_y;
    }
  }
}

/*
 * The four edges, each a rectangle of one row or column between two corners:
 * the corners beside `corner` share its row or its column.
 */
void Panel::DrawRect(Point corner, Point opposite_corner, bool value) {
  int16_t const x0{corner.x.number};
  int16_t const y0{corner.y.number};
  int16_t const x1{opposite_corner.x.number};
  int16_t const y1{opposite_corner.y.number};
  Point const in_its_row{X{x1}, Y{y0}};
  Point const in_its_column{X{x0}, Y{y1}};

  FillRect(corner, in_its_row, value);
  FillRect(corner, in_its_column, value);
  FillRect(in_its_row, opposite_corner, value);
  FillRect(in_its_column, opposite_corner, value);
}

// Only the part on the panel is walked, so that a corner far off it costs no
// time, and the loops cannot overflow.
void Panel::FillRect(Point corner, Point opposite_corner, bool value) {
  auto const last_column{static_cast<int16_t>(GetNumberOfColumns() - 1)};
  int16_t const last_row{kRowNumberOfModule - 1};
  int16_t const left{
      GetGreater(GetLesser(corner.x.number, opposite_corner.x.number), 0)};
  int16_t const right{GetLesser(
      GetGreater(corner.x.number, opposite_corner.x.number), last_column)};
  int16_t const top{
      GetGreater(GetLesser(corner.y.number, opposite_corner.y.number), 0)};
  int16_t const bottom{GetLesser(
      GetGreater(corner.y.number, opposite_corner.y.number), last_row)};

  for (int16_t column{left}; column <= right; column++) {
    for (int16_t row{top}; row <= bottom; row++) {
      SetDot(Point{X{column}, Y{row}}, value);
    }
  }
}

/*
 * The midpoint circle: one eighth of the outline is walked from the top down to
 * the diagonal, and every dot of it is mirrored into the other seven. `error`
 * says whether the next dot lies inside the true circle, and so whether the
 * step goes inwards as well.
 */
void Panel::DrawCircle(Point centre, int16_t radius, bool value) {
  if (radius < 0) {
    return;
  }
  int16_t const x{centre.x.number};
  int16_t const y{centre.y.number};
  int32_t outer{radius};
  int32_t inner{0};
  int32_t error{1 - outer};

  while (outer >= inner) {
    SetFarDot(x + outer, y + inner, value);
    SetFarDot(x - outer, y + inner, value);
    SetFarDot(x + outer, y - inner, value);
    SetFarDot(x - outer, y - inner, value);
    SetFarDot(x + inner, y + outer, value);
    SetFarDot(x - inner, y + outer, value);
    SetFarDot(x + inner, y - outer, value);
    SetFarDot(x - inner, y - outer, value);
    inner++;
    if (error < 0) {
      error += (2 * inner) + 1;
    } else {
      outer--;
      error += (2 * (inner - outer)) + 1;
    }
  }
}

/*
 * Right of the panel nothing is drawn: the loop ends at its right edge, so that
 * the column on the panel, x plus a column of the bitmap, cannot overflow. The
 * point is unpacked first and a new one made for each column, here and in
 * DrawColumn(): with a part of the point passed on, avr-gcc 7.3 no longer saw
 * that a text's row is 0, 120 bytes of flash in ScrollText.
 */
void Panel::DrawBitmap(Point top_left, uint8_t height,
                       span<uint8_t const> bitmap) {
  int16_t left{top_left.x.number};
  int16_t const top{top_left.y.number};

  for (size_t column{0};
       (column < bitmap.size()) && (left < GetNumberOfColumns()); column++) {
    DrawColumn(Point{X{left}, Y{top}}, height,
               Byte{pgm_read_byte(&bitmap[column])});
    left++;
  }
}

/*
 * A whole column at once if it fills one, otherwise dot by dot, as the rows
 * outside it are not its own. A column that starts eight rows or more off the
 * panel draws nothing, and one that starts nearer cannot overflow y plus a row.
 */
void Panel::DrawColumn(Point top, uint8_t height, Byte value) {
  int16_t const x{top.x.number};
  int16_t const y{top.y.number};

  if ((height == kRowNumberOfModule) && (y == 0)) {
    SetColumn(X{x}, value);
  } else if ((y > -kRowNumberOfModule) && (y < kRowNumberOfModule)) {
    for (uint8_t row{0}; row < height; row++) {
      SetDot(Point{X{x}, Y{static_cast<int16_t>(y + row)}},
             ReadBit(value, row));
    }
  }
}

// SetDot() for a dot whose coordinates were added up past what an int16_t
// holds.
void Panel::SetFarDot(int32_t column, int32_t row, bool value) {
  if (HasDot(column, row)) {
    SetDotAt(static_cast<int16_t>(column), static_cast<int16_t>(row), value);
  }
}

/*
 * The shifts work on the picture as the viewer sees it, a column at a time, so
 * that they are the same for every module type and orientation; in place, as a
 * copy of the whole picture would need its length at compile time.
 */
void Panel::ShiftLeft(ShiftFill fill) {
  ShiftLeft(GetFill(fill, 0, static_cast<uint8_t>(GetNumberOfColumns() - 1)));
}

Byte Panel::ShiftLeft(Byte shifted_in) {
  uint8_t const last{static_cast<uint8_t>(GetNumberOfColumns() - 1)};
  Byte const shifted_out{GetLine(0, 0, true)};

  for (uint8_t panel_column{0}; panel_column < last; panel_column++) {
    SetLine(panel_column, 0, true, GetLine(panel_column + 1, 0, true));
  }
  SetLine(last, 0, true, shifted_in);
  return shifted_out;
}

void Panel::ShiftRight(ShiftFill fill) {
  ShiftRight(GetFill(fill, static_cast<uint8_t>(GetNumberOfColumns() - 1), 0));
}

Byte Panel::ShiftRight(Byte shifted_in) {
  uint8_t const last{static_cast<uint8_t>(GetNumberOfColumns() - 1)};
  Byte const shifted_out{GetLine(last, 0, true)};

  for (uint8_t panel_column{last}; panel_column > 0; panel_column--) {
    SetLine(panel_column, 0, true, GetLine(panel_column - 1, 0, true));
  }
  SetLine(0, 0, true, shifted_in);
  return shifted_out;
}

void Panel::ShiftUp(ShiftFill fill) { ShiftRows(true, fill); }

void Panel::ShiftDown(ShiftFill fill) { ShiftRows(false, fill); }

/*
 * The column a shift to the left or right takes in for `fill`: none lit, the
 * column at the edge it leaves, which falls out, or the one at the edge it
 * comes in at, as it is.
 */
Byte Panel::GetFill(ShiftFill fill, uint8_t rotated_column,
                    uint8_t kept_column) {
  if (fill == ShiftFill::kRotate) {
    return GetLine(rotated_column, 0, true);
  }
  if (fill == ShiftFill::kKeep) {
    return GetLine(kept_column, 0, true);
  }
  return Byte{};
}

/*
 * Shifts every column by a bit, up or down: the row that falls out at one edge
 * comes in at the other for kRotate, and the row at the edge that is left keeps
 * its dot for kKeep.
 */
void Panel::ShiftRows(bool up, ShiftFill fill) {
  uint8_t const columns{GetNumberOfColumns()};
  uint8_t const top{0};
  uint8_t const bottom{kRowNumberOfModule - 1};

  for (uint8_t column{0}; column < columns; column++) {
    Byte const value{GetLine(column, 0, true)};
    uint8_t const fill_row{up ? bottom : top};
    bool fill_dot{false};
    if (fill == ShiftFill::kRotate) {
      fill_dot = ReadBit(value, up ? top : bottom);
    } else if (fill == ShiftFill::kKeep) {
      fill_dot = ReadBit(value, fill_row);
    }
    Byte const shifted{up ? (value >> 1u) : (value << 1u)};
    SetLine(column, 0, true, WriteBit(shifted, fill_row, fill_dot));
  }
}

void Panel::SetModuleType(ModuleType module_type) {
  ChangeMapping(matrix_orientation_, module_type);
}

void Panel::SetMatrixOrientation(MatrixOrientation matrix_orientation) {
  ChangeMapping(matrix_orientation, module_type_);
}

/*
 * Maps the panel anew, to a matrix orientation and a module type, so that the
 * viewer sees the same picture, rewriting the buffer a pair of modules at a
 * time: the viewer's modules m and n - 1 - m lie in the buffer's modules m and
 * n - 1 - m whatever the mapping, as the matrix orientation only swaps them and
 * the module type rearranges each in its place. So the pair is read in the old
 * mapping and written in the new without touching any other module, and 16
 * bytes do for any chain, where a copy of the whole picture would need its
 * length at compile time.
 */
void Panel::ChangeMapping(MatrixOrientation matrix_orientation,
                          ModuleType module_type) {
  MatrixOrientation const old_matrix_orientation{matrix_orientation_};
  ModuleType const old_module_type{module_type_};

  uint8_t const modules{GetNumberOfModules()};
  // a module in the middle of an odd chain pairs with itself
  auto const pairs{static_cast<uint8_t>((modules + 1) / 2)};

  for (uint8_t left{0}; left < pairs; left++) {
    auto const right{static_cast<uint8_t>(modules - 1 - left)};
    uint8_t const left_column{
        static_cast<uint8_t>(left * kColumnNumberOfModule)};
    uint8_t const right_column{
        static_cast<uint8_t>(right * kColumnNumberOfModule)};
    /*
     * T-17: the columns are kept as numbers, not as Byte: avr-gcc 7.3 walks an
     * array of a class by a 16-bit index rather than a pointer, 80 bytes of
     * flash more for this function on the Uno. The array does not leave it.
     * T-13: not initialized, as the loop below fills every element before one
     * is read; cleared first, it made every example 10 to 12 bytes larger.
     */
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-member-init): T-13, see above
    array<uint8_t, 2 * kColumnNumberOfModule> pair;

    matrix_orientation_ = old_matrix_orientation;
    module_type_ = old_module_type;
    for (uint8_t column{0}; column < kColumnNumberOfModule; column++) {
      pair[column] = GetLine(left_column + column, 0, true).ToInteger();
      pair[kColumnNumberOfModule + column] =
          GetLine(right_column + column, 0, true).ToInteger();
    }
    matrix_orientation_ = matrix_orientation;
    module_type_ = module_type;
    for (uint8_t column{0}; column < kColumnNumberOfModule; column++) {
      SetLine(left_column + column, 0, true, Byte{pair[column]});
      SetLine(right_column + column, 0, true,
              Byte{pair[kColumnNumberOfModule + column]});
    }
  }
}

// Returns `input` with its bits in the opposite order: the halves swapped, then
// the pairs in each half, then the bits in each pair.
Byte Panel::ReverseByte(Byte input) {
  Byte const halves_swapped{SwapBits(input, kUpperHalf, 4)};
  Byte const pairs_swapped{SwapBits(halves_swapped, kUpperPairs, 2)};
  return SwapBits(pairs_swapped, kUpperBits, 1);
}

/*
 * Finds where a dot as the viewer sees it is kept: its column in the matrix
 * buffer and its bit there. The matrix orientation turns the whole panel first;
 * then the module type says which digit register and which of its bits drive
 * the dot within its module, as MD_MAX72XX describes a module from its input on
 * the right. A buffer column is a digit register, its bit a segment. The dot
 * has to lie within the matrix.
 *
 * It is the only function that knows the matrix orientation and the module
 * type. While every public function handled the orientations itself, rows,
 * shifts and dots disagreed in 90, 180 and 270 degrees. Three properties rather
 * than a case for each of the eight types, so that a mirrored module costs no
 * code of its own.
 */
// FN-16: private; its callers pass a column and a row on the panel that they
// checked or walk, by those names
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Panel::BufferDot Panel::ConvertDotToBuffer(uint8_t column, uint8_t row) const {
  if (matrix_orientation_ == MatrixOrientation::k180) {
    column = static_cast<uint8_t>(GetNumberOfColumns() - 1 - column);
    row = kRowNumberOfModule - 1 - row;
  }

  uint8_t const module{static_cast<uint8_t>(column / kColumnNumberOfModule)};
  uint8_t const column_from_right{static_cast<uint8_t>(
      kColumnNumberOfModule - 1 - (column % kColumnNumberOfModule))};
  uint8_t const type{static_cast<uint8_t>(module_type_)};
  bool const digits_as_rows{Bit<uint8_t>::ReadBitGroup(
                                type, internal::kModuleTypeDigitsAsRows) != 0u};
  uint8_t digit{digits_as_rows ? row : column_from_right};
  uint8_t segment{digits_as_rows ? column_from_right : row};

  if (Bit<uint8_t>::ReadBitGroup(type, internal::kModuleTypeReversedRows) !=
      0u) {
    digit = kColumnNumberOfModule - 1 - digit;
  }
  if (Bit<uint8_t>::ReadBitGroup(type, internal::kModuleTypeReversedColumns) !=
      0u) {
    segment = kRowNumberOfModule - 1 - segment;
  }
  return {static_cast<uint8_t>((module * kColumnNumberOfModule) + digit),
          segment};
}

/*
 * Returns eight dots as the viewer sees them, from `column`, `row` on to the
 * right, or downwards if `vertical`; the first in bit 0. The line has to lie
 * within the matrix.
 */
Byte Panel::GetLine(uint8_t column, uint8_t row, bool vertical) const {
  Byte value{};

  for (uint8_t dot{0}; dot < kDotsPerLine; dot++) {
    BufferDot const buffer_dot{ConvertDotToBuffer(
        vertical ? column : (column + dot), vertical ? (row + dot) : row)};
    value =
        WriteBit(value, dot,
                 ReadBit(wired_.GetColumn(buffer_dot.column), buffer_dot.bit));
  }
  return value;
}

/*
 * Sets eight dots as the viewer sees them, from `column`, `row` on to the
 * right, or downwards if `vertical`; the first in bit 0 of `value`. The line
 * has to lie within one module.
 */
void Panel::SetLine(uint8_t column, uint8_t row, bool vertical, Byte value) {
  for (uint8_t dot{0}; dot < kDotsPerLine; dot++) {
    BufferDot const buffer_dot{ConvertDotToBuffer(
        vertical ? column : (column + dot), vertical ? (row + dot) : row)};
    wired_.StoreBit(buffer_dot.column, buffer_dot.bit, ReadBit(value, dot));
  }
}

}  // namespace internal
}  // namespace max_matrix
