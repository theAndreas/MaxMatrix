// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The report of an error, with MAXMATRIX_DEV_ERROR_DETECT on.
 *
 * The default of MaxMatrixReportError(), which prints on Serial, in a file of its own. A sketch
 * that defines its own report would otherwise pay for Serial all the same, 175 bytes of RAM on the
 * Uno: the reference to Serial in the default pulls HardwareSerial0.o out of the core, even though
 * the default is left out, and serialEventRun() in main() keeps Serial alive from then on. So the
 * library is linked as an archive (dot_a_linkage in library.properties), and this file is only
 * taken when nothing else defines the report or a sketch calls MaxMatrixPrintError().
 */

#include "MaxMatrixError.h"

#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/*! \brief          takes the report of a function that failed, while MAXMATRIX_DEV_ERROR_DETECT is on
 *  \details        this default prints the report on Serial. It is weak, so that a sketch whose Serial carries
 *                  something else, or which wants to halt or blink instead, defines its own and this one is left out.
 *
 *  \param[in]      api         the name of the function of MaxMatrix that failed, in flash
 *  \param[in]      error       why it failed
 *  \pre            the sketch has called Serial.begin()
 */
__attribute__((weak)) void MaxMatrixReportError(__FlashStringHelper const* api, MaxMatrixError error)
{
    MaxMatrixPrintError(api, error);
}

/*! \brief          prints a report on Serial
 *  \details        this function prints one line, "MaxMatrix SetText: unknown char". It is apart from the default
 *                  report, so that a sketch with a report of its own can still print, and so that the tests, which
 *                  replace the report, can test the printing.
 *
 *  \param[in]      api         the name of the function of MaxMatrix that failed, in flash
 *  \param[in]      error       why it failed
 *  \pre            the sketch has called Serial.begin()
 */
void MaxMatrixPrintError(__FlashStringHelper const* api, MaxMatrixError error)
{
    Serial.print(F("MaxMatrix "));
    Serial.print(api);
    Serial.print(F(": "));
    Serial.println(MaxMatrixStatus(error).GetErrorText());
}
#endif
