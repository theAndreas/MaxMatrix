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
    unsigned short Port;
    unsigned long RunFor;
    const char* ExpectFile;

    stdReturnType parseOption(int, char**, int*);

  public:
    SimulatorOptions();

    // get methods
    const std::string& getWiringName() const { return WiringName; }
    bool isUpsideDown() const { return UpsideDown; }
    bool isWeb() const { return Web; }
    unsigned short getPort() const { return Port; }
    unsigned long getRunFor() const { return RunFor; }
    const char* getExpectFile() const { return ExpectFile; }

    // methods
    stdReturnType parse(int, char**);
    static void printUsage(const char*);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
