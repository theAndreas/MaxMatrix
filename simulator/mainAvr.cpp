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
/**     \file       mainAvr.cpp
 *      \brief      Runs an example as the Uno runs it, in simavr, and draws the LED panel
 *
 *      \details    build/SimulatorAvr --elf FILE [--wiring FC16] [--upside-down] [--serial LINE]...
 *                                     [--web [--port PORT]] [--for SECONDS [--expect FILE]]
 *
 *                  The .elf arduino-cli made of an example runs on a simulated ATmega328P, SimulatorAvr, and drives the
 *                  MAX7219 model through the pins and the SPI output, rather than through the Arduino stand-in. The rest
 *                  is the simulator of main.cpp: the same options, panel, terminal and page.
 *
 *                  With --web, or without --for, the simulated time keeps to the host's clock, so that the panel moves
 *                  as it would on the board and the page talks to it in real time. Otherwise it runs as fast as simavr
 *                  can, and as the time is simulated, a run of the same .elf ends with the same panel every time, also
 *                  of an example that moves.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_MAIN_AVR_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <chrono>
#include <thread>
#include "Arduino.h"
#include "SimulatorAvr.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"
#include "SimulatorWeb.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* the examples put chip select on pin 10 */
#define SIMULATOR_CHIP_SELECT_PIN                                   10u

/* exit codes, as in main.cpp */
#define SIMULATOR_EXIT_OK                                           0
#define SIMULATOR_EXIT_FAILED                                       1
#define SIMULATOR_EXIT_USAGE                                        2


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
static SimulatorOptions Options;
static SimulatorPanel Panel;
static SimulatorWeb Web;
static SimulatorAvr Avr;


/******************************************************************************************************************************************************
 * L O C A L   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  begin()
******************************************************************************************************************************************************/
/*! \brief          prepares the output
 *  \details        this function opens the port for the page, or clears the terminal for a run that draws in it
 *
 *  \return         true
 *                  false           the port is taken
 *****************************************************************************************************************************************************/
static bool begin()
{
    if(Options.isWeb()) {
        if(!Web.begin(Options.getPort(), SIMULATOR_PAGE)) {
            fprintf(stderr, "port %u is taken, maybe by another simulator\n", Options.getPort());
            return false;
        }
        printf("http://localhost:%u/\n", Options.getPort());
        fflush(stdout);
    } else if(Options.getRunFor() == 0) {
        SimulatorTerminal::clear();
    }
    return true;
} /* begin */


/******************************************************************************************************************************************************
  follow()
******************************************************************************************************************************************************/
/*! \brief          follows the AVR, once per simulated millisecond
 *  \details        this function serves the page and draws the panel when it is due, as tick() of main.cpp does
 *
 *  \param[in]      Now             the simulated time
 *****************************************************************************************************************************************************/
static void follow(uint32_t Now)
{
    if(Options.isWeb()) Web.task(Panel);
    if(Panel.isDrawDue(Now)) {
        if(Options.isWeb()) Web.send(Panel);
        else if(Options.getRunFor() == 0) SimulatorTerminal::draw(Panel);
        Panel.setDrawn(Now);
    }
} /* follow */


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  main()
******************************************************************************************************************************************************/
/*! \brief          runs the example
 *  \details        this function runs the AVR one simulated millisecond at a time until the run ends
 *
 *  \param[in]      argc            the number of arguments, the program's name included
 *  \param[in]      argv            the arguments
 *  \return         SIMULATOR_EXIT_OK           the run with --for ended with the panel expected and no transfer error
 *                  SIMULATOR_EXIT_FAILED       the .elf cannot be read, the port is taken, the AVR stopped, or the run
 *                                              with --for ended with another panel or a transfer error
 *                  SIMULATOR_EXIT_USAGE        the command line is wrong
 *****************************************************************************************************************************************************/
int main(int argc, char** argv)
{
    if(!Options.parse(argc, argv) || Options.getElfFile() == nullptr || !Panel.setWiring(Options.getWiringName())) {
        SimulatorOptions::printUsage(argv[0]);
        return SIMULATOR_EXIT_USAGE;
    }
    Panel.setUpsideDown(Options.isUpsideDown());
    Mock::reset(SIMULATOR_CHIP_SELECT_PIN, MAXMATRIX_NUMBER_OF_MODULES);
    Mock::SerialInput = Options.getSerialInput();
    if(!Avr.load(Options.getElfFile())) {
        fprintf(stderr, "%s is no .elf for the %s\n", Options.getElfFile(), SIMULATOR_AVR_MCU);
        return SIMULATOR_EXIT_FAILED;
    }
    if(!begin()) return SIMULATOR_EXIT_FAILED;

    /* a page, and whoever is in front of it, speaks in real time */
    bool const RealTime = Options.isWeb() || Options.getRunFor() == 0;
    std::chrono::steady_clock::time_point const Start = std::chrono::steady_clock::now();
    for(uint32_t Now = 1; Options.getRunFor() == 0 || Now <= Options.getRunFor(); Now++) {
        if(!Avr.run(Now)) {
            fprintf(stderr, "the AVR stopped after %u ms\n", Now);
            return SIMULATOR_EXIT_FAILED;
        }
        follow(Now);
        /* simavr runs faster than the Uno, so in real time it waits for the host's clock to catch up */
        if(RealTime) std::this_thread::sleep_until(Start + std::chrono::milliseconds(Now));
    }
    bool const Passed = SimulatorTerminal::printResult(Panel, Options.getExpectFile()) && Mock::Errors == 0;
    return Passed ? SIMULATOR_EXIT_OK : SIMULATOR_EXIT_FAILED;
} /* main */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
