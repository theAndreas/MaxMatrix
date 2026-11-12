// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in the terminal.
 *
 * With ANSI escape sequences, which every terminal VS Code offers understands.
 */

#include "SimulatorTerminal.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

/* ANSI escape sequences */
constexpr char kClearScreen[] = "\x1b[2J";
constexpr char kCursorHome[] = "\x1b[H";
constexpr char kClearLineEnd[] = "\x1b[K";

/* a dot is two chars wide while it is drawn, so that the panel is about as wide
 * as it is high */
constexpr char kLit[] = "\u2588\u2588";
constexpr char kDark[] = "\u00b7 ";

/* a dot is one ASCII char in the result, which a test compares with a file */
constexpr char kResultLit[] = "#";
constexpr char kResultDark[] = ".";

}  // namespace

void SimulatorTerminal::Clear() { std::cout << kClearScreen; }

std::string SimulatorTerminal::GetStatusLine(SimulatorPanel const& panel) {
  std::ostringstream line;

  line << panel.GetWiringName() << ", "
       << static_cast<uint16_t>(SimulatorPanel::GetNumberOfModules())
       << " modules" << (panel.IsUpsideDown() ? ", upside down" : "")
       << ", intensity";
  // The modules are counted from the left, as on the page: counting the chips
  // along the chain, as before, had them backwards on a board upside down.
  for (uint8_t module = 0; module < SimulatorPanel::GetNumberOfModules();
       module++) {
    line << ' ' << static_cast<uint16_t>(panel.GetIntensity(module));
  }
  line << ", " << mock::frames << " frames"
       << ((mock::errors != 0) ? ", TRANSFER ERRORS" : "");
  return line.str();
}

void SimulatorTerminal::Draw(SimulatorPanel const& panel) {
  // The cursor goes home rather than the terminal being cleared, which would
  // flicker.
  std::cout << kCursorHome << GetStatusLine(panel) << kClearLineEnd << "\n\n"
            << panel.GetText(kLit, kDark, "\n");
  std::cout.flush();
}

bool SimulatorTerminal::PrintResult(SimulatorPanel const& panel,
                                    char const* expect_file) {
  std::string const result = panel.GetText(kResultLit, kResultDark, "\n");
  std::string expected;

  std::cout << result;
  if (expect_file == nullptr) {
    return true;
  }

  if (!ReadFile(expect_file, &expected) || expected != result) {
    std::cerr << "differs from " << expect_file << ":\n" << expected;
    return false;
  }
  return true;
}

// Reads the whole file at `path` into `text`. Returns false if it cannot be
// read; `text` then holds what was read even so, so that the caller can show
// it.
bool SimulatorTerminal::ReadFile(char const* path, std::string* text) {
  std::ifstream const file{path};
  std::stringstream content;

  content << file.rdbuf();
  *text = content.str();
  return static_cast<bool>(file);
}
