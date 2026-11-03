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
 *****************************************************************************************************************************************************/
SimulatorOptions::SimulatorOptions()
{
    WiringName = SIMULATOR_PANEL_DEFAULT_WIRING;
    UpsideDown = false;
    Web = false;
    Port = SIMULATOR_OPTIONS_DEFAULT_PORT;
    RunFor = 0;
    ExpectFile = nullptr;
    ElfFile = nullptr;
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
 *  \return         true
 *                  false           the command line is wrong; printUsage() says how it should be
 *****************************************************************************************************************************************************/
bool SimulatorOptions::parse(int argc, char** argv)
{
    for(int i = 1; i < argc; i++) {
        if(!parseOption(argc, argv, &i)) return false;
    }
    return ExpectFile == nullptr || RunFor != 0;
} /* parse */


/******************************************************************************************************************************************************
  printUsage()
******************************************************************************************************************************************************/
/*! \brief          says how the command line should be
 *  \details        this function writes to stderr, beside whatever went wrong
 *
 *  \param[in]      Program         the program's name, argv[0]
 *****************************************************************************************************************************************************/
void SimulatorOptions::printUsage(char const* Program)
{
    fprintf(stderr, "usage: %s [--elf FILE] [--wiring NAME] [--upside-down] [--serial LINE]... [--web [--port PORT]]\n", Program);
    fprintf(stderr, "         [--for SECONDS [--expect FILE]]\n");
    fprintf(stderr, "  FILE of --elf: the example built for the Uno; SimulatorAvr only, which needs it\n");
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
 *  \return         true
 *                  false           the option is unknown, lacks its value or has a value it cannot take
 *****************************************************************************************************************************************************/
bool SimulatorOptions::parseOption(int argc, char** argv, int* Index)
{
    std::string const Option = argv[*Index];
    bool const HasValue = *Index + 1 < argc;

    if(Option == "--upside-down") {
        UpsideDown = true;
    } else if(Option == "--web") {
        Web = true;
    } else if(Option == "--wiring" && HasValue) {
        WiringName = argv[++*Index];
    } else if(Option == "--port" && HasValue) {
        if(!convertToPort(argv[++*Index], &Port)) return false;
    } else if(Option == "--for" && HasValue) {
        if(!convertToRunFor(argv[++*Index], &RunFor)) return false;
    } else if(Option == "--expect" && HasValue) {
        ExpectFile = argv[++*Index];
    } else if(Option == "--elf" && HasValue) {
        ElfFile = argv[++*Index];
    } else if(Option == "--serial" && HasValue) {
        SerialInput += std::string(argv[++*Index]) + "\n";
    } else return false;
    return true;
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
 *  \return         true
 *                  false           the value is no number, or out of range; the port is left as it was
 *****************************************************************************************************************************************************/
bool SimulatorOptions::convertToPort(char const* Text, uint16_t* Port)
{
    char* End;
    unsigned long const Value = strtoul(Text, &End, 10);

    /* strtoul() takes a sign and wraps "-1" round to ULONG_MAX, which the range check then refuses */
    if(End == Text || *End != '\0' || Value == 0 || Value > SIMULATOR_OPTIONS_MAX_PORT) return false;
    *Port = static_cast<uint16_t>(Value);
    return true;
} /* convertToPort */


/******************************************************************************************************************************************************
  convertToRunFor()
******************************************************************************************************************************************************/
/*! \brief          converts the value of --for
 *  \details        this function takes the seconds as a decimal number, fractions included, and nothing else. atof(), as
 *                  before, took "2x" as two seconds, and a negative or a huge number reached a cast to uint32_t that is
 *                  undefined for it.
 *
 *  \param[in]      Text            the value as given on the command line
 *  \param[out]     RunFor          the milliseconds
 *  \return         true
 *                  false           the value is no number, or not a single millisecond, or more than the milliseconds fit;
 *                                  the time is left as it was
 *****************************************************************************************************************************************************/
bool SimulatorOptions::convertToRunFor(char const* Text, uint32_t* RunFor)
{
    char* End;
    double const Milliseconds = strtod(Text, &End) * 1000;

    /* written so that NaN, which compares false with everything, is refused as well */
    if(End == Text || *End != '\0' || !(Milliseconds >= 1 && Milliseconds <= SIMULATOR_OPTIONS_MAX_RUN_FOR)) return false;
    *RunFor = static_cast<uint32_t>(Milliseconds);
    return true;
} /* convertToRunFor */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
