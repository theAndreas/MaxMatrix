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
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixError.h"


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
/* The texts and the table of them lie in flash. A switch that returned F("…") for each error was turned by avr-gcc into
 * a table of pointers in .rodata, which the Uno copies into RAM: 14 bytes for seven errors. */
static char const kErrorTextOk[] PROGMEM = "ok";
static char const kErrorTextOutOfRange[] PROGMEM = "out of range";
static char const kErrorTextUnknownChar[] PROGMEM = "unknown char";
static char const kErrorTextNoText[] PROGMEM = "no text";
static char const kErrorTextNotANumber[] PROGMEM = "not a number";
static char const kErrorTextUnknownCommand[] PROGMEM = "unknown command";
static char const kErrorTextLineTooLong[] PROGMEM = "line too long";
static char const kErrorTextUnknownError[] PROGMEM = "unknown error";

/* in the order of MaxMatrixError */
static char const* const kErrorTexts[] PROGMEM = {
    kErrorTextOk, kErrorTextOutOfRange, kErrorTextUnknownChar, kErrorTextNoText, kErrorTextNotANumber, kErrorTextUnknownCommand,
    kErrorTextLineTooLong
};

static_assert(sizeof(kErrorTexts) / sizeof(kErrorTexts[0]) == static_cast<uint8_t>(MaxMatrixError::kLineTooLong) + 1, "every error needs a text");


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  GetErrorText()
******************************************************************************************************************************************************/
/*! \brief          the error in words
 *  \details        this function returns a few words a sketch can print with Serial.print(), "out of range" for
 *                  MaxMatrixError::kOutOfRange. Serial.print() reads a __FlashStringHelper from flash, so the text needs no
 *                  RAM. A sketch that never calls this function pays nothing, as the linker drops it with its texts.
 *
 *  \return         the text, in flash; "unknown error" for a byte cast to the enum that is none of its values
 *****************************************************************************************************************************************************/
__FlashStringHelper const* MaxMatrixStatus::GetErrorText() const
{
    char const* text = kErrorTextUnknownError;
    uint8_t const index = static_cast<uint8_t>(error_);

    if(index < sizeof(kErrorTexts) / sizeof(kErrorTexts[0])) text = static_cast<char const*>(pgm_read_ptr(&kErrorTexts[index]));
    return reinterpret_cast<__FlashStringHelper const*>(text);
} /* GetErrorText */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
