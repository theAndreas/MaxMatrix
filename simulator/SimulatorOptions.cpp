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
    wiring_name_ = SIMULATOR_PANEL_DEFAULT_WIRING;
    upside_down_ = false;
    web_ = false;
    port_ = SIMULATOR_OPTIONS_DEFAULT_PORT;
    run_for_ = 0;
    expect_file_ = nullptr;
    elf_file_ = nullptr;
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
bool SimulatorOptions::Parse(int argc, char** argv)
{
    for(int i = 1; i < argc; i++) {
        if(!ParseOption(argc, argv, &i)) return false;
    }
    return expect_file_ == nullptr || run_for_ != 0;
} /* parse */


/******************************************************************************************************************************************************
  printUsage()
******************************************************************************************************************************************************/
/*! \brief          says how the command line should be
 *  \details        this function writes to stderr, beside whatever went wrong
 *
 *  \param[in]      Program         the program's name, argv[0]
 *****************************************************************************************************************************************************/
void SimulatorOptions::PrintUsage(char const* program)
{
    fprintf(stderr, "usage: %s [--elf FILE] [--wiring NAME] [--upside-down] [--serial LINE]... [--web [--port PORT]]\n", program);
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
bool SimulatorOptions::ParseOption(int argc, char** argv, int* index)
{
    std::string const option = argv[*index];
    bool const has_value = *index + 1 < argc;

    if(option == "--upside-down") {
        upside_down_ = true;
    } else if(option == "--web") {
        web_ = true;
    } else if(option == "--wiring" && has_value) {
        wiring_name_ = argv[++*index];
    } else if(option == "--port" && has_value) {
        if(!ConvertToPort(argv[++*index], &port_)) return false;
    } else if(option == "--for" && has_value) {
        if(!ConvertToRunFor(argv[++*index], &run_for_)) return false;
    } else if(option == "--expect" && has_value) {
        expect_file_ = argv[++*index];
    } else if(option == "--elf" && has_value) {
        elf_file_ = argv[++*index];
    } else if(option == "--serial" && has_value) {
        serial_input_ += std::string(argv[++*index]) + "\n";
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
bool SimulatorOptions::ConvertToPort(char const* text, uint16_t* port)
{
    char* end;
    unsigned long const value = strtoul(text, &end, 10);

    /* strtoul() takes a sign and wraps "-1" round to ULONG_MAX, which the range check then refuses */
    if(end == text || *end != '\0' || value == 0 || value > SIMULATOR_OPTIONS_MAX_PORT) return false;
    *port = static_cast<uint16_t>(value);
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
bool SimulatorOptions::ConvertToRunFor(char const* text, uint32_t* run_for)
{
    char* end;
    double const milliseconds = strtod(text, &end) * 1000;

    /* written so that NaN, which compares false with everything, is refused as well */
    if(end == text || *end != '\0' || !(milliseconds >= 1 && milliseconds <= SIMULATOR_OPTIONS_MAX_RUN_FOR)) return false;
    *run_for = static_cast<uint32_t>(milliseconds);
    return true;
} /* convertToRunFor */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
