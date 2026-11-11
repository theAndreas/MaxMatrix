// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The LED panel of the simulator, as a module of the chosen wiring
 *        lights it.
 *
 * The wiring by name, the panel as text, the brightness of every module, and
 * whether it is time to draw.
 */

#include "SimulatorPanel.h"

#include <string>

/* the four wirings MD_MAX72XX has names for, beside DR0CR0RR0 ... DR1CR1RR1
 * from kMax7219Wirings */
static struct {
  char const* name;
  Max7219Wiring const* wiring;
} const kNamedWirings[] = {
    {"FC16", &kMaX7219WiringFC16},
    {"GENERIC", &kMaX7219WiringGeneric},
    {"PAROLA", &kMaX7219WiringParola},
    {"ICSTATION", &kMaX7219WiringIcstation},
};

SimulatorPanel::SimulatorPanel() {
  wiring_name_ = SIMULATOR_PANEL_DEFAULT_WIRING;
  wiring_ = FindWiring(wiring_name_);
  upside_down_ = false;
  drawn_frames_ = -1;
  last_draw_ = 0;
  draw_requested_ = true;
}

std::string SimulatorPanel::GetText(char const* lit, char const* dark,
                                    char const* end_of_row) const {
  std::string text;

  for (uint8_t y = 0; y < max_matrix::kRowNumberOfModule; y++) {
    for (uint8_t x = 0; x < GetNumberOfColumns(); x++)
      text += mock::chain.IsLit(*wiring_, x, y, upside_down_) ? lit : dark;
    text += end_of_row;
  }
  return text;
}

uint8_t SimulatorPanel::GetIntensity(uint8_t module) const {
  return mock::chain
      .ChipAt(module * max_matrix::kColumnNumberOfModule, upside_down_)
      .intensity;
}

bool SimulatorPanel::SetWiring(std::string const& name) {
  Max7219Wiring const* found = FindWiring(name);

  if (found != nullptr) {
    wiring_ = found;
    wiring_name_ = name;
    draw_requested_ = true;
    return true;
  } else {
    return false;
  }
}

void SimulatorPanel::SetUpsideDown(bool upside_down) {
  upside_down_ = upside_down;
  draw_requested_ = true;
}

bool SimulatorPanel::IsDrawDue(uint32_t now) const {
  bool const changed = mock::frames != drawn_frames_ || draw_requested_;

  return changed && now - last_draw_ >= SIMULATOR_PANEL_REDRAW_INTERVAL;
}

void SimulatorPanel::SetDrawn(uint32_t now) {
  drawn_frames_ = mock::frames;
  last_draw_ = now;
  draw_requested_ = false;
}

// Finds a wiring by its name: among the names MD_MAX72XX has first, then among
// DR0CR0RR0 ... DR1CR1RR1. Returns nullptr if no wiring has this name.
Max7219Wiring const* SimulatorPanel::FindWiring(std::string const& name) {
  for (auto const& named : kNamedWirings) {
    if (name == named.name) return named.wiring;
  }
  for (Max7219Wiring const& each : kMax7219Wirings) {
    if (name == each.name) return &each;
  }
  return nullptr;
}
