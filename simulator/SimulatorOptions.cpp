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
/**     \file       SimulatorOptions.cpp
 *      \brief      The command line of the simulator
 *
 *      \details    Parsed once at the start; an option that is unknown, lacks its value or has a value of zero fails it.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_OPTIONS_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "SimulatorOptions.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorOptions
******************************************************************************************************************************************************/
/*! \brief          SimulatorOptions constructor
 *  \details        Instantiation of the options a simulator has without any on its command line: the default wiring,
 *                  mounted upright, in the terminal, until Ctrl+C
 *
 *  \return         -
 *****************************************************************************************************************************************************/
SimulatorOptions::SimulatorOptions()
{
    WiringName = SIMULATOR_PANEL_DEFAULT_WIRING;
    UpsideDown = false;
    Web = false;
    Port = SIMULATOR_OPTIONS_DEFAULT_PORT;
    RunFor = 0;
    ExpectFile = nullptr;
} /* SimulatorOptions */


/******************************************************************************************************************************************************
  parse()
******************************************************************************************************************************************************/
/*! \brief          reads the command line
 *  \details        this function reads one option after the other; --expect only makes sense with --for, which ends the
 *                  run at a time the expected panel was taken at
 *
 *  \param[in]      argc            the number of arguments, the program's name included
 *  \param[in]      argv            the arguments
 *  \return         E_OK
 *                  E_NOT_OK        the command line is wrong; printUsage() says how it should be
 *****************************************************************************************************************************************************/
stdReturnType SimulatorOptions::parse(int argc, char** argv)
{
    for(int i = 1; i < argc; i++) {
        if(E_OK != parseOption(argc, argv, &i)) return E_NOT_OK;
    }
    return (ExpectFile != nullptr && RunFor == 0) ? E_NOT_OK : E_OK;
} /* parse */


/******************************************************************************************************************************************************
  printUsage()
******************************************************************************************************************************************************/
/*! \brief          says how the command line should be
 *  \details        this function writes to stderr, beside whatever went wrong
 *
 *  \param[in]      Program         the program's name, argv[0]
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorOptions::printUsage(const char* Program)
{
    fprintf(stderr, "usage: %s [--wiring NAME] [--upside-down] [--web [--port PORT]] [--for SECONDS [--expect FILE]]\n", Program);
    fprintf(stderr, "  NAME: FC16 (default), GENERIC, PAROLA, ICSTATION or DR0CR0RR0 ... DR1CR1RR1\n");
} /* printUsage */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  parseOption()
******************************************************************************************************************************************************/
/*! \brief          reads one option
 *  \details        this function also reads the option's value, if it has one, and moves the index past it
 *
 *  \param[in]      argc            the number of arguments
 *  \param[in]      argv            the arguments
 *  \param[in,out]  Index           the option's index; on return the index of its value, if it has one
 *  \return         E_OK
 *                  E_NOT_OK        the option is unknown, lacks its value or has a value it cannot take
 *****************************************************************************************************************************************************/
stdReturnType SimulatorOptions::parseOption(int argc, char** argv, int* Index)
{
    const std::string Option = argv[*Index];
    const bool HasValue = *Index + 1 < argc;

    if(Option == "--upside-down") {
        UpsideDown = true;
    } else if(Option == "--web") {
        Web = true;
    } else if(Option == "--wiring" && HasValue) {
        WiringName = argv[++*Index];
    } else if(Option == "--port" && HasValue) {
        if(E_OK != convertToPort(argv[++*Index], &Port)) return E_NOT_OK;
    } else if(Option == "--for" && HasValue) {
        RunFor = static_cast<unsigned long>(atof(argv[++*Index]) * 1000);
        if(RunFor == 0) return E_NOT_OK;
    } else if(Option == "--expect" && HasValue) {
        ExpectFile = argv[++*Index];
    } else return E_NOT_OK;
    return E_OK;
} /* parseOption */


/******************************************************************************************************************************************************
  convertToPort()
******************************************************************************************************************************************************/
/*! \brief          converts the value of --port
 *  \details        this function takes a decimal number from 1 to 65535 and nothing else. atoi() and a cast, as before,
 *                  turned 70000 into port 4464 and "80x" into port 80, so the simulator listened somewhere else than
 *                  asked without a word.
 *
 *  \param[in]      Text            the value as given on the command line
 *  \param[out]     Port            the port
 *  \return         E_OK
 *                  E_NOT_OK        the value is no number, or out of range; the port is left as it was
 *****************************************************************************************************************************************************/
stdReturnType SimulatorOptions::convertToPort(const char* Text, unsigned short* Port)
{
    char* End;
    const unsigned long Value = strtoul(Text, &End, 10);

    /* strtoul() takes a sign and wraps "-1" round to ULONG_MAX, which the range check then refuses */
    if(End == Text || *End != '\0' || Value == 0 || Value > SIMULATOR_OPTIONS_MAX_PORT) return E_NOT_OK;
    *Port = static_cast<unsigned short>(Value);
    return E_OK;
} /* convertToPort */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
