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
 *      \details    [--wiring NAME] [--upside-down] [--web [--port PORT]] [--for SECONDS [--expect FILE]]
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_OPTIONS_H_
#define _SIMULATOR_OPTIONS_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <string>
#include "MaxMatrixStandardTypes.h"
#include "SimulatorPanel.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorOptions Parameter */
#define SIMULATOR_OPTIONS_DEFAULT_PORT                              8080u
#define SIMULATOR_OPTIONS_MAX_PORT                                  65535u


/******************************************************************************************************************************************************
 *  CLASS  SimulatorOptions
 *****************************************************************************************************************************************************/
/* Only reads the command line; whether the wiring exists is the panel's to say, which knows the names. */
class SimulatorOptions
{
  private:
    std::string WiringName;
    bool UpsideDown;
    bool Web;
    uint16_t Port;
    uint32_t RunFor;
    const char* ExpectFile;

    stdReturnType parseOption(int, char**, int*);
    static stdReturnType convertToPort(const char*, uint16_t*);

  public:
    SimulatorOptions();

    // get methods
    const std::string& getWiringName() const { return WiringName; }
    bool isUpsideDown() const { return UpsideDown; }
    bool isWeb() const { return Web; }
    uint16_t getPort() const { return Port; }
    uint32_t getRunFor() const { return RunFor; }
    const char* getExpectFile() const { return ExpectFile; }

    // methods
    stdReturnType parse(int, char**);
    static void printUsage(const char*);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
