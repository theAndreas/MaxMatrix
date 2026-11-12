// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The window of the control program.
 *
 * The labels are English, as the rest of the repository is; the old tool's were
 * German.
 */

#include "ControlWindow.h"

#include <inttypes.h>
#include <wx/aboutdlg.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/combobox.h>
#include <wx/menu.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <chrono>
#include <string>
#include <vector>

#include "ControlCommand.h"
#include "ControlDotPanel.h"
#include "MaxMatrixConfig.h"

namespace {

/* ControlWindow Parameter */
/* what SerialControl opens its Serial with */
constexpr uint32_t kBaudRate{115200u};
/* the port the README starts the simulator with, beside the page's 8080 */
constexpr int kTcpPort{8081};
constexpr int kPollMs{20};
constexpr int kMaxIntensity{15};
constexpr int kMinSpeed{10};
constexpr int kMaxSpeed{500};
constexpr int kMaxSpace{255};
constexpr int kConnectionsWidth{220};
/* a dozen lines of the log, which makes the window as tall as the old tool's */
constexpr int kLogHeight{200};
constexpr int kBorder{8};

}  // namespace

/* ControlWindow colours of a connection's state */
#define CONTROL_WINDOW_READY wxColour(0x00, 0x80, 0x00)
#define CONTROL_WINDOW_WAITING wxColour(0xC0, 0x60, 0x00)
#define CONTROL_WINDOW_TOO_LONG wxColour(0xC0, 0x00, 0x00)

ControlWindow::ControlWindow()
    : wxFrame{nullptr, wxID_ANY, "MaxMatrix Control"},
      board_link_{serial_port_},
      simulator_link_{tcp_port_},
      timer_{this} {
  wxPanel* const content = new wxPanel(this);
  wxBoxSizer* const columns = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* const controls = new wxBoxSizer(wxVERTICAL);

  MakeMenu();
  controls_ = new wxPanel(content);
  controls->Add(MakeMatrix(controls_), 0, wxEXPAND);
  controls->Add(MakeText(controls_), 1, wxEXPAND);
  controls_->SetSizer(controls);
  columns->Add(controls_, 1, wxEXPAND | wxALL, FromDIP(kBorder));
  columns->Add(
      MakeConnections(content), 0,
      // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
      wxEXPAND | wxTOP | wxBOTTOM | wxRIGHT, FromDIP(kBorder));
  content->SetSizer(columns);
  SetClientSize(content->GetBestSize());
  SetMinClientSize(content->GetBestSize());

  Bind(wxEVT_TIMER, &ControlWindow::OnTimer, this);
  timer_.Start(kPollMs);
  ShowState();
}

// File and Help, as the old tool had them.
void ControlWindow::MakeMenu() {
  wxMenuBar* const bar = new wxMenuBar();
  wxMenu* const file = new wxMenu();
  wxMenu* const help = new wxMenu();

  file->Append(wxID_EXIT);
  help->Append(wxID_ABOUT);
  bar->Append(file, "&File");
  bar->Append(help, "&Help");
  SetMenuBar(bar);
  Bind(wxEVT_MENU, &ControlWindow::OnExit, this, wxID_EXIT);
  Bind(wxEVT_MENU, &ControlWindow::OnAbout, this, wxID_ABOUT);
}

/*
 * The text and how it is shown: the field with the bytes it may still take, the
 * brightness and the speed as sliders, and beside them whether it shifts, the
 * space between the chars, Clear and Send. Enter in the field sends as well.
 */
