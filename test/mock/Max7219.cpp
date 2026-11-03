#include "Max7219.h"
#include <string.h>

Max7219Wiring const kMax7219Wirings[8] = {
    { "DR0CR0RR0", false, false, false },
    { "DR0CR0RR1", false, false, true },
    { "DR0CR1RR0", false, true, false },
    { "DR0CR1RR1", false, true, true },
    { "DR1CR0RR0", true, false, false },
    { "DR1CR0RR1", true, false, true },
    { "DR1CR1RR0", true, true, false },
    { "DR1CR1RR1", true, true, true },
};

Max7219Wiring const kMaX7219WiringGeneric = kMax7219Wirings[2];
Max7219Wiring const kMaX7219WiringFC16 = kMax7219Wirings[4];
Max7219Wiring const kMaX7219WiringParola = kMax7219Wirings[6];
Max7219Wiring const kMaX7219WiringIcstation = kMax7219Wirings[7];

/* the datasheet's power-up state: control registers reset, which means shut down, one digit scanned, no decode and
 * minimum intensity; the digit registers are undefined there and zero here */
void Max7219::Reset()
{
    memset(digit, 0, sizeof(digit));
    decode_mode = 0;
    intensity = 0;
    scan_limit = 0;
    shutdown = 0;
    display_test = 0;
}

/* D15 to D12 are don't care, so only the low nibble of the address byte selects the register; 0x0D and 0x0E are no
 * register at all and change nothing, like the no-op */
void Max7219::Latch(uint8_t address, uint8_t data)
{
    address &= 0x0F;
    if(address >= 0x01 && address <= 0x08) digit[address - 1] = data;
    else if(address == 0x09) decode_mode = data;
    else if(address == 0x0A) intensity = data & 0x0F;
    else if(address == 0x0B) scan_limit = data & 0x07;
    else if(address == 0x0C) shutdown = data & 0x01;
    else if(address == 0x0F) display_test = data & 0x01;
}

/* lit segments of a digit, laid out as in no-decode mode: bit 7 is DP, bit 6 segment A down to bit 0 segment G. On
 * a matrix module a digit is one column (or row) and a segment one LED. Display test lights everything, even in
 * shutdown. */
uint8_t Max7219::Segments(uint8_t digit_index) const
{
    /* Code B font, segments A to G in bits 6 to 0: 0-9, '-', 'E', 'H', 'L', 'P', blank */
    static uint8_t const kCodeB[16] = {
        0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70, 0x7F, 0x7B, 0x01, 0x4F, 0x37, 0x0E, 0x67, 0x00
    };

    if(display_test) return 0xFF;
    if(!shutdown || digit_index > scan_limit) return 0x00;
    if(decode_mode & (1 << digit_index)) return (digit[digit_index] & 0x80) | kCodeB[digit[digit_index] & 0x0F];
    return digit[digit_index];
}

void Max7219Chain::Reset(uint8_t number_of_chips)
{
    chips.assign(number_of_chips, Max7219());
    shift_register_.assign(2 * number_of_chips, 0);
}

/* a byte as the wire carries it, most significant bit first */
void Max7219Chain::Shift(uint8_t value)
{
    for(int8_t bit = 7; bit >= 0; bit--) ShiftBit((value >> bit) & 0x01);
}

/* The MAX7219 shifts one bit on every rising clock edge, whatever chip select does - unlike the MAX7221, which needs
 * it low. The bit that falls out of one chip's register goes into the next, so a frame of one pair per chip leaves
 * the first pair sent in the chip at the far end. Bit by bit rather than byte by byte, so that a frame that is not a
 * whole number of bytes, which bit banging can send, latches what the hardware would. */
void Max7219Chain::ShiftBit(bool value)
{
    if(shift_register_.empty()) return;
    for(size_t i = shift_register_.size() - 1; i > 0; i--) {
        shift_register_[i] = static_cast<uint8_t>((shift_register_[i] << 1) | (shift_register_[i - 1] >> 7));
    }
    shift_register_[0] = static_cast<uint8_t>((shift_register_[0] << 1) | (value ? 1 : 0));
}

/* every chip latches whatever its register holds on the rising edge of chip select - after a frame that was too
 * short, that is part of the frame before */
void Max7219Chain::Latch()
{
    for(size_t chip = 0; chip < chips.size(); chip++) chips[chip].Latch(shift_register_[2 * chip + 1], shift_register_[2 * chip]);
}

/* whether the LED at X, Y of the panel is lit, X from the left across all modules and Y from the top. The modules
 * are chained from right to left, as MD_MAX72XX has them, so Chips[0] at DIN is the rightmost. A board mounted
 * upside down shows every LED at the opposite place: its chain starts on the left, each module turned around. */
bool Max7219Chain::IsLit(Max7219Wiring const& wiring, uint16_t x, uint8_t y, bool upside_down) const
{
    if(upside_down) {
        x = chips.size() * Max7219::kNumberOfDigits - 1 - x;
        y = Max7219::kNumberOfDigits - 1 - y;
    }

    Max7219 const& chip = chips[chips.size() - 1 - x / Max7219::kNumberOfDigits];
    uint8_t column = Max7219::kNumberOfDigits - 1 - x % Max7219::kNumberOfDigits;
    uint8_t digit = wiring.digits_as_rows ? y : column;
    uint8_t segment = wiring.digits_as_rows ? column : y;

    if(wiring.reversed_rows) digit = Max7219::kNumberOfDigits - 1 - digit;
    if(wiring.reversed_columns) segment = Max7219::kNumberOfDigits - 1 - segment;
    return (chip.Segments(digit) >> segment) & 0x01;
}

/* the chip that drives the LEDs in column X of the panel, by the same placement as IsLit() */
Max7219 const& Max7219Chain::ChipAt(uint16_t x, bool upside_down) const
{
    if(upside_down) x = chips.size() * Max7219::kNumberOfDigits - 1 - x;
    return chips[chips.size() - 1 - x / Max7219::kNumberOfDigits];
}
