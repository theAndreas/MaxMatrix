// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Why a call did not do what it was asked.
 *
 * The text of every error, for a sketch that prints it.
 */

#include "MaxMatrixError.h"

namespace max_matrix {

/*
 * The texts and the table of them lie in flash. A switch that returned F("…")
 * for each error was turned by avr-gcc into a table of pointers in .rodata,
 * which the Uno copies into RAM: 14 bytes for seven errors.
 */
static char const kErrorTextOk[] PROGMEM = "ok";
static char const kErrorTextOutOfRange[] PROGMEM = "out of range";
static char const kErrorTextUnknownChar[] PROGMEM = "unknown char";
static char const kErrorTextNoText[] PROGMEM = "no text";
static char const kErrorTextNotANumber[] PROGMEM = "not a number";
static char const kErrorTextUnknownCommand[] PROGMEM = "unknown command";
static char const kErrorTextLineTooLong[] PROGMEM = "line too long";
static char const kErrorTextNotAFont[] PROGMEM = "not a font";
static char const kErrorTextUnknownError[] PROGMEM = "unknown error";

/* in the order of Error */
static char const* const kErrorTexts[] PROGMEM = {
    kErrorTextOk,          kErrorTextOutOfRange, kErrorTextUnknownChar,
    kErrorTextNoText,      kErrorTextNotANumber, kErrorTextUnknownCommand,
    kErrorTextLineTooLong, kErrorTextNotAFont};

static_assert(sizeof(kErrorTexts) / sizeof(kErrorTexts[0]) ==
                  static_cast<uint8_t>(Error::kNotAFont) + 1,
              "every error needs a text");

__FlashStringHelper const* Status::GetErrorText() const {
  char const* text = kErrorTextUnknownError;
  uint8_t const index = static_cast<uint8_t>(error_);

  if (index < sizeof(kErrorTexts) / sizeof(kErrorTexts[0]))
    text = static_cast<char const*>(pgm_read_ptr(&kErrorTexts[index]));
  return reinterpret_cast<__FlashStringHelper const*>(text);
}

}  // namespace max_matrix