wxWindow* ControlWindow::MakeText(wxWindow* parent) {
  wxPanel* const page = new wxPanel(parent);
  wxBoxSizer* const rows = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer* const settings = new wxBoxSizer(wxHORIZONTAL);
  wxStaticBoxSizer* const intensity_box =
      new wxStaticBoxSizer(wxVERTICAL, page, "Intensity");
  wxStaticBoxSizer* const speed_box =
      new wxStaticBoxSizer(wxVERTICAL, page, "Speed, ms per column");
  wxBoxSizer* const actions = new wxBoxSizer(wxVERTICAL);
  wxBoxSizer* const space_row = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* const text_row = new wxBoxSizer(wxHORIZONTAL);
  wxButton* const clear = new wxButton(page, wxID_ANY, "Clear");
  wxCoord const border = FromDIP(kBorder);

  send_ = new wxButton(page, wxID_ANY, "Send");
  text_ = new wxTextCtrl(page, wxID_ANY, "Hello", wxDefaultPosition,
                         wxDefaultSize, wxTE_PROCESS_ENTER);
  bytes_left_ = new wxStaticText(
      page, wxID_ANY, wxEmptyString, wxDefaultPosition,
      // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
      wxDefaultSize, wxALIGN_RIGHT | wxST_NO_AUTORESIZE);
  // as wide as the longest it says, so that the field does not change its width
  // as one types
  bytes_left_->SetMinSize(bytes_left_->GetTextExtent("000 bytes too long"));
  intensity_ = new wxSlider(intensity_box->GetStaticBox(), wxID_ANY,
                            MAXMATRIX_INTENSITY_INIT_VALUE, 0, kMaxIntensity,
                            wxDefaultPosition, FromDIP(wxSize(-1, 140)),
                            wxSL_VERTICAL | wxSL_INVERSE | wxSL_VALUE_LABEL);
  speed_ = new wxSlider(speed_box->GetStaticBox(), wxID_ANY,
                        MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE, kMinSpeed,
                        kMaxSpeed, wxDefaultPosition, FromDIP(wxSize(-1, 140)),
                        wxSL_VERTICAL | wxSL_INVERSE | wxSL_VALUE_LABEL);
  shift_ = new wxCheckBox(page, wxID_ANY, "Shift in");
  space_ = new wxSpinCtrl(page, wxID_ANY, wxEmptyString, wxDefaultPosition,
                          wxDefaultSize, wxSP_ARROW_KEYS, 0, kMaxSpace,
                          MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE);

  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  intensity_box->Add(intensity_, 1, wxALIGN_CENTER_HORIZONTAL | wxALL, border);
  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  speed_box->Add(speed_, 1, wxALIGN_CENTER_HORIZONTAL | wxALL, border);
  space_row->Add(
      new wxStaticText(page, wxID_ANY, "Space"), 0,
      // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
      wxALIGN_CENTER_VERTICAL | wxRIGHT, border);
  space_row->Add(space_, 1);
  actions->Add(shift_, 0, wxBOTTOM, border);
  actions->Add(space_row, 0, wxEXPAND | wxBOTTOM, border);
  actions->AddStretchSpacer();
  actions->Add(clear, 0, wxEXPAND | wxBOTTOM, border);
  actions->Add(send_, 0, wxEXPAND);
  settings->Add(intensity_box, 0, wxEXPAND | wxRIGHT, border);
  settings->Add(speed_box, 0, wxEXPAND | wxRIGHT, border);
  settings->Add(actions, 1, wxEXPAND);
  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  text_row->Add(text_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, border);
  text_row->Add(bytes_left_, 0, wxALIGN_CENTER_VERTICAL);
  rows->Add(text_row, 0, wxEXPAND | wxALL, border);
  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  rows->Add(settings, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, border);
  page->SetSizer(rows);

  text_->Bind(wxEVT_TEXT_ENTER, &ControlWindow::OnSend, this);
  text_->Bind(wxEVT_TEXT, [this](wxCommandEvent& /* event */) { ShowState(); });
  send_->Bind(wxEVT_BUTTON, &ControlWindow::OnSend, this);
  clear->Bind(wxEVT_BUTTON, &ControlWindow::OnClear, this);
  shift_->Bind(wxEVT_CHECKBOX, &ControlWindow::OnShift, this);
  intensity_->Bind(wxEVT_SLIDER, &ControlWindow::OnIntensity, this);
  speed_->Bind(wxEVT_SLIDER, &ControlWindow::OnSpeed, this);
  space_->Bind(wxEVT_SPINCTRL, &ControlWindow::OnSpace, this);
  return page;
}

// The whole panel as the matrix shows it, to click on; Clear below it turns
// every LED off.
wxWindow* ControlWindow::MakeMatrix(wxWindow* parent) {
  wxPanel* const panel = new wxPanel(parent);
  wxStaticBoxSizer* const box =
      new wxStaticBoxSizer(wxVERTICAL, panel, "Matrix");

  dots_ = new ControlDotPanel(
      box->GetStaticBox(), [this](uint8_t column, uint8_t row, bool lit) {
        Broadcast(ControlCommand::MakeDot(column, row, lit));
      });
  box->Add(dots_, 0, wxALL, FromDIP(kBorder));
  panel->SetSizer(box);
  return panel;
}

/*
 * The column on the right: the board's serial port, the simulator's TCP port,
 * each with a button and its state, and the log below them.
 */
