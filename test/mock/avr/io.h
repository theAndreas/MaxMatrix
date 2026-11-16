// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Stand-in for avr-libc's registers, see Arduino.h: the status register alone,
 * which the library saves and restores around a write to a port. */
#ifndef MAXMATRIX_TEST_MOCK_AVR_IO_H_
#define MAXMATRIX_TEST_MOCK_AVR_IO_H_

#include <stdint.h>

/* avr-libc's name, which the library uses. */
extern uint8_t SREG;  // NOLINT(readability-identifier-naming)

#endif  // MAXMATRIX_TEST_MOCK_AVR_IO_H_
