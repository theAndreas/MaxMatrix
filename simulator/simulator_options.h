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
/**     \file       simulator_options.h
 *      \brief      The command line of the simulator
 *
 *      \details    [--elf FILE] [--wiring NAME] [--upside-down] [--serial LINE]... [--web [--port PORT]]
 *                  [--for SECONDS [--expect FILE]]
 *
 *****************************************************************************************************************************************************/
#ifndef MAXMATRIX_SIMULATOR_SIMULATOR_OPTIONS_H_
#define MAXMATRIX_SIMULATOR_SIMULATOR_OPTIONS_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include "simulator_panel.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorOptions Parameter */
#define SIMULATOR_OPTIONS_DEFAULT_PORT                              8080u
#define SIMULATOR_OPTIONS_MAX_PORT                                  65535u
#define SIMULATOR_OPTIONS_MAX_RUN_FOR                               UINT32_MAX /* milliseconds */


/******************************************************************************************************************************************************
 *  CLASS  SimulatorOptions
 *****************************************************************************************************************************************************/
/* Only reads the command line; whether the wiring exists is the panel's to say, which knows the names, and whether a
 * simulator takes an .elf is its own: SimulatorAvr needs one, the simulator a sketch is built into refuses it. */
class SimulatorOptions
{
  private:
    std::string wiring_name_;
    bool upside_down_;
    bool web_;
    uint16_t port_;
    uint32_t run_for_;
    char const* expect_file_;
    char const* elf_file_;
    std::string serial_input_;

    bool ParseOption(int, char**, int*);
    static bool ConvertToPort(char const*, uint16_t*);
    static bool ConvertToRunFor(char const*, uint32_t*);

  public:
    SimulatorOptions();

    // get methods
    std::string const& GetWiringName() const { return wiring_name_; }
    bool IsUpsideDown() const { return upside_down_; }
    bool IsWeb() const { return web_; }
    uint16_t GetPort() const { return port_; }
    uint32_t GetRunFor() const { return run_for_; }
    char const* GetExpectFile() const { return expect_file_; }
    char const* GetElfFile() const { return elf_file_; }
    /* the lines of every --serial, each ended with a line feed, for the sketch's Serial to read */
    std::string const& GetSerialInput() const { return serial_input_; }

    // methods
    bool Parse(int, char**);
    static void PrintUsage(char const*);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
