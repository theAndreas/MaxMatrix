// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Stand-in for avr-libc's interrupts, see Arduino.h: cli(), which the library
 * holds the interrupts off with while it writes a port. It clears the I bit of
 * SREG, 0x80, which the library puts back by restoring SREG. */
#ifndef MAXMATRIX_TEST_MOCK_AVR_INTERRUPT_H_
#define MAXMATRIX_TEST_MOCK_AVR_INTERRUPT_H_

#include <avr/io.h>

/* avr-libc's name, which the library calls. */
inline void cli() { SREG &= 0x7FU; }  // NOLINT(readability-identifier-naming)

#endif  // MAXMATRIX_TEST_MOCK_AVR_INTERRUPT_H_
