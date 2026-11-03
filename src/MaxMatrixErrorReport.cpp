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
/**     \file       MaxMatrixErrorReport.cpp
 *      \brief      The report of an error, with MAXMATRIX_DEV_ERROR_DETECT on
 *
 *      \details    The default of maxMatrixReportError(), which prints on Serial, in a file of its own. A sketch that
 *                  defines its own report would otherwise pay for Serial all the same, 175 bytes of RAM on the Uno:
 *                  the reference to Serial in the default pulls HardwareSerial0.o out of the core, even though the
 *                  default is left out, and serialEventRun() in main() keeps Serial alive from then on. So the library
 *                  is linked as an archive (dot_a_linkage in library.properties), and this file is only taken when
 *                  nothing else defines the report or a sketch calls maxMatrixPrintError().
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_ERROR_REPORT_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixError.h"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

#if(MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/******************************************************************************************************************************************************
  maxMatrixReportError()
******************************************************************************************************************************************************/
/*! \brief          takes the report of a function that failed, while MAXMATRIX_DEV_ERROR_DETECT is on
 *  \details        this default prints the report on Serial. It is weak, so that a sketch whose Serial carries
 *                  something else, or which wants to halt or blink instead, defines its own and this one is left out.
 *
 *  \param[in]      Api         the name of the function of MaxMatrix that failed, in flash
 *  \param[in]      Error       why it failed
 *  \pre            the sketch has called Serial.begin()
 *****************************************************************************************************************************************************/
__attribute__((weak)) void maxMatrixReportError(__FlashStringHelper const* Api, MaxMatrixErrorType Error)
{
    maxMatrixPrintError(Api, Error);
} /* maxMatrixReportError */


/******************************************************************************************************************************************************
  maxMatrixPrintError()
******************************************************************************************************************************************************/
/*! \brief          prints a report on Serial
 *  \details        this function prints one line, "MaxMatrix setText: unknown char". It is apart from the default
 *                  report, so that a sketch with a report of its own can still print, and so that the tests, which
 *                  replace the report, can test the printing.
 *
 *  \param[in]      Api         the name of the function of MaxMatrix that failed, in flash
 *  \param[in]      Error       why it failed
 *  \pre            the sketch has called Serial.begin()
 *****************************************************************************************************************************************************/
void maxMatrixPrintError(__FlashStringHelper const* Api, MaxMatrixErrorType Error)
{
    Serial.print(F("MaxMatrix "));
    Serial.print(Api);
    Serial.print(F(": "));
    Serial.println(MaxMatrixStatus(Error).getErrorText());
} /* maxMatrixPrintError */
#endif


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
