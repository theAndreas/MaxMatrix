// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Configuration of the MaxMatrix library.
 *
 * The parameters every part of the library builds on: the size of the chain and
 * how it is driven.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIXCONFIG_H_
#define MAXMATRIX_SRC_MAXMATRIXCONFIG_H_

// Not Arduino.h: the control program on the host reads the sizes of a module
// too, without the core.
#include <stddef.h>
#include <stdint.h>

#include "MaxMatrixStandardTypes.h"

/*
 * The configuration. Each parameter can be given on the compiler command line
 * instead, which is how the tests build both the bit banging and the SPI
 * variant. The Arduino IDE does not pass a sketch's defines to a library, so
 * for a sketch they are changed here.
 */

/*!
 * \brief `STD_ON` sends through the hardware SPI, whose pins are fixed;
 *        `STD_OFF` bit bangs on any three pins. Any other value fails when
 *        compiling.
 */
#ifndef MAXMATRIX_USE_SPI
#define MAXMATRIX_USE_SPI STD_OFF
#endif

/*! \brief The SPI clock in Hz; the MAX7219 takes 10 MHz at most, and a value
 * beyond fails when compiling. */
#ifndef MAXMATRIX_SPI_CLOCK_SPEED
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a sketch sets it
#define MAXMATRIX_SPI_CLOCK_SPEED 10000000
#endif

/*! \brief The empty columns between two chars of a text, until
 * Matrix::SetSpaceBetweenChars(), 0 to 255; a value beyond fails when
 * compiling. */
#ifndef MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a sketch sets it
#define MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE 1
#endif

/*! \brief The intensity Matrix::Init() sets, 0 to 15; a value beyond fails
 * when compiling. */
#ifndef MAXMATRIX_INTENSITY_INIT_VALUE
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a sketch sets it
#define MAXMATRIX_INTENSITY_INIT_VALUE 5
#endif

/*! \brief The chars of a line Command takes; it costs twice as many
 * bytes of RAM. */
#ifndef MAXMATRIX_COMMAND_LINE_LENGTH
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a sketch sets it
#define MAXMATRIX_COMMAND_LINE_LENGTH 64
#endif

/*!
 * \brief The milliseconds per column of a shifting text, until
 *        Matrix::SetShiftInterval(); 0 for a step on every call, up to 65535; a
 *        value beyond fails when compiling.
 */
#ifndef MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE
// NOLINTNEXTLINE(cppcoreguidelines-macro-usage): T-6, a sketch sets it
#define MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE 50
#endif

// STD_ON reports every error to ReportError(), see MaxMatrixError.h.
// Not a block: the Doxyfile sets it, so that the report is in the reference,
// and the README's table has it.
#ifndef MAXMATRIX_DEV_ERROR_DETECT
#define MAXMATRIX_DEV_ERROR_DETECT STD_OFF
#endif

// E-2: #if takes any other value for STD_OFF, and the sketch would not get
// what it switched on.
static_assert((MAXMATRIX_USE_SPI == STD_ON) || (MAXMATRIX_USE_SPI == STD_OFF),
              "MAXMATRIX_USE_SPI is STD_ON or STD_OFF");
static_assert((MAXMATRIX_DEV_ERROR_DETECT == STD_ON) ||
                  (MAXMATRIX_DEV_ERROR_DETECT == STD_OFF),
              "MAXMATRIX_DEV_ERROR_DETECT is STD_ON or STD_OFF");

namespace max_matrix {

/*! \brief The columns of one module. */
constexpr uint8_t kColumnNumberOfModule{8};

/*! \brief The rows of one module, and of the matrix. */
constexpr uint8_t kRowNumberOfModule{8};

/*!
 * \brief The most modules a chain may have: a column is counted in a byte
 *        throughout, as an index into the buffer and in every loop over the
 *        columns.
 */
constexpr uint8_t kMaxNumberOfModules{31};

/*!
 * \brief The bytes of the buffer of a chain, one per column.
 *
 * \param[in] modules The modules of the chain.
 * \return The size of its buffer in bytes.
 */
constexpr size_t BufferSize(size_t modules) {
  return modules * kColumnNumberOfModule;
}

}  // namespace max_matrix

#endif  // MAXMATRIX_SRC_MAXMATRIXCONFIG_H_
