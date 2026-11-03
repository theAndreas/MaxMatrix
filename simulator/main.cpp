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
 *      \details    build/Simulator-ScrollText-bitbang [--wiring FC16] [--upside-down] [--serial LINE]...
 *                                                     [--web [--port PORT]] [--for SECONDS [--expect FILE]]
 *
 *                  The sketch is compiled unchanged against the Arduino stand-in and the MAX7219 model in test/mock/:
 *                  setup() runs once, loop() over and over, millis() and delay() on the host clock.
 *
 *                  Without --for it runs until Ctrl+C and redraws the panel in the terminal. With --for it runs that
 *                  long, prints the panel once and exits; --expect then compares the printout with a file and fails on
 *                  a difference.
 *
 *                  --serial hands the sketch's Serial a line to read, as if it was typed; each one given is read in turn.
 *
 *                  --web draws in a browser instead, on http://localhost:8080/ (simulator/index.html). --port picks
 *                  another port, so that two simulators, or a simulator and the tests, can run side by side.
 *
 *****************************************************************************************************************************************************/
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <chrono>
#include <thread>
#include "Arduino.h"
#include "simulator_options.h"
#include "simulator_panel.h"
#include "simulator_terminal.h"
#include "simulator_web.h"


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
/* mock::tick is a plain function pointer, so what Tick() works on has to be reachable without an object. */
static SimulatorOptions options;
static SimulatorPanel panel;
static SimulatorWeb web;


/******************************************************************************************************************************************************
 * L O C A L   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  Finish()
******************************************************************************************************************************************************/
/*! \brief          ends a run with --for
 *  \details        this function prints the panel and exits, from within the sketch's loop() or delay(), which never
 *                  return on their own
 *****************************************************************************************************************************************************/
static void Finish()
{
    bool const passed = SimulatorTerminal::PrintResult(panel, options.GetExpectFile()) && mock::errors == 0;

    exit(passed ? SIMULATOR_EXIT_OK : SIMULATOR_EXIT_FAILED);
} /* Finish */


/******************************************************************************************************************************************************
  Tick()
******************************************************************************************************************************************************/
/*! \brief          follows the sketch
 *  \details        this function is called after every frame and every millisecond of delay(), so that the panel is
 *                  drawn while a sketch blocks. In a run with --for the terminal draws only at the end.
 *****************************************************************************************************************************************************/
static void Tick()
{
    uint32_t const now = millis();

    if(options.IsWeb()) web.Task(panel);
    if(options.GetRunFor() != 0 && now >= options.GetRunFor()) Finish();

    if(panel.IsDrawDue(now)) {
        if(options.IsWeb()) web.Send(panel);
        else if(options.GetRunFor() == 0) SimulatorTerminal::Draw(panel);
        panel.SetDrawn(now);
    }
} /* Tick */


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
    /* the sketch is built in; an .elf is for SimulatorAvr */
    if(!options.Parse(argc, argv) || options.GetElfFile() != nullptr || !panel.SetWiring(options.GetWiringName())) {
        SimulatorOptions::PrintUsage(argv[0]);
        return SIMULATOR_EXIT_USAGE;
    }
    panel.SetUpsideDown(options.IsUpsideDown());
    mock::Reset(SIMULATOR_CHIP_SELECT_PIN, MAXMATRIX_NUMBER_OF_MODULES);
    mock::serial_input = options.GetSerialInput();
    mock::tick = Tick;
    if(!Begin()) return SIMULATOR_EXIT_FAILED;

    setup();
    for(;;) {
        loop();
        Tick();
        /* a sketch polling millis() would otherwise keep a core busy */
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
} /* main */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
