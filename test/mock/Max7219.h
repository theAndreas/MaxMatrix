/* Model of a chain of MAX7219, fed from the wire by the Arduino stand-in.
 *
 * Driven by the bytes the library shifts out and by the rising edge of chip select, not by MatrixBuffer: the buffer
 * only says what the library believes is on the LEDs, and a wrong module, register or frame length leaves it right
 * and the display wrong. Only the logic is modelled - no timing, nothing electrical, and no multiplexing: a digit
 * within the scan limit shows its register for as long as the chip is not shut down. */
#ifndef _MOCK_MAX7219_H_
#define _MOCK_MAX7219_H_

#include <stdint.h>
#include <vector>

class Max7219 {
  public:
    static const uint8_t NumberOfDigits = 8;

    uint8_t Digit[NumberOfDigits];
    uint8_t DecodeMode;
    uint8_t Intensity;
    uint8_t ScanLimit;
    uint8_t Shutdown;
    uint8_t DisplayTest;

    Max7219() { reset(); }
    void reset();
    void latch(uint8_t Address, uint8_t Data);
    uint8_t segments(uint8_t DigitIndex) const;
};

class Max7219Chain {
  public:
    /* Chips[0] is the chip at DIN, next to the Arduino; a byte reaches the far end last */
    std::vector<Max7219> Chips;

    void reset(uint8_t NumberOfChips);
    void shift(uint8_t Value);
    void latch();

  private:
    /* two bytes per chip, ShiftRegister[2 * Chip] its data byte and ShiftRegister[2 * Chip + 1] its address byte */
    std::vector<uint8_t> ShiftRegister;
};

#endif
