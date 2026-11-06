// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The window of the control program.
 *
 * Laid out as the Windows tool of AndreasBur/Led-Matrix was, but for its two tabs: the matrix and
 * the text below it on the left, the connections and the log on the right.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLWINDOW_H_
#define MAXMATRIX_CONTROL_CONTROLWINDOW_H_

#include <string>
#include <wx/frame.h>
#include <wx/timer.h>
#include "ControlLink.h"
#include "ControlSerialPort.h"
#include "ControlTcpPort.h"

class wxButton;
class wxCheckBox;
class wxComboBox;
class wxSlider;
class wxSpinCtrl;
class wxSpinEvent;
class wxStaticText;
class wxTextCtrl;
class ControlDotPanel;

/*!
 * \brief The window of the control program.
 *
 * Every command goes to the board and to the simulator, whichever is connected, as on the
 * simulator's page; with SerialControl in both, the simulator shows what the board should. The
 * controls are enabled while at least one of them takes commands.
 *
 * The matrix in the window is what the board shows, or the simulator while no board is ready, as
 * SerialControl answers `picture`; it is asked for up to twenty times a second. When SerialControl
 * starts, or the simulator is connected, it is sent the intensity, speed and space the window
 * shows, so that the two agree.
 *
 * It holds no logic a test would need: the lines are ControlCommand's, the connections ControlLink's,
 * so the window itself is checked by hand.
 */
class ControlWindow : public wxFrame
{
  private:
    ControlSerialPort serial_port_;
    ControlTcpPort tcp_port_;
    ControlLink board_link_;
    ControlLink simulator_link_;
    wxTimer timer_;
    std::string last_setting_;

    wxWindow* controls_;
    wxTextCtrl* text_;
    wxCheckBox* shift_;
    wxSpinCtrl* space_;
    wxSlider* intensity_;
    wxSlider* speed_;
    ControlDotPanel* dots_;
    wxComboBox* serial_port_name_;
    wxButton* board_button_;
    wxStaticText* board_status_;
    wxSpinCtrl* tcp_port_number_;
    wxButton* simulator_button_;
    wxStaticText* simulator_status_;
    wxTextCtrl* log_;

    void MakeMenu();
    wxWindow* MakeMatrix(wxWindow* parent);
    wxWindow* MakeText(wxWindow* parent);
    wxWindow* MakeConnections(wxWindow* parent);
    void OnSend(wxCommandEvent& event);
    void OnClear(wxCommandEvent& event);
    void OnShift(wxCommandEvent& event);
    void OnIntensity(wxCommandEvent& event);
    void OnSpeed(wxCommandEvent& event);
    void OnSpace(wxSpinEvent& event);
    void OnBoard(wxCommandEvent& event);
    void OnSimulator(wxCommandEvent& event);
    void OnListPorts(wxCommandEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnAbout(wxCommandEvent& event);
    void OnExit(wxCommandEvent& event);
    void Broadcast(std::string const& line);
    void BroadcastSetting(std::string const& line);
    void SendSettings(ControlLink& link, char const* name);
    void Follow(ControlLink& link, char const* name);
    ControlLink* GetShownLink();
    void ShowState();
    void Log(wxString const& text);
    static wxString ConvertToText(std::string const& line);

  public:
    /*! \brief Makes the window, with nothing connected. */
    ControlWindow();
};

#endif
