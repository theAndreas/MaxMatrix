// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LEDs of the whole panel, as the matrix shows them, to switch one
 *        by a click.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_
#define MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_

#include <stdint.h>
#include <wx/panel.h>

#include <functional>
#include <vector>

/*!
 * \brief The LEDs of the whole panel, as the matrix shows them, to switch one
 *        by a click.
 *
 * Moved with the button held, the mouse paints: every LED it passes takes the
 * state the first one took, as in a paint program, so that a line is one stroke
 * rather than a click per LED.
 *
 * Every module of the chain side by side, with a gap between two, as the
 * simulator's page draws them. What it shows is the last picture it was given,
 * from SerialControl's answer to `picture`; a click or Clear() changes it at
 * once, so that the click shows before the next picture does.
 */
class ControlDotPanel : public wxPanel {
 private:
  /* one byte per column, bit 0 the top row, as SerialControl answers picture */
  std::vector<uint8_t> columns_;
  std::function<void(uint8_t, uint8_t, bool)> on_switch_;
  /* while the button is held: the state every LED passed takes, and the LED the
   * mouse was on last, if it was on one */
  bool painting_;
  bool paint_lit_;
  bool on_dot_;
  uint8_t last_column_;
  uint8_t last_row_;

  void OnPaint(wxPaintEvent& event);
  void OnLeftDown(wxMouseEvent& event);
  void OnMotion(wxMouseEvent& event);
  void OnLeftUp(wxMouseEvent& event);
  void OnCaptureLost(wxMouseCaptureLostEvent& event);
  void PaintDot(uint8_t column, uint8_t row);
  bool IsLit(uint8_t column, uint8_t row) const;
  bool GetDotAt(wxPoint const& point, uint8_t* column, uint8_t* row) const;

 public:
  /*!
   * \brief Makes the panel with every LED off.
   *
   * \param[in] parent The window it lies in.
   * \param[in] on_switch Called with the column, the row and whether the dot is
   *                      lit now, after a click or a stroke has switched it;
   *                      once for every LED switched.
   */
  ControlDotPanel(wxWindow* parent,
                  std::function<void(uint8_t, uint8_t, bool)> on_switch);

  /*!
   * \brief Shows `columns`, unless they are what it shows already.
   *
   * \param[in] columns One byte per column as the viewer sees it, from the
   *                    left, bit 0 the top row; anything but
   *                    `CONTROL_NUMBER_OF_COLUMNS` of them is left out.
   */
  void SetPicture(std::vector<uint8_t> const& columns);

  /*! \brief Turns every LED off. */
  void Clear();
};

#endif  // MAXMATRIX_CONTROL_CONTROLDOTPANEL_H_