wxWindow* ControlWindow::MakeConnections(wxWindow* parent) {
  wxPanel* const column = new wxPanel(parent);
  wxBoxSizer* const rows = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer* const board_box =
      new wxStaticBoxSizer(wxVERTICAL, column, "Board");
  wxStaticBoxSizer* const simulator_box =
      new wxStaticBoxSizer(wxVERTICAL, column, "Simulator");
  wxStaticBoxSizer* const log_box =
      new wxStaticBoxSizer(wxVERTICAL, column, "Log");
  wxBoxSizer* const board_row = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* const simulator_row = new wxBoxSizer(wxHORIZONTAL);
  wxBoxSizer* const port_row = new wxBoxSizer(wxHORIZONTAL);
  wxButton* const clear_log =
      new wxButton(log_box->GetStaticBox(), wxID_ANY, "Clear log");
  wxCoord const border = FromDIP(kBorder);

  serial_port_name_ = new wxComboBox(board_box->GetStaticBox(), wxID_ANY);
  board_button_ = new wxButton(board_box->GetStaticBox(), wxID_ANY, "Open");
  board_status_ =
      new wxStaticText(board_box->GetStaticBox(), wxID_ANY, wxEmptyString);
  tcp_port_number_ = new wxSpinCtrl(
      simulator_box->GetStaticBox(), wxID_ANY, wxEmptyString, wxDefaultPosition,
      wxDefaultSize, wxSP_ARROW_KEYS, 1, UINT16_MAX, kTcpPort);
  simulator_button_ =
      new wxButton(simulator_box->GetStaticBox(), wxID_ANY, "Connect");
  simulator_status_ =
      new wxStaticText(simulator_box->GetStaticBox(), wxID_ANY, wxEmptyString);
  log_ = new wxTextCtrl(log_box->GetStaticBox(), wxID_ANY, wxEmptyString,
                        wxDefaultPosition, wxDefaultSize,
                        wxTE_MULTILINE | wxTE_READONLY);

  board_row->Add(board_button_, 0, wxRIGHT, border);
  board_row->Add(board_status_, 1, wxALIGN_CENTER_VERTICAL);
  board_box->Add(serial_port_name_, 0, wxEXPAND | wxALL, border);
  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  board_box->Add(board_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, border);
  port_row->Add(
      new wxStaticText(simulator_box->GetStaticBox(), wxID_ANY, "Port"), 0,
      // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
      wxALIGN_CENTER_VERTICAL | wxRIGHT, border);
  port_row->Add(tcp_port_number_, 1);
  simulator_row->Add(simulator_button_, 0, wxRIGHT, border);
  simulator_row->Add(simulator_status_, 1, wxALIGN_CENTER_VERTICAL);
  simulator_box->Add(port_row, 0, wxEXPAND | wxALL, border);
  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  simulator_box->Add(simulator_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                     border);
  log_->SetMinSize(FromDIP(wxSize(-1, kLogHeight)));
  log_box->Add(log_, 1, wxEXPAND | wxALL, border);
  // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, wx's flags are int
  log_box->Add(clear_log, 0, wxLEFT | wxRIGHT | wxBOTTOM, border);
  rows->Add(board_box, 0, wxEXPAND | wxBOTTOM, border);
  rows->Add(simulator_box, 0, wxEXPAND | wxBOTTOM, border);
  rows->Add(log_box, 1, wxEXPAND);
  rows->SetMinSize(FromDIP(wxSize(kConnectionsWidth, -1)));
  column->SetSizer(rows);

  board_button_->Bind(wxEVT_BUTTON, &ControlWindow::OnBoard, this);
  simulator_button_->Bind(wxEVT_BUTTON, &ControlWindow::OnSimulator, this);
  serial_port_name_->Bind(wxEVT_COMBOBOX_DROPDOWN, &ControlWindow::OnListPorts,
                          this);
  clear_log->Bind(wxEVT_BUTTON,
                  [this](wxCommandEvent& /* event */) { log_->Clear(); });
  wxCommandEvent list;
  OnListPorts(list);
  return column;
}

/*
 * Sends the text, standing or shifting in; one too long for SerialControl's
 * line buffer is not sent, as the board would drop it with no more than "error:
 * line too long".
 */
void ControlWindow::OnSend(wxCommandEvent& /* event */) {
  std::string const line = ControlCommand::MakeText(
      std::string(text_->GetValue().ToUTF8()), shift_->GetValue());

  if (ControlCommand::Fits(line)) {
    Broadcast(line);
  } else {
    Log(wxString::Format(
        "Not sent: the line takes %zu bytes, SerialControl takes %u.\n",
        line.size(), MAXMATRIX_COMMAND_LINE_LENGTH));
  }
}

