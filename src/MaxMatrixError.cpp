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
/**     \file       MaxMatrixError.cpp
 *      \brief      Why a call did not do what it was asked
 *
 *      \details    The text of every error, for a sketch that prints it.
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_ERROR_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixError.h"


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
/* The texts and the table of them lie in flash. A switch that returned F("…") for each error was turned by avr-gcc into
 * a table of pointers in .rodata, which the Uno copies into RAM: 14 bytes for seven errors. */
static char const ErrorTextOk[] PROGMEM = "ok";
static char const ErrorTextOutOfRange[] PROGMEM = "out of range";
static char const ErrorTextUnknownChar[] PROGMEM = "unknown char";
static char const ErrorTextNoText[] PROGMEM = "no text";
static char const ErrorTextNotANumber[] PROGMEM = "not a number";
static char const ErrorTextUnknownCommand[] PROGMEM = "unknown command";
static char const ErrorTextLineTooLong[] PROGMEM = "line too long";
static char const ErrorTextUnknownError[] PROGMEM = "unknown error";

/* in the order of MaxMatrixErrorType */
static char const* const ErrorTexts[] PROGMEM = {
    ErrorTextOk, ErrorTextOutOfRange, ErrorTextUnknownChar, ErrorTextNoText, ErrorTextNotANumber, ErrorTextUnknownCommand,
    ErrorTextLineTooLong
};

static_assert(sizeof(ErrorTexts) / sizeof(ErrorTexts[0]) == MAXMATRIX_E_LINE_TOO_LONG + 1, "every error needs a text");


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  getErrorText()
******************************************************************************************************************************************************/
/*! \brief          the error in words
 *  \details        this function returns a few words a sketch can print with Serial.print(), "out of range" for
 *                  MAXMATRIX_E_OUT_OF_RANGE. Serial.print() reads a __FlashStringHelper from flash, so the text needs no
 *                  RAM. A sketch that never calls this function pays nothing, as the linker drops it with its texts.
 *
 *  \return         the text, in flash; "unknown error" for a byte cast to the enum that is none of its values
 *****************************************************************************************************************************************************/
__FlashStringHelper const* MaxMatrixStatus::getErrorText() const
{
    char const* Text = ErrorTextUnknownError;

    if(Error < sizeof(ErrorTexts) / sizeof(ErrorTexts[0])) Text = static_cast<char const*>(pgm_read_ptr(&ErrorTexts[Error]));
    return reinterpret_cast<__FlashStringHelper const*>(Text);
} /* getErrorText */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
