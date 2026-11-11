// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The matrix as the viewer sees it.
 *
 * Columns, rows and dots in the viewer's coordinates, mapped onto the buffer as
 * wired by the matrix orientation and the module type.
 */

#include "MaxMatrixPanel.h"

namespace max_matrix {
namespace internal {

Panel::Panel(byte data_in_pin, byte chip_select_pin, byte clock_pin,
             byte* buffer, byte number_of_modules)
    : wired_(data_in_pin, chip_select_pin, clock_pin, buffer,
             number_of_modules) {
  module_type_ = ModuleType::kDr0Cr0Rr1;
  matrix_orientation_ = MatrixOrientation::k0;
}

Result<byte> Panel::GetColumn(int16_t column) {
  if (column >= 0 && column < GetNumberOfColumns())
    return GetLine(static_cast<byte>(column), 0, true);
  else
    return Error::kOutOfRange;
}

Result<byte> Panel::GetColumn(byte module, byte column) {
  if (module < GetNumberOfModules() && column < kColumnNumberOfModule) {
    return GetColumn(
        static_cast<int16_t>((module * kColumnNumberOfModule) + column));
  } else {
    return Error::kOutOfRange;
  }
}

void Panel::SetColumn(int16_t column, byte value) {
  if (column >= 0 && column < GetNumberOfColumns())
    SetLine(static_cast<byte>(column), 0, true, value);
}

void Panel::SetColumn(byte module, byte column, byte value) {
  // The column is checked against the module, not the panel, or column 8 of
  // module 0 would be drawn on module 1.
  if (module < GetNumberOfModules() && column < kColumnNumberOfModule) {
    SetColumn(static_cast<int16_t>((module * kColumnNumberOfModule) + column),
              value);
  }
}

// The row is taken by reference rather than as a pointer: GetRow(module, 0)
// would otherwise be ambiguous, as a literal 0 is a null pointer as well.
Status Panel::GetRow(int16_t row, byte* values, byte number_of_values) {
  if (row >= 0 && row < kRowNumberOfModule &&
      number_of_values >= GetNumberOfModules()) {
    byte const modules = GetNumberOfModules();
    for (byte module = 0; module < modules; module++)
      values[module] = GetRow(module, static_cast<byte>(row)).GetValue();
    return Error::kOk;
  } else {
    return Error::kOutOfRange;
  }
}

Result<byte> Panel::GetRow(byte module, byte row) {
  if (module < GetNumberOfModules() && row < kRowNumberOfModule) {
    return ReverseByte(GetLine(module * kColumnNumberOfModule, row, false));
  } else {
    return Error::kOutOfRange;
  }
}

void Panel::SetRow(int16_t row, byte const* values, byte number_of_values) {
  if (row >= 0 && row < kRowNumberOfModule) {
    byte const modules = GetNumberOfModules() < number_of_values
                             ? GetNumberOfModules()
                             : number_of_values;
    for (byte module = 0; module < modules; module++)
      SetRow(module, static_cast<byte>(row), values[module]);
  }
}

void Panel::SetRow(byte module, byte row, byte value) {
  if (module < GetNumberOfModules() && row < kRowNumberOfModule) {
    SetLine(module * kColumnNumberOfModule, row, false, ReverseByte(value));
  }
}

Result<bool> Panel::GetDot(int16_t column, int16_t row) {
  byte buffer_column, buffer_bit;

  if (column >= 0 && column < GetNumberOfColumns() && row >= 0 &&
      row < kRowNumberOfModule) {
    ConvertDotToBuffer(static_cast<byte>(column), static_cast<byte>(row),
                       &buffer_column, &buffer_bit);
    return static_cast<bool>(
        bitRead(wired_.GetColumn(buffer_column), buffer_bit));
  } else {
    return Error::kOutOfRange;
  }
}

void Panel::SetDot(int16_t column, int16_t row, bool value) {
  byte buffer_column, buffer_bit;

  if (column >= 0 && column < GetNumberOfColumns() && row >= 0 &&
      row < kRowNumberOfModule) {
    ConvertDotToBuffer(static_cast<byte>(column), static_cast<byte>(row),
                       &buffer_column, &buffer_bit);
    wired_.StoreBit(buffer_column, buffer_bit, value);
  }
}

/*
 * Bresenham's line, in all eight directions: `error` says how far the dot drawn
 * is off the true line, and each step goes along x, along y or both, whichever
 * keeps it small. The sums are int32_t, as the ends may lie further apart than
 * an int16_t holds, and an int is 16 bits on the Uno.
 */
void Panel::DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                     bool value) {
  int32_t const width =
      x1 > x0 ? static_cast<int32_t>(x1) - x0 : static_cast<int32_t>(x0) - x1;
  int32_t const height =
      y1 > y0 ? static_cast<int32_t>(y0) - y1 : static_cast<int32_t>(y1) - y0;
  int8_t const step_x = x0 < x1 ? 1 : -1;
  int8_t const step_y = y0 < y1 ? 1 : -1;
  int32_t x = x0;
  int32_t y = y0;
  int32_t error = width + height;

  for (;;) {
    SetFarDot(x, y, value);
    if (x == x1 && y == y1) break;
    int32_t const twice = 2 * error;
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

void Panel::DrawRect(int16_t x, int16_t y, int16_t width, int16_t height,
                     bool value) {
  if (width <= 0 || height <= 0) return;
  int32_t const right = static_cast<int32_t>(x) + width - 1;
  int32_t const bottom = static_cast<int32_t>(y) + height - 1;

  FillArea(x, y, right, y, value);
  FillArea(x, bottom, right, bottom, value);
  FillArea(x, y, x, bottom, value);
  FillArea(right, y, right, bottom, value);
}

void Panel::FillRect(int16_t x, int16_t y, int16_t width, int16_t height,
                     bool value) {
  if (width <= 0 || height <= 0) return;
  FillArea(x, y, static_cast<int32_t>(x) + width - 1,
           static_cast<int32_t>(y) + height - 1, value);
}

/*
 * The midpoint circle: one eighth of the outline is walked from the top down to
 * the diagonal, and every dot of it is mirrored into the other seven. `error`
 * says whether the next dot lies inside the true circle, and so whether the
 * step goes inwards as well.
 */
void Panel::DrawCircle(int16_t x, int16_t y, int16_t radius, bool value) {
  if (radius < 0) return;
  int32_t outer = radius;
  int32_t inner = 0;
  int32_t error = 1 - outer;

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
      error += 2 * inner + 1;
    } else {
      outer--;
      error += 2 * (inner - outer) + 1;
    }
  }
}

// Right of the panel nothing is drawn, and left of its right edge x plus a
// column cannot overflow.
void Panel::DrawBitmap(int16_t x, int16_t y, byte const* bitmap, byte width,
                       byte height) {
  if (x >= GetNumberOfColumns()) return;
  for (byte column = 0; column < width; column++)
    DrawColumn(static_cast<int16_t>(x + column), y, height,
               pgm_read_byte(bitmap + column));
}

/*
 * A whole column at once if it fills one, otherwise dot by dot, as the rows
 * outside it are not its own. A column that starts eight rows or more off the
 * panel draws nothing, and one that starts nearer cannot overflow y plus a row.
 */
void Panel::DrawColumn(int16_t x, int16_t y, byte height, byte value) {
  if (height == kRowNumberOfModule && y == 0) {
    SetColumn(x, value);
  } else if (y > -kRowNumberOfModule && y < kRowNumberOfModule) {
    for (byte row = 0; row < height; row++)
      SetDot(x, static_cast<int16_t>(y + row), bitRead(value, row));
  }
}

// SetDot() for a dot whose coordinates were added up past what an int16_t
// holds.
void Panel::SetFarDot(int32_t column, int32_t row, bool value) {
  if (column >= 0 && column < GetNumberOfColumns() && row >= 0 &&
      row < kRowNumberOfModule) {
    SetDot(static_cast<int16_t>(column), static_cast<int16_t>(row), value);
  }
}

// Sets every dot from `left`, `top` to `right`, `bottom`, both included, that
// lies on the panel.
void Panel::FillArea(int32_t left, int32_t top, int32_t right, int32_t bottom,
                     bool value) {
  if (left < 0) left = 0;
  if (top < 0) top = 0;
  if (right >= GetNumberOfColumns()) right = GetNumberOfColumns() - 1;
  if (bottom >= kRowNumberOfModule) bottom = kRowNumberOfModule - 1;
  for (int32_t column = left; column <= right; column++) {
    for (int32_t row = top; row <= bottom; row++)
      SetDot(static_cast<int16_t>(column), static_cast<int16_t>(row), value);
  }
}

/*
 * The shifts work on the picture as the viewer sees it, a column at a time, so
 * that they are the same for every module type and orientation; in place, as a
 * copy of the whole picture would need its length at compile time.
 */
void Panel::ShiftLeft(ShiftFill fill) {
  ShiftLeft(GetFill(fill, 0, static_cast<byte>(GetNumberOfColumns() - 1)));
}

byte Panel::ShiftLeft(byte column) {
  byte const last = static_cast<byte>(GetNumberOfColumns() - 1);
  byte const shifted_out = GetLine(0, 0, true);

  for (byte panel_column = 0; panel_column < last; panel_column++)
    SetLine(panel_column, 0, true, GetLine(panel_column + 1, 0, true));
  SetLine(last, 0, true, column);
  return shifted_out;
}

void Panel::ShiftRight(ShiftFill fill) {
  ShiftRight(GetFill(fill, static_cast<byte>(GetNumberOfColumns() - 1), 0));
}

byte Panel::ShiftRight(byte column) {
  byte const last = static_cast<byte>(GetNumberOfColumns() - 1);
  byte const shifted_out = GetLine(last, 0, true);

  for (byte panel_column = last; panel_column > 0; panel_column--)
    SetLine(panel_column, 0, true, GetLine(panel_column - 1, 0, true));
  SetLine(0, 0, true, column);
  return shifted_out;
}

void Panel::ShiftUp(ShiftFill fill) { ShiftRows(true, fill); }

void Panel::ShiftDown(ShiftFill fill) { ShiftRows(false, fill); }

/*
 * The column a shift to the left or right takes in for `fill`: none lit, the
 * column at the edge it leaves, which falls out, or the one at the edge it
 * comes in at, as it is.
 */
byte Panel::GetFill(ShiftFill fill, byte rotated_column, byte kept_column) {
  if (fill == ShiftFill::kRotate) return GetLine(rotated_column, 0, true);
  if (fill == ShiftFill::kKeep) return GetLine(kept_column, 0, true);
  return 0;
}

/*
 * Shifts every column by a bit, up or down: the row that falls out at one edge
 * comes in at the other for kRotate, and the row at the edge that is left keeps
 * its dot for kKeep.
 */
void Panel::ShiftRows(bool up, ShiftFill fill) {
  byte const columns = GetNumberOfColumns();
  byte const top = 0;
  byte const bottom = kRowNumberOfModule - 1;

  for (byte column = 0; column < columns; column++) {
    byte const value = GetLine(column, 0, true);
    byte const fill_row = up ? bottom : top;
    bool const fill_dot = fill == ShiftFill::kRotate
                              ? bitRead(value, up ? top : bottom)
                          : fill == ShiftFill::kKeep ? bitRead(value, fill_row)
                                                     : false;
    byte shifted =
        up ? static_cast<byte>(value >> 1) : static_cast<byte>(value << 1);
    bitWrite(shifted, fill_row, fill_dot);
    SetLine(column, 0, true, shifted);
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
  MatrixOrientation const old_matrix_orientation = matrix_orientation_;
  ModuleType const old_module_type = module_type_;

  for (int16_t left = 0, right = static_cast<int16_t>(GetNumberOfModules() - 1);
       left <= right; left++, right--) {
    byte const left_column = static_cast<byte>(left * kColumnNumberOfModule);
    byte const right_column = static_cast<byte>(right * kColumnNumberOfModule);
    byte pair[2 * kColumnNumberOfModule];

    matrix_orientation_ = old_matrix_orientation;
    module_type_ = old_module_type;
    for (byte column = 0; column < kColumnNumberOfModule; column++) {
      pair[column] = GetLine(left_column + column, 0, true);
      pair[kColumnNumberOfModule + column] =
          GetLine(right_column + column, 0, true);
    }
    matrix_orientation_ = matrix_orientation;
    module_type_ = module_type;
    for (byte column = 0; column < kColumnNumberOfModule; column++) {
      SetLine(left_column + column, 0, true, pair[column]);
      SetLine(right_column + column, 0, true,
              pair[kColumnNumberOfModule + column]);
    }
  }
}

// Returns `input` with its bits in the opposite order.
byte Panel::ReverseByte(byte input) {
  input = static_cast<byte>((input & 0xF0) >> 4 | (input & 0x0F) << 4);
  input = static_cast<byte>((input & 0xCC) >> 2 | (input & 0x33) << 2);
  input = static_cast<byte>((input & 0xAA) >> 1 | (input & 0x55) << 1);

  return input;
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
void Panel::ConvertDotToBuffer(byte column, byte row, byte* buffer_column,
                               byte* buffer_bit) {
  if (matrix_orientation_ == MatrixOrientation::k180) {
    column = static_cast<byte>(GetNumberOfColumns() - 1 - column);
    row = kRowNumberOfModule - 1 - row;
  }

  byte const module = column / kColumnNumberOfModule;
  byte const column_from_right =
      kColumnNumberOfModule - 1 - (column % kColumnNumberOfModule);
  byte const type = static_cast<byte>(module_type_);
  bool const digits_as_rows = (type & internal::kModuleTypeDigitsAsRows) != 0;
  byte digit = digits_as_rows ? row : column_from_right;
  byte segment = digits_as_rows ? column_from_right : row;

  if ((type & internal::kModuleTypeReversedRows) != 0)
    digit = kColumnNumberOfModule - 1 - digit;
  if ((type & internal::kModuleTypeReversedColumns) != 0)
    segment = kRowNumberOfModule - 1 - segment;
  *buffer_column = static_cast<byte>((module * kColumnNumberOfModule) + digit);
  *buffer_bit = segment;
}

/*
 * Returns eight dots as the viewer sees them, from `column`, `row` on to the
 * right, or downwards if `vertical`; the first in bit 0. The line has to lie
 * within the matrix.
 */
byte Panel::GetLine(byte column, byte row, bool vertical) {
  byte buffer_column, buffer_bit;
  byte value = 0;

  for (byte dot = 0; dot < 8; dot++) {
    ConvertDotToBuffer(vertical ? column : column + dot,
                       vertical ? row + dot : row, &buffer_column, &buffer_bit);
    bitWrite(value, dot, bitRead(wired_.GetColumn(buffer_column), buffer_bit));
  }
  return value;
}

/*
 * Sets eight dots as the viewer sees them, from `column`, `row` on to the
 * right, or downwards if `vertical`; the first in bit 0 of `value`. The line
 * has to lie within one module.
 */
void Panel::SetLine(byte column, byte row, bool vertical, byte value) {
  byte buffer_column, buffer_bit;

  for (byte dot = 0; dot < 8; dot++) {
    ConvertDotToBuffer(vertical ? column : column + dot,
                       vertical ? row + dot : row, &buffer_column, &buffer_bit);
    wired_.StoreBit(buffer_column, buffer_bit, bitRead(value, dot));
  }
}

}  // namespace internal
}  // namespace max_matrix
