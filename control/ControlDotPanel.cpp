// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LEDs of the whole panel, as the matrix shows them, to switch one by a click.
 *
 * As many modules as the library is built for by default, `MAXMATRIX_NUMBER_OF_MODULES`, which is
 * what SerialControl drives unless its sketch was built for another chain.
 */

#include <wx/dcbuffer.h>
#include "ControlDotPanel.h"
#include "MaxMatrixConfig.h"

/* ControlDotPanel Parameter, in pixels at 96 dpi */
#define CONTROL_DOT_PANEL_CELL                                      18
#define CONTROL_DOT_PANEL_LED                                       14
#define CONTROL_DOT_PANEL_MODULE_GAP                                8
#define CONTROL_DOT_PANEL_MARGIN                                    6

/* ControlDotPanel colours: the red of the common modules, and the board behind them */
#define CONTROL_DOT_PANEL_BOARD                                     wxColour(0x20, 0x20, 0x20)
#define CONTROL_DOT_PANEL_LIT                                       wxColour(0xFF, 0x30, 0x30)
#define CONTROL_DOT_PANEL_DARK                                      wxColour(0x50, 0x14, 0x14)
#define CONTROL_DOT_PANEL_DISABLED                                  wxColour(0x40, 0x40, 0x40)

ControlDotPanel::ControlDotPanel(wxWindow* parent, std::function<void(uint8_t, uint8_t, bool)> on_switch)
    : wxPanel(parent), columns_(static_cast<size_t>(MAXMATRIX_NUMBER_OF_COLUMNS), 0x00), on_switch_(std::move(on_switch))
{
    wxCoord const width = MAXMATRIX_NUMBER_OF_COLUMNS * CONTROL_DOT_PANEL_CELL + (MAXMATRIX_NUMBER_OF_MODULES - 1) * CONTROL_DOT_PANEL_MODULE_GAP +
                      2 * CONTROL_DOT_PANEL_MARGIN;
    wxCoord const height = MAXMATRIX_ROW_NUMBER_OF_MODULE * CONTROL_DOT_PANEL_CELL + 2 * CONTROL_DOT_PANEL_MARGIN;

    // the background is painted whole by OnPaint(), which the buffered DC needs to know
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(FromDIP(wxSize(width, height)));
    Bind(wxEVT_PAINT, &ControlDotPanel::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &ControlDotPanel::OnClick, this);
}

void ControlDotPanel::SetPicture(std::vector<uint8_t> const& columns)
{
    if(columns.size() != columns_.size() || columns == columns_) return;
    columns_ = columns;
    Refresh();
}

void ControlDotPanel::Clear()
{
    std::fill(columns_.begin(), columns_.end(), 0x00);
    Refresh();
}

// Draws every LED, the lit ones bright; grey while the panel is disabled, as there is nothing to
// switch.
void ControlDotPanel::OnPaint(wxPaintEvent& /* event */)
{
    wxAutoBufferedPaintDC dc(this);
    wxCoord const cell = FromDIP(CONTROL_DOT_PANEL_CELL);
    wxCoord const led = FromDIP(CONTROL_DOT_PANEL_LED);
    wxCoord const gap = FromDIP(CONTROL_DOT_PANEL_MODULE_GAP);
    wxCoord const margin = FromDIP(CONTROL_DOT_PANEL_MARGIN);

    dc.SetBackground(wxBrush(CONTROL_DOT_PANEL_BOARD));
    dc.Clear();
    dc.SetPen(*wxTRANSPARENT_PEN);
    for(uint8_t row = 0; row < MAXMATRIX_ROW_NUMBER_OF_MODULE; row++) {
        for(uint8_t column = 0; column < MAXMATRIX_NUMBER_OF_COLUMNS; column++) {
            bool const lit = ((columns_[column] >> row) & 0x01) != 0;
            wxCoord const x = margin + column * cell + (column / MAXMATRIX_COLUMN_NUMBER_OF_MODULE) * gap + (cell - led) / 2;
            wxCoord const y = margin + row * cell + (cell - led) / 2;
            if(!IsEnabled()) dc.SetBrush(wxBrush(CONTROL_DOT_PANEL_DISABLED));
            else dc.SetBrush(wxBrush(lit ? CONTROL_DOT_PANEL_LIT : CONTROL_DOT_PANEL_DARK));
            dc.DrawEllipse(x, y, led, led);
        }
    }
}

// Switches the LED under the click and says so; a click into a gap or the margin does nothing.
void ControlDotPanel::OnClick(wxMouseEvent& event)
{
    uint8_t column;
    uint8_t row;

    if(!GetDotAt(event.GetPosition(), &column, &row)) return;
    columns_[column] ^= static_cast<uint8_t>(1u << row);
    Refresh();
    on_switch_(column, row, ((columns_[column] >> row) & 0x01) != 0);
}

/*
 * Finds the LED at `point`, in pixels of the panel, and gives its `column` and `row`. Returns
 * false for a point in the margin or in the gap between two modules, which belongs to no LED.
 */
bool ControlDotPanel::GetDotAt(wxPoint const& point, uint8_t* column, uint8_t* row) const
{
    wxCoord const cell = FromDIP(CONTROL_DOT_PANEL_CELL);
    wxCoord const module_width = MAXMATRIX_COLUMN_NUMBER_OF_MODULE * cell + FromDIP(CONTROL_DOT_PANEL_MODULE_GAP);
    wxCoord const x = point.x - FromDIP(CONTROL_DOT_PANEL_MARGIN);
    wxCoord const y = point.y - FromDIP(CONTROL_DOT_PANEL_MARGIN);
    wxCoord const in_module = x % module_width;

    if(x < 0 || y < 0 || in_module >= MAXMATRIX_COLUMN_NUMBER_OF_MODULE * cell) return false;
    wxCoord const found_column = x / module_width * MAXMATRIX_COLUMN_NUMBER_OF_MODULE + in_module / cell;
    wxCoord const found_row = y / cell;
    if(found_column >= MAXMATRIX_NUMBER_OF_COLUMNS || found_row >= MAXMATRIX_ROW_NUMBER_OF_MODULE) return false;
    *column = static_cast<uint8_t>(found_column);
    *row = static_cast<uint8_t>(found_row);
    return true;
}
