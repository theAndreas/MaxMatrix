/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       SimulatorTerminal.cpp
 *      \brief      The simulator's panel in the terminal
 *
 *      \details    With ANSI escape sequences, which every terminal VS Code offers understands.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_TERMINAL_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <fstream>
#include <sstream>
#include "SimulatorTerminal.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
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


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  clear()
******************************************************************************************************************************************************/
/*! \brief          clears the terminal
 *  \details        this function is called once before the first draw; every draw after it only moves the cursor home
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorTerminal::clear()
{
    fputs(SIMULATOR_TERMINAL_CLEAR_SCREEN, stdout);
} /* clear */


/******************************************************************************************************************************************************
  getStatusLine()
******************************************************************************************************************************************************/
/*! \brief          the line above the panel
 *  \details        this function says which wiring and mounting the panel is drawn for, the intensity of every module
 *                  and how many frames the sketch has sent
 *
 *  \param[in]      Panel           the panel to describe
 *  \return         the line, without a line break
 *****************************************************************************************************************************************************/
std::string SimulatorTerminal::getStatusLine(const SimulatorPanel& Panel)
{
    std::ostringstream Line;

    Line << Panel.getWiringName() << ", " << MAXMATRIX_NUMBER_OF_MODULES << " modules" << (Panel.isUpsideDown() ? ", upside down" : "")
         << ", intensity";
    for(size_t Chip = Mock::Chain.Chips.size(); Chip > 0; Chip--) Line << ' ' << static_cast<unsigned>(Mock::Chain.Chips[Chip - 1].Intensity);
    Line << ", " << Mock::Frames << " frames" << (Mock::Errors ? ", TRANSFER ERRORS" : "");
    return Line.str();
} /* getStatusLine */


/******************************************************************************************************************************************************
  draw()
******************************************************************************************************************************************************/
/*! \brief          draws the panel in place
 *  \details        this function moves the cursor home rather than clearing the terminal, which would flicker, and
 *                  writes the status line above the panel
 *
 *  \param[in]      Panel           the panel to draw
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorTerminal::draw(const SimulatorPanel& Panel)
{
    printf(SIMULATOR_TERMINAL_CURSOR_HOME "%s" SIMULATOR_TERMINAL_CLEAR_LINE_END "\n\n", getStatusLine(Panel).c_str());
    fputs(Panel.getText(SIMULATOR_TERMINAL_LIT, SIMULATOR_TERMINAL_DARK, "\n").c_str(), stdout);
    fflush(stdout);
} /* draw */


/******************************************************************************************************************************************************
  printResult()
******************************************************************************************************************************************************/
/*! \brief          prints the panel at the end of a run
 *  \details        this function prints the panel once, in ASCII, and compares it with the expected panel if there is one
 *
 *  \param[in]      Panel           the panel to print
 *  \param[in]      ExpectFile      the file with the expected panel, or nullptr
 *  \return         E_OK
 *                  E_NOT_OK        the panel differs from the file, or the file cannot be read
 *****************************************************************************************************************************************************/
stdReturnType SimulatorTerminal::printResult(const SimulatorPanel& Panel, const char* ExpectFile)
{
    const std::string Result = Panel.getText(SIMULATOR_TERMINAL_RESULT_LIT, SIMULATOR_TERMINAL_RESULT_DARK, "\n");
    std::string Expected;

    fputs(Result.c_str(), stdout);
    if(ExpectFile == nullptr) return E_OK;

    if(E_OK != readFile(ExpectFile, &Expected) || Expected != Result) {
        fprintf(stderr, "differs from %s:\n%s", ExpectFile, Expected.c_str());
        return E_NOT_OK;
    } else return E_OK;
} /* printResult */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  readFile()
******************************************************************************************************************************************************/
/*! \brief          reads a whole file
 *  \details        this function returns what was read even when it fails, so that the caller can show it
 *
 *  \param[in]      Path            the file
 *  \param[out]     Text            what it holds
 *  \return         E_OK
 *                  E_NOT_OK        the file cannot be read
 *****************************************************************************************************************************************************/
stdReturnType SimulatorTerminal::readFile(const char* Path, std::string* Text)
{
    std::ifstream File(Path);
    std::stringstream Content;

    Content << File.rdbuf();
    *Text = Content.str();
    return File ? E_OK : E_NOT_OK;
} /* readFile */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
