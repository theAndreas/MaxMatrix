// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Stand-in for avr-libc's access to flash, see Arduino.h. The host has no
 * separate flash: PROGMEM is nothing, and a read from flash is a read. */
#ifndef MAXMATRIX_TEST_MOCK_AVR_PGMSPACE_H_
#define MAXMATRIX_TEST_MOCK_AVR_PGMSPACE_H_

#include <stdint.h>
#include <string.h>

/* The names of avr-libc, which the library calls by them. */
// NOLINTBEGIN(readability-identifier-naming)
#define PROGMEM
#define memcpy_P memcpy
#define PSTR(text) (text)
#define strcmp_P strcmp
/* pgm_read_byte() reads the byte at address, as avr-libc reads it from flash,
 * taking the address as it is given, as pgm_read_ptr() does */
template <typename AddressType>
inline uint8_t pgm_read_byte(AddressType const* address) {
  return *reinterpret_cast<uint8_t const*>(address);
}
/* pgm_read_ptr() reads the bytes of the pointer at address, as avr-libc reads
 * them from flash, and takes the address as it is given, as avr-libc's macro
 * does. A macro that cast the stored pointer to void* made the library's cast
 * back to its own type one through void, and a parameter of const void* a
 * conversion of the address: clang-tidy reports both in the library, though
 * neither is there on the board. */
template <typename AddressType>
inline void* pgm_read_ptr(AddressType const* address) {
  void* value;
  memcpy(&value, address, sizeof(value));
  return value;
}
// NOLINTEND(readability-identifier-naming)

#endif  // MAXMATRIX_TEST_MOCK_AVR_PGMSPACE_H_
