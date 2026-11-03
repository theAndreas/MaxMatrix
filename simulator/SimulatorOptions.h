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
/**     \file       SimulatorOptions.h
 *      \brief      The command line of the simulator
 *
 *      \details    [--elf FILE] [--wiring NAME] [--upside-down] [--serial LINE]... [--web [--port PORT]]
 *                  [--for SECONDS [--expect FILE]]
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_OPTIONS_H_
#define _SIMULATOR_OPTIONS_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include "SimulatorPanel.h"


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
    std::string WiringName;
    bool UpsideDown;
    bool Web;
    uint16_t Port;
    uint32_t RunFor;
    char const* ExpectFile;
    char const* ElfFile;
    std::string SerialInput;

    bool parseOption(int, char**, int*);
    static bool convertToPort(char const*, uint16_t*);
    static bool convertToRunFor(char const*, uint32_t*);

  public:
    SimulatorOptions();

    // get methods
    std::string const& getWiringName() const { return WiringName; }
    bool isUpsideDown() const { return UpsideDown; }
    bool isWeb() const { return Web; }
    uint16_t getPort() const { return Port; }
    uint32_t getRunFor() const { return RunFor; }
    char const* getExpectFile() const { return ExpectFile; }
    char const* getElfFile() const { return ElfFile; }
    /* the lines of every --serial, each ended with a line feed, for the sketch's Serial to read */
    std::string const& getSerialInput() const { return SerialInput; }

    // methods
    bool parse(int, char**);
    static void printUsage(char const*);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
