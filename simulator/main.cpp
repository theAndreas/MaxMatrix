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
/**     \file       main.cpp
 *      \brief      Runs an example sketch from examples/ on the host and draws the LED panel
 *
 *      \details    build/Simulator-ScrollText-bitbang [--wiring FC16] [--upside-down] [--web [--port PORT]]
 *                                                     [--for SECONDS [--expect FILE]]
 *
 *                  The sketch is compiled unchanged against the Arduino stand-in and the MAX7219 model in test/mock/:
 *                  setup() runs once, loop() over and over, millis() and delay() on the host clock.
 *
 *                  Without --for it runs until Ctrl+C and redraws the panel in the terminal. With --for it runs that
 *                  long, prints the panel once and exits; --expect then compares the printout with a file and fails on
 *                  a difference.
 *
 *                  --web draws in a browser instead, on http://localhost:8080/ (simulator/index.html). --port picks
 *                  another port, so that two simulators, or a simulator and the tests, can run side by side.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_MAIN_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include "Arduino.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"
#include "SimulatorWeb.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* the examples put chip select on pin 10 */
#define SIMULATOR_CHIP_SELECT_PIN                                   10u

/* exit codes */
#define SIMULATOR_EXIT_OK                                           0
#define SIMULATOR_EXIT_FAILED                                       1
#define SIMULATOR_EXIT_USAGE                                        2


/******************************************************************************************************************************************************
 *  GLOBAL FUNCTION PROTOTYPES
 *****************************************************************************************************************************************************/
/* the sketch's, compiled into the same program */
void setup();
void loop();


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
/* Mock::Tick is a plain function pointer, so what tick() works on has to be reachable without an object. */
static SimulatorOptions Options;
static SimulatorPanel Panel;
static SimulatorWeb Web;


/******************************************************************************************************************************************************
 * L O C A L   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  finish()
******************************************************************************************************************************************************/
/*! \brief          ends a run with --for
 *  \details        this function prints the panel and exits, from within the sketch's loop() or delay(), which never
 *                  return on their own
 *
 *  \return         - (does not return)
 *****************************************************************************************************************************************************/
static void finish()
{
    const bool Passed = E_OK == SimulatorTerminal::printResult(Panel, Options.getExpectFile()) && Mock::Errors == 0;

    exit(Passed ? SIMULATOR_EXIT_OK : SIMULATOR_EXIT_FAILED);
} /* finish */


/******************************************************************************************************************************************************
  tick()
******************************************************************************************************************************************************/
/*! \brief          follows the sketch
 *  \details        this function is called after every frame and every millisecond of delay(), so that the panel is
 *                  drawn while a sketch blocks. In a run with --for the terminal draws only at the end.
 *
 *  \return         -
 *****************************************************************************************************************************************************/
static void tick()
{
    const uint32_t Now = millis();

    if(Options.isWeb()) Web.task(Panel);
    if(Options.getRunFor() != 0 && Now >= Options.getRunFor()) finish();

    if(Panel.isDrawDue(Now)) {
        if(Options.isWeb()) Web.send(Panel);
        else if(Options.getRunFor() == 0) SimulatorTerminal::draw(Panel);
        Panel.setDrawn(Now);
    }
} /* tick */


/******************************************************************************************************************************************************
  begin()
******************************************************************************************************************************************************/
/*! \brief          prepares the output
 *  \details        this function opens the port for the page, or clears the terminal for a run that draws in it
 *
 *  \return         E_OK
 *                  E_NOT_OK        the port is taken
 *****************************************************************************************************************************************************/
static stdReturnType begin()
{
    if(Options.isWeb()) {
        if(E_OK != Web.begin(Options.getPort(), SIMULATOR_PAGE)) {
            fprintf(stderr, "port %u is taken, maybe by another simulator\n", Options.getPort());
            return E_NOT_OK;
        }
        printf("http://localhost:%u/\n", Options.getPort());
        fflush(stdout);
    } else if(Options.getRunFor() == 0) {
        SimulatorTerminal::clear();
    }
    return E_OK;
} /* begin */


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  main()
******************************************************************************************************************************************************/
/*! \brief          runs the sketch
 *  \details        this function runs setup() once and loop() until the run ends
 *
 *  \param[in]      argc            the number of arguments, the program's name included
 *  \param[in]      argv            the arguments
 *  \return         SIMULATOR_EXIT_FAILED       the port is taken
 *                  SIMULATOR_EXIT_USAGE        the command line is wrong
 *****************************************************************************************************************************************************/
int main(int argc, char** argv)
{
    if(E_OK != Options.parse(argc, argv) || E_OK != Panel.setWiring(Options.getWiringName())) {
        SimulatorOptions::printUsage(argv[0]);
        return SIMULATOR_EXIT_USAGE;
    }
    Panel.setUpsideDown(Options.isUpsideDown());
    Mock::reset(SIMULATOR_CHIP_SELECT_PIN, MAXMATRIX_NUMBER_OF_MODULES);
    Mock::Tick = tick;
    if(E_OK != begin()) return SIMULATOR_EXIT_FAILED;

    setup();
    for(;;) {
        loop();
        tick();
        /* a sketch polling millis() would otherwise keep a core busy */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
} /* main */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
