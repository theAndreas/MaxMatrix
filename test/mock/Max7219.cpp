#include "Max7219.h"
#include <string.h>

const Max7219WiringType Max7219Wirings[8] = {
    { "DR0CR0RR0", false, false, false },
    { "DR0CR0RR1", false, false, true },
    { "DR0CR1RR0", false, true, false },
    { "DR0CR1RR1", false, true, true },
    { "DR1CR0RR0", true, false, false },
    { "DR1CR0RR1", true, false, true },
    { "DR1CR1RR0", true, true, false },
    { "DR1CR1RR1", true, true, true },
};

const Max7219WiringType MAX7219_WIRING_GENERIC = Max7219Wirings[2];
const Max7219WiringType MAX7219_WIRING_FC16 = Max7219Wirings[4];
const Max7219WiringType MAX7219_WIRING_PAROLA = Max7219Wirings[6];
const Max7219WiringType MAX7219_WIRING_ICSTATION = Max7219Wirings[7];

/* the datasheet's power-up state: control registers reset, which means shut down, one digit scanned, no decode and
 * minimum intensity; the digit registers are undefined there and zero here */
void Max7219::reset()
{
    memset(Digit, 0, sizeof(Digit));
    DecodeMode = 0;
    Intensity = 0;
    ScanLimit = 0;
    Shutdown = 0;
    DisplayTest = 0;
}

/* D15 to D12 are don't care, so only the low nibble of the address byte selects the register; 0x0D and 0x0E are no
 * register at all and change nothing, like the no-op */
void Max7219::latch(uint8_t Address, uint8_t Data)
{
    Address &= 0x0F;
    if(Address >= 0x01 && Address <= 0x08) Digit[Address - 1] = Data;
    else if(Address == 0x09) DecodeMode = Data;
    else if(Address == 0x0A) Intensity = Data & 0x0F;
    else if(Address == 0x0B) ScanLimit = Data & 0x07;
    else if(Address == 0x0C) Shutdown = Data & 0x01;
    else if(Address == 0x0F) DisplayTest = Data & 0x01;
}

/* lit segments of a digit, laid out as in no-decode mode: bit 7 is DP, bit 6 segment A down to bit 0 segment G. On
 * a matrix module a digit is one column (or row) and a segment one LED. Display test lights everything, even in
 * shutdown. */
uint8_t Max7219::segments(uint8_t DigitIndex) const
{
    /* Code B font, segments A to G in bits 6 to 0: 0-9, '-', 'E', 'H', 'L', 'P', blank */
    static const uint8_t CodeB[16] = {
        0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70, 0x7F, 0x7B, 0x01, 0x4F, 0x37, 0x0E, 0x67, 0x00
    };

    if(DisplayTest) return 0xFF;
    if(!Shutdown || DigitIndex > ScanLimit) return 0x00;
    if(DecodeMode & (1 << DigitIndex)) return (Digit[DigitIndex] & 0x80) | CodeB[Digit[DigitIndex] & 0x0F];
    return Digit[DigitIndex];
}

void Max7219Chain::reset(uint8_t NumberOfChips)
{
    Chips.assign(NumberOfChips, Max7219());
    ShiftRegister.assign(2 * NumberOfChips, 0);
}

/* The MAX7219 shifts on every rising clock edge, whatever chip select does - unlike the MAX7221, which needs it low.
 * The byte that falls out of one chip's register goes into the next, so a frame of one pair per chip leaves the
 * first pair sent in the chip at the far end. */
void Max7219Chain::shift(uint8_t Value)
{
    if(ShiftRegister.empty()) return;
    for(size_t i = ShiftRegister.size() - 1; i > 0; i--) ShiftRegister[i] = ShiftRegister[i - 1];
    ShiftRegister[0] = Value;
}

/* every chip latches whatever its register holds on the rising edge of chip select - after a frame that was too
 * short, that is part of the frame before */
void Max7219Chain::latch()
{
    for(size_t Chip = 0; Chip < Chips.size(); Chip++) Chips[Chip].latch(ShiftRegister[2 * Chip + 1], ShiftRegister[2 * Chip]);
}

/* whether the LED at X, Y of the panel is lit, X from the left across all modules and Y from the top. The modules
 * are chained from right to left, as MD_MAX72XX has them, so Chips[0] at DIN is the rightmost. A board mounted
 * upside down shows every LED at the opposite place: its chain starts on the left, each module turned around. */
bool Max7219Chain::isLit(const Max7219WiringType& Wiring, uint16_t X, uint8_t Y, bool UpsideDown) const
{
    if(UpsideDown) {
        X = Chips.size() * Max7219::NumberOfDigits - 1 - X;
        Y = Max7219::NumberOfDigits - 1 - Y;
    }

    const Max7219& Chip = Chips[Chips.size() - 1 - X / Max7219::NumberOfDigits];
    uint8_t Column = Max7219::NumberOfDigits - 1 - X % Max7219::NumberOfDigits;
    uint8_t Digit = Wiring.DigitsAsRows ? Y : Column;
    uint8_t Segment = Wiring.DigitsAsRows ? Column : Y;

    if(Wiring.ReversedRows) Digit = Max7219::NumberOfDigits - 1 - Digit;
    if(Wiring.ReversedColumns) Segment = Max7219::NumberOfDigits - 1 - Segment;
    return (Chip.segments(Digit) >> Segment) & 0x01;
}

/* the chip that drives the LEDs in column X of the panel, by the same placement as isLit() */
const Max7219& Max7219Chain::chipAt(uint16_t X, bool UpsideDown) const
{
    if(UpsideDown) X = Chips.size() * Max7219::NumberOfDigits - 1 - X;
    return Chips[Chips.size() - 1 - X / Max7219::NumberOfDigits];
}
