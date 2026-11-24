// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Stand-in for the Arduino SPI library, see Arduino.h. */
#ifndef MAXMATRIX_TEST_MOCK_SPI_H_
#define MAXMATRIX_TEST_MOCK_SPI_H_

/* The names of the Arduino core, which the library calls by them. */
// NOLINTBEGIN(readability-identifier-naming)

#include "Arduino.h"

#define SPI_MODE0 0

class SPISettings {
 public:
  SPISettings(uint32_t, uint8_t, uint8_t) {}
};

/* a bus of its own per object, as on a board with more than one, so that a
 * test sees which one the library sends on */
class SPIClass {
 public:
  void begin();
  void beginTransaction(SPISettings);
  void endTransaction();
  uint8_t transfer(uint8_t value);

 private:
  /* begin() was called on this bus: before it, the Uno waits for ever for a
   * byte to go out, as the SPI is off */
  bool begun_{false};
};

/* the bus every core has, and a second, as the ESP32, the Pico and the Uno R4
 * have */
extern SPIClass SPI;
extern SPIClass SPI1;

// NOLINTEND(readability-identifier-naming)

namespace mock {
/* the bus the chain is wired to, SPI after Reset(): only a byte on it reaches
 * the chain, and a call on another one counts as an error */
extern SPIClass* chain_bus;
}  // namespace mock
#endif  // MAXMATRIX_TEST_MOCK_SPI_H_
