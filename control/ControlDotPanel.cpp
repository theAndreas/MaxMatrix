// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LEDs of the whole panel, as the matrix shows them, to switch one
 *        by a click.
 *
 * As many modules as `CONTROL_NUMBER_OF_MODULES`, the chain SerialControl
 * drives; the picture of a sketch built for another chain is not drawn.
 */

#include "ControlDotPanel.h"

#include <math.h>
#include <stdlib.h>
#include <wx/dcbuffer.h>

#include <algorithm>
#include <utility>

#include "ControlCommand.h"
#include "MaxMatrixConfig.h"

/* ControlDotPanel Parameter, in pixels at 96 dpi */
#define CONTROL_DOT_PANEL_CELL 18
#define CONTROL_DOT_PANEL_LED 14
#define CONTROL_DOT_PANEL_MODULE_GAP 8
#define CONTROL_DOT_PANEL_MARGIN 6

/* ControlDotPanel colours: the red of the common modules, and the board behind
 * them */
#define CONTROL_DOT_PANEL_BOARD wxColour(0x20, 0x20, 0x20)
#define CONTROL_DOT_PANEL_LIT wxColour(0xFF, 0x30, 0x30)
#define CONTROL_DOT_PANEL_DARK wxColour(0x50, 0x14, 0x14)
#define CONTROL_DOT_PANEL_DISABLED wxColour(0x40, 0x40, 0x40)

ControlDotPanel::ControlDotPanel(
    wxWindow* parent, std::function<void(uint8_t, uint8_t, bool)> on_switch)
    : wxPanel{parent},
      columns_(static_cast<size_t>(CONTROL_NUMBER_OF_COLUMNS), 0x00),
      on_switch_{std::move(on_switch)} {
  painting_ = false;
  paint_lit_ = false;
  on_dot_ = false;
  last_column_ = 0;
  last_row_ = 0;
  wxCoord const width =
      CONTROL_NUMBER_OF_COLUMNS * CONTROL_DOT_PANEL_CELL +
      (CONTROL_NUMBER_OF_MODULES - 1) * CONTROL_DOT_PANEL_MODULE_GAP +
      2 * CONTROL_DOT_PANEL_MARGIN;
  wxCoord const height =
      max_matrix::kRowNumberOfModule * CONTROL_DOT_PANEL_CELL +
      2 * CONTROL_DOT_PANEL_MARGIN;

  // the background is painted whole by OnPaint(), which the buffered DC needs
  // to know
  SetBackgroundStyle(wxBG_STYLE_PAINT);
  SetMinSize(FromDIP(wxSize(width, height)));
  Bind(wxEVT_PAINT, &ControlDotPanel::OnPaint, this);
  Bind(wxEVT_LEFT_DOWN, &ControlDotPanel::OnLeftDown, this);
  Bind(wxEVT_MOTION, &ControlDotPanel::OnMotion, this);
  Bind(wxEVT_LEFT_UP, &ControlDotPanel::OnLeftUp, this);
  Bind(wxEVT_MOUSE_CAPTURE_LOST, &ControlDotPanel::OnCaptureLost, this);
}

void ControlDotPanel::SetPicture(std::vector<uint8_t> const& columns) {
  if (columns.size() != columns_.size() || columns == columns_) return;
  columns_ = columns;
  Refresh();
}

void ControlDotPanel::Clear() {
  std::fill(columns_.begin(), columns_.end(), 0x00);
  Refresh();
}

// Draws every LED, the lit ones bright; grey while the panel is disabled, as
// there is nothing to switch.
void ControlDotPanel::OnPaint(wxPaintEvent& /* event */) {
  wxAutoBufferedPaintDC dc(this);
  wxCoord const cell = FromDIP(CONTROL_DOT_PANEL_CELL);
  wxCoord const led = FromDIP(CONTROL_DOT_PANEL_LED);
  wxCoord const gap = FromDIP(CONTROL_DOT_PANEL_MODULE_GAP);
  wxCoord const margin = FromDIP(CONTROL_DOT_PANEL_MARGIN);

  dc.SetBackground(wxBrush(CONTROL_DOT_PANEL_BOARD));
  dc.Clear();
  dc.SetPen(*wxTRANSPARENT_PEN);
  for (uint8_t row = 0; row < max_matrix::kRowNumberOfModule; row++) {
    for (uint8_t column = 0; column < CONTROL_NUMBER_OF_COLUMNS; column++) {
      bool const lit = IsLit(column, row);
      wxCoord const x = margin + column * cell +
                        (column / max_matrix::kColumnNumberOfModule) * gap +
                        (cell - led) / 2;
      wxCoord const y = margin + row * cell + (cell - led) / 2;
      if (!IsEnabled())
        dc.SetBrush(wxBrush(CONTROL_DOT_PANEL_DISABLED));
      else
        dc.SetBrush(
            wxBrush(lit ? CONTROL_DOT_PANEL_LIT : CONTROL_DOT_PANEL_DARK));
      dc.DrawEllipse(x, y, led, led);
    }
  }
}

