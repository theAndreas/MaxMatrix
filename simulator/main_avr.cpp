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
/**     \file       main_avr.cpp
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
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <chrono>
#include <thread>
#include "Arduino.h"
#include "simulator_avr.h"
#include "simulator_options.h"
#include "simulator_panel.h"
#include "simulator_terminal.h"
#include "simulator_web.h"


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
static SimulatorOptions options;
static SimulatorPanel panel;
static SimulatorWeb web;
static SimulatorAvr avr;


/******************************************************************************************************************************************************
 * L O C A L   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  Begin()
******************************************************************************************************************************************************/
/*! \brief          prepares the output
 *  \details        this function opens the port for the page, or clears the terminal for a run that draws in it
 *
 *  \return         true
 *                  false           the port is taken
 *****************************************************************************************************************************************************/
static bool Begin()
{
    if(options.IsWeb()) {
        if(!web.Begin(options.GetPort(), SIMULATOR_PAGE)) {
            fprintf(stderr, "port %u is taken, maybe by another simulator\n", options.GetPort());
            return false;
        }
        printf("http://localhost:%u/\n", options.GetPort());
        fflush(stdout);
    } else if(options.GetRunFor() == 0) {
        SimulatorTerminal::Clear();
    }
    return true;
} /* Begin */


/******************************************************************************************************************************************************
  Follow()
******************************************************************************************************************************************************/
/*! \brief          follows the AVR, once per simulated millisecond
 *  \details        this function serves the page and draws the panel when it is due, as Tick() of main.cpp does
 *
 *  \param[in]      now             the simulated time
 *****************************************************************************************************************************************************/
static void Follow(uint32_t now)
{
    if(options.IsWeb()) web.Task(panel);
    if(panel.IsDrawDue(now)) {
        if(options.IsWeb()) web.Send(panel);
        else if(options.GetRunFor() == 0) SimulatorTerminal::Draw(panel);
        panel.SetDrawn(now);
    }
} /* Follow */


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
    if(!options.Parse(argc, argv) || options.GetElfFile() == nullptr || !panel.SetWiring(options.GetWiringName())) {
        SimulatorOptions::PrintUsage(argv[0]);
        return SIMULATOR_EXIT_USAGE;
    }
    panel.SetUpsideDown(options.IsUpsideDown());
    mock::Reset(SIMULATOR_CHIP_SELECT_PIN, MAXMATRIX_NUMBER_OF_MODULES);
    mock::serial_input = options.GetSerialInput();
    if(!avr.Load(options.GetElfFile())) {
        fprintf(stderr, "%s is no .elf for the %s\n", options.GetElfFile(), SIMULATOR_AVR_MCU);
        return SIMULATOR_EXIT_FAILED;
    }
    if(!Begin()) return SIMULATOR_EXIT_FAILED;

    /* a page, and whoever is in front of it, speaks in real time */
    bool const real_time = options.IsWeb() || options.GetRunFor() == 0;
    std::chrono::steady_clock::time_point const start = std::chrono::steady_clock::now();
    for(uint32_t now = 1; options.GetRunFor() == 0 || now <= options.GetRunFor(); now++) {
        if(!avr.Run(now)) {
            fprintf(stderr, "the AVR stopped after %u ms\n", now);
            return SIMULATOR_EXIT_FAILED;
        }
        Follow(now);
        /* simavr runs faster than the Uno, so in real time it waits for the host's clock to catch up */
        if(real_time) std::this_thread::sleep_until(start + std::chrono::milliseconds(now));
    }
    bool const passed = SimulatorTerminal::PrintResult(panel, options.GetExpectFile()) && mock::errors == 0;
    return passed ? SIMULATOR_EXIT_OK : SIMULATOR_EXIT_FAILED;
} /* main */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
