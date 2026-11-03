/* Stand-in for the Arduino core, so the library can be compiled and tested with the host compiler.
 *
 * Only what MaxMatrix uses is here. The pins are not simulated as hardware: the mock records what the
 * library sends between a falling and a rising chip select, which is the frame the MAX7219 chain latches,
 * and counts every transfer that happens outside such a frame or outside an SPI transaction as an error.
 * Every byte and every rising chip select also go to the MAX7219 model in Max7219.h, Mock::Chain. */
#ifndef MAXMATRIX_TEST_MOCK_ARDUINO_H_
#define MAXMATRIX_TEST_MOCK_ARDUINO_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>
#include "binary.h"
#include "Max7219.h"

/* The names of the Arduino core and avr-libc, which the library calls by them. */
// NOLINTBEGIN(readability-identifier-naming)
typedef uint8_t byte;

#define PROGMEM
#define memcpy_P memcpy
#define PSTR(text) (text)
#define strcmp_P strcmp
/* F() has the type it has in the Arduino core, so that a function returning a text in flash builds as on the board */
class __FlashStringHelper;
#define F(text) (reinterpret_cast<__FlashStringHelper const*>(PSTR(text)))
/* pgm_read_ptr() reads the bytes of the pointer at address, as avr-libc reads them from flash, and takes the address
 * as it is given, as avr-libc's macro does. A macro that cast the stored pointer to void* made the library's cast back
 * to its own type one through void, and a parameter of const void* a conversion of the address: clang-tidy reports
 * both in the library, though neither is there on the board. */
template <typename AddressType>
inline void* pgm_read_ptr(AddressType const* address)
{
    void* value;
    memcpy(&value, address, sizeof(value));
    return value;
}

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define LSBFIRST 0
#define MSBFIRST 1

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
void shiftOut(uint8_t data_pin, uint8_t clock_pin, uint8_t bit_order, uint8_t value);
unsigned long millis();
unsigned long micros();
void delay(unsigned long milliseconds);

/* Serial as far as the examples use it, declared with the types of the Arduino core: it reads what a test or the
 * simulator has put into Mock::SerialInput and appends what the sketch prints to Mock::SerialOutput */
class HardwareSerial {
  public:
    void begin(unsigned long baud);
    int available();
    int read();
    size_t print(char const* text);
    size_t println(char const* text);
    size_t print(__FlashStringHelper const* text);
    size_t println(__FlashStringHelper const* text);
};

extern HardwareSerial Serial;
// NOLINTEND(readability-identifier-naming)

namespace mock {
    uint8_t const kNumberOfPins = 64;

    extern uint8_t pin_mode[kNumberOfPins];
    extern uint8_t pin_level[kNumberOfPins];
    extern uint8_t chip_select_pin;
    extern bool transaction_open;
    extern int32_t errors;
    extern int32_t frames;
    extern std::vector<byte> frame;
    extern std::vector<byte> last_frame;
    extern Max7219Chain chain;
    /* the bytes Serial has yet to read, and what the sketch has printed */
    extern std::string serial_input;
    extern std::string serial_output;
    /* called after every latched frame and every millisecond delay() waits, so that the simulator can draw
     * while a sketch blocks in delay(); none in the tests */
    extern void (*tick)();

    void Reset(uint8_t chip_select_pin, uint8_t number_of_chips);
    /* holds millis() at Milliseconds until the next reset(); delay() then moves it on without waiting */
    void SetMillis(uint32_t milliseconds);
    void Transfer(uint8_t value);
    bool UsesSpi();
}

#endif
