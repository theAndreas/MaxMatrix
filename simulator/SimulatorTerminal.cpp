// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in the terminal.
 *
 * With ANSI escape sequences, which every terminal VS Code offers understands.
 */

#include "SimulatorTerminal.h"

#include <stdio.h>

#include <fstream>
#include <sstream>
#include <string>

/* ANSI escape sequences */
#define SIMULATOR_TERMINAL_CLEAR_SCREEN "\x1b[2J"
#define SIMULATOR_TERMINAL_CURSOR_HOME "\x1b[H"
#define SIMULATOR_TERMINAL_CLEAR_LINE_END "\x1b[K"

/* a dot is two chars wide while it is drawn, so that the panel is about as wide
 * as it is high */
#define SIMULATOR_TERMINAL_LIT "\u2588\u2588"
#define SIMULATOR_TERMINAL_DARK "\u00b7 "

/* a dot is one ASCII char in the result, which a test compares with a file */
#define SIMULATOR_TERMINAL_RESULT_LIT "#"
#define SIMULATOR_TERMINAL_RESULT_DARK "."

void SimulatorTerminal::Clear() {
  fputs(SIMULATOR_TERMINAL_CLEAR_SCREEN, stdout);
}

std::string SimulatorTerminal::GetStatusLine(SimulatorPanel const& panel) {
  std::ostringstream line;

  line << panel.GetWiringName() << ", "
       << static_cast<uint16_t>(panel.GetNumberOfModules()) << " modules"
       << (panel.IsUpsideDown() ? ", upside down" : "") << ", intensity";
  // The modules are counted from the left, as on the page: counting the chips
  // along the chain, as before, had them backwards on a board upside down.
  for (uint8_t module = 0; module < panel.GetNumberOfModules(); module++) {
    line << ' ' << static_cast<uint16_t>(panel.GetIntensity(module));
  }
  line << ", " << mock::frames << " frames"
       << (mock::errors ? ", TRANSFER ERRORS" : "");
  return line.str();
}

void SimulatorTerminal::Draw(SimulatorPanel const& panel) {
  // The cursor goes home rather than the terminal being cleared, which would
  // flicker.
  printf(SIMULATOR_TERMINAL_CURSOR_HOME "%s" SIMULATOR_TERMINAL_CLEAR_LINE_END
                                        "\n\n",
         GetStatusLine(panel).c_str());
  fputs(panel.GetText(SIMULATOR_TERMINAL_LIT, SIMULATOR_TERMINAL_DARK, "\n")
            .c_str(),
        stdout);
  fflush(stdout);
}

bool SimulatorTerminal::PrintResult(SimulatorPanel const& panel,
                                    char const* expect_file) {
  std::string const result = panel.GetText(
      SIMULATOR_TERMINAL_RESULT_LIT, SIMULATOR_TERMINAL_RESULT_DARK, "\n");
  std::string expected;

  fputs(result.c_str(), stdout);
  if (expect_file == nullptr) return true;

  if (!ReadFile(expect_file, &expected) || expected != result) {
    fprintf(stderr, "differs from %s:\n%s", expect_file, expected.c_str());
    return false;
  } else {
    return true;
  }
}

// Reads the whole file at `path` into `text`. Returns false if it cannot be
// read; `text` then holds what was read even so, so that the caller can show
// it.
bool SimulatorTerminal::ReadFile(char const* path, std::string* text) {
  std::ifstream file(path);
  std::stringstream content;

  content << file.rdbuf();
  *text = content.str();
  return static_cast<bool>(file);
}