// Turns every LED off, on the board and in the window at once.
void ControlWindow::OnClear(wxCommandEvent& /* event */) {
  Broadcast(ControlCommand::MakeClear());
  dots_->Clear();
}

// The speed matters only to a text that shifts, as in the old tool; textShift=
// also takes five bytes more of the line than text=.
void ControlWindow::OnShift(wxCommandEvent& /* event */) { ShowState(); }

void ControlWindow::OnIntensity(wxCommandEvent& /* event */) {
  BroadcastSetting(ControlCommand::MakeIntensity(
      static_cast<uint8_t>(intensity_->GetValue())));
}

void ControlWindow::OnSpeed(wxCommandEvent& /* event */) {
  BroadcastSetting(
      ControlCommand::MakeSpeed(static_cast<uint16_t>(speed_->GetValue())));
}

void ControlWindow::OnSpace(wxSpinEvent& /* event */) {
  Broadcast(
      ControlCommand::MakeSpace(static_cast<uint8_t>(space_->GetValue())));
}

// Opens the port in the box, or closes the one that is open.
void ControlWindow::OnBoard(wxCommandEvent& /* event */) {
  wxString const name = serial_port_name_->GetValue();

  if (serial_port_.IsOpen()) {
    serial_port_.Close();
    Log("board: closed\n");
  } else if (serial_port_.Open(std::string(name.ToUTF8()), kBaudRate)) {
    board_link_.Begin(true);
    Log("board: " + name + " open, waiting for SerialControl to say ready\n");
  } else {
    Log("board: cannot open " + name + ", which may not exist or be in use\n");
  }
  ShowState();
}

// Connects to the simulator on the port in the box, or ends the connection.
void ControlWindow::OnSimulator(wxCommandEvent& /* event */) {
  uint16_t const port = static_cast<uint16_t>(tcp_port_number_->GetValue());

  if (tcp_port_.IsOpen()) {
    tcp_port_.Close();
    Log("simulator: closed\n");
  } else if (tcp_port_.Open(port)) {
    simulator_link_.Begin(false);
    Log(wxString::Format("simulator: connected to localhost:%u\n", port));
    SendSettings(simulator_link_, "simulator");
  } else {
    Log(wxString::Format(
        "simulator: none on localhost:%u; start it with --tcp %u\n", port,
        port));
  }
  ShowState();
}

// Fills the box with the ports a board may be on, and keeps what was typed into
// it.
void ControlWindow::OnListPorts(wxCommandEvent& /* event */) {
  wxString const typed = serial_port_name_->GetValue();

  serial_port_name_->Clear();
  for (std::string const& port : ControlSerialPort::List()) {
    serial_port_name_->Append(wxString::FromUTF8(port));
  }
  if (!typed.empty()) {
    serial_port_name_->SetValue(typed);
  } else if (serial_port_name_->GetCount() > 0) {
    serial_port_name_->SetSelection(0);
  }
}

/*
 * Reads what the board and the simulator have said, and asks the one the window
 * shows for its picture; only that one, as the other would answer for nothing.
 */
void ControlWindow::OnTimer(wxTimerEvent& /* event */) {
  uint32_t const now = static_cast<uint32_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());

  Follow(board_link_, "board");
  Follow(simulator_link_, "simulator");
  ControlLink* const shown = GetShownLink();
  if (shown == nullptr) return;
  shown->AskForPicture(now);
  dots_->SetPicture(shown->GetPicture());
}

void ControlWindow::OnAbout(wxCommandEvent& /* event */) {
  wxAboutDialogInfo about;

  about.SetName("MaxMatrix Control");
  about.SetDescription(
      "Drives the SerialControl example of MaxMatrix: on a board over its "
      "serial port, in the simulator over TCP.");
  about.SetCopyright("(C) 2026 Andreas Burnickl");
  about.SetLicence("GNU General Public License, version 3 or later");
  about.SetWebSite("https://github.com/theAndreas/MaxMatrix");
  wxAboutBox(about, this);
}

void ControlWindow::OnExit(wxCommandEvent& /* event */) { Close(true); }

// Sends `line` to every end that takes commands, and logs it once.
void ControlWindow::Broadcast(std::string const& line) {
  Log("> " + ConvertToText(line) + "\n");
  board_link_.Send(line);
  simulator_link_.Send(line);
  ShowState();
}

// Sends a slider's `line` unless it is the one sent last: GTK reports a click
// on a slider twice.
void ControlWindow::BroadcastSetting(std::string const& line) {
  if (line == last_setting_) return;
  last_setting_ = line;
  Broadcast(line);
}

