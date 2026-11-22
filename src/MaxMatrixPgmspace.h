// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief `PROGMEM` and `pgm_read_byte()` on every board, from the header its
 *        core has them in.
 *
 * The AVR has them in avr-libc's `<avr/pgmspace.h>`, which the cores of the
 * Uno R4 and the Pico copy; the ESP32's core has them in `<pgmspace.h>` alone.
 * Asked by `__has_include` rather than by the board, so that a core that
 * brings either needs no line here.
 */

#ifndef MAXMATRIX_SRC_MAXMATRIXPGMSPACE_H_
#define MAXMATRIX_SRC_MAXMATRIXPGMSPACE_H_

#if __has_include(<avr/pgmspace.h>)
#include <avr/pgmspace.h>  // IWYU pragma: export
#else
#include <pgmspace.h>  // IWYU pragma: export
#endif

#endif  // MAXMATRIX_SRC_MAXMATRIXPGMSPACE_H_