/*
 * Switches the LED under the click and starts a stroke that gives every LED it
 * passes the same state; a click into a gap or the margin does nothing. The
 * mouse is captured, so that the stroke ends where the button is let go, even
 * outside the panel.
 */
void ControlDotPanel::OnLeftDown(wxMouseEvent& event) {
  uint8_t column;
  uint8_t row;

  if (!GetDotAt(event.GetPosition(), &column, &row)) return;
  painting_ = true;
  paint_lit_ = !IsLit(column, row);
  on_dot_ = true;
  last_column_ = column;
  last_row_ = row;
  if (!HasCapture()) CaptureMouse();
  PaintDot(column, row);
}

/*
 * Paints every LED on the way from the last one to the one under the mouse:
 * motion events come every few pixels, and a quick stroke would leave gaps
 * otherwise. Off the LEDs, in a gap or outside the panel, the stroke pauses and
 * starts again at the next LED without a line to it.
 */
void ControlDotPanel::OnMotion(wxMouseEvent& event) {
  uint8_t column;
  uint8_t row;

  if (!painting_ || !event.LeftIsDown()) return;
  if (!GetDotAt(event.GetPosition(), &column, &row)) {
    on_dot_ = false;
    return;
  }
  if (on_dot_) {
    int16_t const delta_column = static_cast<int16_t>(column - last_column_);
    int16_t const delta_row = static_cast<int16_t>(row - last_row_);
    int16_t const steps =
        static_cast<int16_t>(std::max(abs(delta_column), abs(delta_row)));
    for (int16_t step = 1; step < steps; step++) {
      PaintDot(static_cast<uint8_t>(
                   last_column_ +
                   lround(static_cast<double>(delta_column) * step / steps)),
               static_cast<uint8_t>(
                   last_row_ +
                   lround(static_cast<double>(delta_row) * step / steps)));
    }
  }
  PaintDot(column, row);
  on_dot_ = true;
  last_column_ = column;
  last_row_ = row;
}

void ControlDotPanel::OnLeftUp(wxMouseEvent& /* event */) {
  painting_ = false;
  if (HasCapture()) ReleaseMouse();
}

// Another window took the mouse, a dialog say; the stroke ends there.
void ControlDotPanel::OnCaptureLost(wxMouseCaptureLostEvent& /* event */) {
  painting_ = false;
}

// Gives the LED the stroke's state, and says so, unless it has it already.
void ControlDotPanel::PaintDot(uint8_t column, uint8_t row) {
  if (IsLit(column, row) == paint_lit_) return;
  columns_[column] ^= static_cast<uint8_t>(1u << row);
  Refresh();
  on_switch_(column, row, paint_lit_);
}

bool ControlDotPanel::IsLit(uint8_t column, uint8_t row) const {
  return ((columns_[column] >> row) & 0x01) != 0;
}

/*
 * Finds the LED at `point`, in pixels of the panel, and gives its `column` and
 * `row`. Returns false for a point in the margin or in the gap between two
 * modules, which belongs to no LED.
 */
bool ControlDotPanel::GetDotAt(wxPoint const& point, uint8_t* column,
                               uint8_t* row) const {
  wxCoord const cell = FromDIP(CONTROL_DOT_PANEL_CELL);
  wxCoord const module_width = max_matrix::kColumnNumberOfModule * cell +
                               FromDIP(CONTROL_DOT_PANEL_MODULE_GAP);
  wxCoord const x = point.x - FromDIP(CONTROL_DOT_PANEL_MARGIN);
  wxCoord const y = point.y - FromDIP(CONTROL_DOT_PANEL_MARGIN);
  wxCoord const in_module = x % module_width;

  if (x < 0 || y < 0 || in_module >= max_matrix::kColumnNumberOfModule * cell)
    return false;
  wxCoord const found_column =
      x / module_width * max_matrix::kColumnNumberOfModule + in_module / cell;
  wxCoord const found_row = y / cell;
  if (found_column >= CONTROL_NUMBER_OF_COLUMNS ||
      found_row >= max_matrix::kRowNumberOfModule)
    return false;
  *column = static_cast<uint8_t>(found_column);
  *row = static_cast<uint8_t>(found_row);
  return true;
}
