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

/* How the LEDs of a module are wired to digits and segments, as the viewer sees them, in the three properties
 * MD_MAX72XX names its hardware types by, DR0CR0RR0_HW to DR1CR1RR1_HW, and with the same meaning, so that a module
 * MD_MAX72XX has tested is the same module here. As there, a module's column is counted from the right and its row
 * from the top:
 *   DigitsAsRows      a digit drives a row of LEDs and a segment bit a column; otherwise a digit drives a column
 *   ReversedColumns   the segment bit index is reversed, bit 7 - x for x
 *   ReversedRows      the digit index is reversed, digit 7 - x for x */
struct Max7219WiringType {
    const char* Name;
    bool DigitsAsRows;
    bool ReversedColumns;
    bool ReversedRows;
};

/* the four MD_MAX72XX has names for */
extern const Max7219WiringType MAX7219_WIRING_FC16;
extern const Max7219WiringType MAX7219_WIRING_GENERIC;
extern const Max7219WiringType MAX7219_WIRING_PAROLA;
extern const Max7219WiringType MAX7219_WIRING_ICSTATION;

/* all eight, DR0CR0RR0 to DR1CR1RR1 */
extern const Max7219WiringType Max7219Wirings[8];

class Max7219Chain {
  public:
    /* Chips[0] is the chip at DIN, next to the Arduino; a byte reaches the far end last */
    std::vector<Max7219> Chips;

    void reset(uint8_t NumberOfChips);
    void shift(uint8_t Value);
    void latch();
    bool isLit(const Max7219WiringType& Wiring, uint16_t X, uint8_t Y, bool UpsideDown = false) const;
    const Max7219& chipAt(uint16_t X, bool UpsideDown = false) const;

  private:
    /* two bytes per chip, ShiftRegister[2 * Chip] its data byte and ShiftRegister[2 * Chip + 1] its address byte */
    std::vector<uint8_t> ShiftRegister;
};

#endif
