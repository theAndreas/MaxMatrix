// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in the terminal.
 *
 * With ANSI escape sequences, which every terminal VS Code offers understands.
 */

#include <stdio.h>
#include <fstream>
#include <sstream>
#include "SimulatorTerminal.h"

/* ANSI escape sequences */
#define SIMULATOR_TERMINAL_CLEAR_SCREEN                             "\x1b[2J"
#define SIMULATOR_TERMINAL_CURSOR_HOME                              "\x1b[H"
#define SIMULATOR_TERMINAL_CLEAR_LINE_END                           "\x1b[K"

/* a dot is two chars wide while it is drawn, so that the panel is about as wide as it is high */
#define SIMULATOR_TERMINAL_LIT                                      "\u2588\u2588"
#define SIMULATOR_TERMINAL_DARK                                     "\u00b7 "

/* a dot is one ASCII char in the result, which a test compares with a file */
#define SIMULATOR_TERMINAL_RESULT_LIT                               "#"
#define SIMULATOR_TERMINAL_RESULT_DARK                              "."

/*! \brief          clears the terminal
 *  \details        this function is called once before the first draw; every draw after it only moves the cursor home
 */
void SimulatorTerminal::Clear()
{
    fputs(SIMULATOR_TERMINAL_CLEAR_SCREEN, stdout);
}

/*! \brief          the line above the panel
 *  \details        this function says which wiring and mounting the panel is drawn for, the intensity of every module
 *                  and how many frames the sketch has sent. The modules are counted from the left as on the page;
 *                  counting the chips along the chain, as before, had them backwards on a board upside down.
 *
 *  \param[in]      panel           the panel to describe
 *  \return         the line, without a line break
 */
std::string SimulatorTerminal::GetStatusLine(SimulatorPanel const& panel)
{
    std::ostringstream line;

    line << panel.GetWiringName() << ", " << MAXMATRIX_NUMBER_OF_MODULES << " modules" << (panel.IsUpsideDown() ? ", upside down" : "")
         << ", intensity";
    for(uint8_t module = 0; module < MAXMATRIX_NUMBER_OF_MODULES; module++) line << ' ' << static_cast<uint16_t>(panel.GetIntensity(module));
    line << ", " << mock::frames << " frames" << (mock::errors ? ", TRANSFER ERRORS" : "");
    return line.str();
}

/*! \brief          draws the panel in place
 *  \details        this function moves the cursor home rather than clearing the terminal, which would flicker, and
 *                  writes the status line above the panel
 *
 *  \param[in]      panel           the panel to draw
 */
void SimulatorTerminal::Draw(SimulatorPanel const& panel)
{
    printf(SIMULATOR_TERMINAL_CURSOR_HOME "%s" SIMULATOR_TERMINAL_CLEAR_LINE_END "\n\n", GetStatusLine(panel).c_str());
    fputs(panel.GetText(SIMULATOR_TERMINAL_LIT, SIMULATOR_TERMINAL_DARK, "\n").c_str(), stdout);
    fflush(stdout);
}

/*! \brief          prints the panel at the end of a run
 *  \details        this function prints the panel once, in ASCII, and compares it with the expected panel if there is one
 *
 *  \param[in]      panel           the panel to print
 *  \param[in]      expect_file     the file with the expected panel, or nullptr
 *  \return         true
 *                  false           the panel differs from the file, or the file cannot be read
 */
bool SimulatorTerminal::PrintResult(SimulatorPanel const& panel, char const* expect_file)
{
    std::string const result = panel.GetText(SIMULATOR_TERMINAL_RESULT_LIT, SIMULATOR_TERMINAL_RESULT_DARK, "\n");
    std::string expected;

    fputs(result.c_str(), stdout);
    if(expect_file == nullptr) return true;

    if(!ReadFile(expect_file, &expected) || expected != result) {
        fprintf(stderr, "differs from %s:\n%s", expect_file, expected.c_str());
        return false;
    } else return true;
}

/*! \brief          reads a whole file
 *  \details        this function returns what was read even when it fails, so that the caller can show it
 *
 *  \param[in]      path            the file
 *  \param[out]     text            what it holds
 *  \return         true
 *                  false           the file cannot be read
 */
bool SimulatorTerminal::ReadFile(char const* path, std::string* text)
{
    std::ifstream file(path);
    std::stringstream content;

    content << file.rdbuf();
    *text = content.str();
    return static_cast<bool>(file);
}
