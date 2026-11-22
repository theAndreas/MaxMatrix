// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Stand-in for avr-libc's critical sections, see Arduino.h: ATOMIC_BLOCK with
 * ATOMIC_RESTORESTATE, the one kind the library takes (AVR-2). Built as
 * avr-libc builds it, a for loop that runs once with the interrupts off and a
 * copy of SREG that its cleanup puts back however the block is left, so that
 * the mock's SREG goes the way the board's does. */
#ifndef MAXMATRIX_TEST_MOCK_UTIL_ATOMIC_H_
#define MAXMATRIX_TEST_MOCK_UTIL_ATOMIC_H_

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdint.h>

/* avr-libc's __iCliRetVal() and __iRestore(), by names the host does not
 * reserve. */
inline uint8_t MockAtomicCli() {
  cli();
  return 1u;
}
inline void MockAtomicRestore(uint8_t const* status_register) {
  SREG = *status_register;
}

/* The names of avr-libc, which the library uses. */
// NOLINTBEGIN(readability-identifier-naming,cppcoreguidelines-macro-usage)
#define ATOMIC_BLOCK(type)                                                 \
  for (type, mock_atomic_to_do = MockAtomicCli(); mock_atomic_to_do != 0u; \
       mock_atomic_to_do = 0u)
#define ATOMIC_RESTORESTATE           \
  uint8_t mock_atomic_status_register \
      __attribute__((__cleanup__(MockAtomicRestore))) = SREG
// NOLINTEND(readability-identifier-naming,cppcoreguidelines-macro-usage)

#endif  // MAXMATRIX_TEST_MOCK_UTIL_ATOMIC_H_