/*
 * Sends `link` the intensity, speed and space the window shows: its
 * SerialControl has just started with the ones it was built with, or the
 * simulator's has been set by another program before.
 */
void ControlWindow::SendSettings(ControlLink& link, char const* name) {
  std::string const lines[] = {
      ControlCommand::MakeIntensity(
          static_cast<uint8_t>(intensity_->GetValue())),
      ControlCommand::MakeSpeed(static_cast<uint16_t>(speed_->GetValue())),
      ControlCommand::MakeSpace(static_cast<uint8_t>(space_->GetValue()))};

  for (std::string const& line : lines) {
    Log(wxString(name) + ": > " + ConvertToText(line) + "\n");
    link.Send(line);
  }
}

/*
 * Logs what `link` has received, each line after the `name` of its end, and
 * says so when its port has closed under it: the board unplugged, the simulator
 * stopped or taken by another program. A SerialControl that says ready has
 * started, at the first connection or after a reset, and takes the window's
 * settings.
 */
void ControlWindow::Follow(ControlLink& link, char const* name) {
  std::vector<std::string> lines;

  if (!link.IsOpen()) return;
  bool const was_ready = link.IsReady();
  bool const open = link.Task(&lines);

  for (std::string const& line : lines) {
    Log(wxString(name) + ": " + ConvertToText(line) + "\n");
    if (line == kControlLinkReady) SendSettings(link, name);
  }
  if (!open) Log(wxString(name) + ": the connection has closed\n");
  if (!open || was_ready != link.IsReady()) ShowState();
}

// The board while it is ready, as that is what the window is for; the simulator
// otherwise.
ControlLink* ControlWindow::GetShownLink() {
  if (board_link_.IsReady()) return &board_link_;
  if (simulator_link_.IsReady()) return &simulator_link_;
  return nullptr;
}

/*
 * Shows on every button, state and control what the connections allow: a
 * control is enabled while the board or the simulator takes commands, the speed
 * only for a text that shifts.
 */
void ControlWindow::ShowState() {
  bool const any_ready = board_link_.IsReady() || simulator_link_.IsReady();

  board_button_->SetLabel(serial_port_.IsOpen() ? "Close" : "Open");
  serial_port_name_->Enable(!serial_port_.IsOpen());
  board_status_->SetLabel(board_link_.IsReady()   ? "ready"
                          : serial_port_.IsOpen() ? "waiting for ready"
                                                  : "closed");
  board_status_->SetForegroundColour(
      board_link_.IsReady() ? CONTROL_WINDOW_READY
      : serial_port_.IsOpen()
          ? CONTROL_WINDOW_WAITING
          : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  simulator_button_->SetLabel(tcp_port_.IsOpen() ? "Disconnect" : "Connect");
  tcp_port_number_->Enable(!tcp_port_.IsOpen());
  simulator_status_->SetLabel(simulator_link_.IsReady() ? "ready"
                                                        : "not connected");
  simulator_status_->SetForegroundColour(
      simulator_link_.IsReady()
          ? CONTROL_WINDOW_READY
          : wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
  controls_->Enable(any_ready);
  ShowBytesLeft();
  speed_->Enable(any_ready && shift_->GetValue());
  dots_->Refresh();
}

/*
 * Shows beside the field how many bytes the text may still take before
 * SerialControl drops the line, red once it is too long, and enables Send only
 * for a line that fits: the bytes are those of the whole line in UTF-8, so an
 * umlaut takes two, and Shift in five more.
 */
void ControlWindow::ShowBytesLeft() {
  std::string const line = ControlCommand::MakeText(
      std::string(text_->GetValue().ToUTF8()), shift_->GetValue());
  int32_t const left = ControlCommand::GetBytesLeft(line);

  if (left >= 0) {
    bytes_left_->SetLabel(wxString::Format("%" PRId32 " bytes left", left));
  } else {
    bytes_left_->SetLabel(
        wxString::Format("%" PRId32 " bytes too long", -left));
  }
  bytes_left_->SetForegroundColour(
      left >= 0 ? wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT)
                : CONTROL_WINDOW_TOO_LONG);
  send_->Enable(ControlCommand::Fits(line));
}

void ControlWindow::Log(wxString const& text) { log_->AppendText(text); }

/*
 * Converts a line to show it: UTF-8, as the program sends a text, or else
 * latin-1, which is what a sketch that prints the char of the library's font
 * sends.
 */
wxString ControlWindow::ConvertToText(std::string const& line) {
  wxString const text = wxString::FromUTF8(line);

  return text.empty() && !line.empty() ? wxString(line.c_str(), wxConvISO8859_1)
                                       : text;
}
