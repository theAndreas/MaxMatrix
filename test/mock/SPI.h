/* Stand-in for the Arduino SPI library, see Arduino.h. */
#ifndef _MOCK_SPI_H_
#define _MOCK_SPI_H_

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
    uint8_t transfer(uint8_t Value);
};

extern SPIClass SPI;

#endif
