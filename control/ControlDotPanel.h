// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LEDs of the whole panel, as the matrix shows them, to switch one by a click.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_
#define MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_

#include <stdint.h>
#include <functional>
#include <vector>
#include <wx/panel.h>

/*!
 * \brief The LEDs of the whole panel, as the matrix shows them, to switch one by a click.
 *
 * Every module of the chain side by side, with a gap between two, as the simulator's page draws
 * them. What it shows is the last picture it was given, from SerialControl's answer to `picture`;
 * a click or Clear() changes it at once, so that the click shows before the next picture does.
 */
class ControlDotPanel : public wxPanel
{
  private:
    /* one byte per column, bit 0 the top row, as SerialControl answers picture */
    std::vector<uint8_t> columns_;
    std::function<void(uint8_t, uint8_t, bool)> on_switch_;

    void OnPaint(wxPaintEvent& event);
    void OnClick(wxMouseEvent& event);
    bool GetDotAt(wxPoint const& point, uint8_t* column, uint8_t* row) const;

  public:
    /*!
     * \brief Makes the panel with every LED off.
     *
     * \param[in] parent The window it lies in.
     * \param[in] on_switch Called with the column, the row and whether the dot is lit now, after a
     *                      click has switched it.
     */
    ControlDotPanel(wxWindow* parent, std::function<void(uint8_t, uint8_t, bool)> on_switch);

    /*!
     * \brief Shows `columns`, unless they are what it shows already.
     *
     * \param[in] columns One byte per column as the viewer sees it, from the left, bit 0 the top
     *                    row; anything but `MAXMATRIX_NUMBER_OF_COLUMNS` of them is left out.
     */
    void SetPicture(std::vector<uint8_t> const& columns);

    /*! \brief Turns every LED off. */
    void Clear();
};

#endif
