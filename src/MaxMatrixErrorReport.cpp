// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The report of an error, with MAXMATRIX_DEV_ERROR_DETECT on.
 *
 * The default of ReportError(), which prints on Serial, in a file of
 * its own. A sketch that defines its own report would otherwise pay for Serial
 * all the same, 175 bytes of RAM on the Uno: the reference to Serial in the
 * default pulls HardwareSerial0.o out of the core, even though the default is
 * left out, and serialEventRun() in main() keeps Serial alive from then on. So
 * the library is linked as an archive (dot_a_linkage in library.properties),
 * and this file is only taken when nothing else defines the report or a sketch
 * calls PrintError().
 */

#include "MaxMatrixError.h"

namespace max_matrix {

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
__attribute__((weak)) void ReportError(__FlashStringHelper const* api,
                                       Error error) {
  PrintError(api, error);
}

// Apart from the default report, so that a sketch with a report of its own can
// still print, and so that the tests, which replace the report, can test the
// printing.
void PrintError(__FlashStringHelper const* api, Error error) {
  Serial.print(F("MaxMatrix "));
  Serial.print(api);
  Serial.print(F(": "));
  Serial.println(Status(error).GetErrorText());
}
#endif

}  // namespace max_matrix
