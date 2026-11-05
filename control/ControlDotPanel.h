// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LEDs of the whole panel, to switch one by a click.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_
#define MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_

#include <stdint.h>
#include <functional>
#include <vector>
#include <wx/panel.h>

/*!
 * \brief The LEDs of the whole panel, to switch one by a click.
 *
 * Every module of the chain side by side, with a gap between two, as the simulator's page draws
 * them. A board cannot be read back, so the dots are the ones clicked since the last Clear(), not
 * what a text has lit.
 */
class ControlDotPanel : public wxPanel
{
  private:
    std::vector<bool> lit_;
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

    /*! \brief Turns every LED off. */
    void Clear();
};

#endif
