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

class SPIClass {
 public:
  void begin() {}
  void beginTransaction(SPISettings);
  void endTransaction();
  uint8_t transfer(uint8_t value);
};

extern SPIClass SPI;

// NOLINTEND(readability-identifier-naming)
#endif
