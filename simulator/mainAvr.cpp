// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Runs an example as the Uno runs it, in simavr, and draws the LED panel.
 *
 * Build/SimulatorAvr --elf FILE [--wiring FC16] [--upside-down] [--serial LINE]... [--web [--port
 * PORT]] [--for SECONDS [--expect FILE]].
 *
 * The .elf arduino-cli made of an example runs on a simulated ATmega328P, SimulatorAvr, and drives
 * the MAX7219 model through the pins and the SPI output, rather than through the Arduino stand-in.
 * The rest is the simulator of main.cpp: the same options, panel, terminal and page.
 *
 * With --web, or without --for, the simulated time keeps to the host's clock, so that the panel
 * moves as it would on the board and the page talks to it in real time. Otherwise it runs as fast
 * as simavr can, and as the time is simulated, a run of the same .elf ends with the same panel
 * every time, also of an example that moves.
 */

#include <stdio.h>
#include <chrono>
#include <thread>
#include "Arduino.h"
#include "SimulatorAvr.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"
#include "SimulatorWeb.h"

/* the examples put chip select on pin 10 */
#define SIMULATOR_CHIP_SELECT_PIN                                   10u

/* exit codes, as in main.cpp */
#define SIMULATOR_EXIT_OK                                           0
#define SIMULATOR_EXIT_FAILED                                       1
#define SIMULATOR_EXIT_USAGE                                        2

static SimulatorOptions options;
static SimulatorPanel panel;
static SimulatorWeb web;
static SimulatorAvr avr;

/*!
 * \brief Prepares the output.
 *
 * It opens the port for the page, or clears the terminal for a run that draws in it.
 *
 * \return True once the output is prepared, or false if the port is taken.
 */
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
}

/*!
 * \brief Follows the AVR, once per simulated millisecond.
 *
 * It serves the page and draws the panel when it is due, as Tick() of main.cpp does.
 *
 * \param[in] now The simulated time.
 */
static void Follow(uint32_t now)
{
    if(options.IsWeb()) web.Task(panel);
    if(panel.IsDrawDue(now)) {
        if(options.IsWeb()) web.Send(panel);
        else if(options.GetRunFor() == 0) SimulatorTerminal::Draw(panel);
        panel.SetDrawn(now);
    }
}

/*!
 * \brief Runs the example.
 *
 * It runs the AVR one simulated millisecond at a time until the run ends.
 *
 * \param[in] argc The number of arguments, the program's name included.
 * \param[in] argv The arguments.
 * \return `SIMULATOR_EXIT_OK` if the run with `--for` ended with the panel expected and no transfer
 *         error; `SIMULATOR_EXIT_FAILED` if the .elf cannot be read, the port is taken, the AVR
 *         stopped, or the run with `--for` ended with another panel or a transfer error; or
 *         `SIMULATOR_EXIT_USAGE` if the command line is wrong.
 */
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
        // simavr runs faster than the Uno, so in real time it waits for the host's clock to catch
        // up.
        if(real_time) std::this_thread::sleep_until(start + std::chrono::milliseconds(now));
    }
    bool const passed = SimulatorTerminal::PrintResult(panel, options.GetExpectFile()) && mock::errors == 0;
    return passed ? SIMULATOR_EXIT_OK : SIMULATOR_EXIT_FAILED;
}
