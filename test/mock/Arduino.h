// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Stand-in for the Arduino core, so the library can be compiled and tested with
 * the host compiler.
 *
 * Only what MaxMatrix uses is here. The pins are followed as the chain sees
 * them: a falling chip select starts a frame and a rising one latches it, and a
 * rising clock shifts the level of data in into the chain, as SimulatorAvr does
 * for the real binary. The mock records what the library sends within a frame
 * and counts every bit or SPI byte outside one, outside an SPI transaction, or
 * on a pin that is no output, a frame that ends within a byte, and a port
 * written with the interrupts on, as an error. Every bit and every rising chip
 * select also go to the MAX7219 model in Max7219.h, mock::chain. */
#ifndef MAXMATRIX_TEST_MOCK_ARDUINO_H_
#define MAXMATRIX_TEST_MOCK_ARDUINO_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <string>
#include <vector>

#include "Max7219.h"
#include "binary.h"

/* The names of the Arduino core and avr-libc, which the library calls by them.
 */
// NOLINTBEGIN(readability-identifier-naming)
typedef uint8_t byte;

#define PROGMEM
#define memcpy_P memcpy
#define PSTR(text) (text)
#define strcmp_P strcmp
/* F() has the type it has in the Arduino core, so that a function returning a
 * text in flash builds as on the board */
class __FlashStringHelper;
#define F(text) (reinterpret_cast<__FlashStringHelper const*>(PSTR(text)))
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

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define LSBFIRST 0
#define MSBFIRST 1

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
// The core's bitClear() is ((value) &= ~(1UL << (bit))), which narrows an
// unsigned long into the byte it clears a bit of: avr-gcc warns of none,
// -Wconversion here of each. So it is a function.
#define bitClear(value, bit) mock::BitClear((value), (bit))
#define bitWrite(value, bit, bitvalue) \
  ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))

void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t value);
// uint32_t rather than the core's unsigned long, which is 32 bits on the Uno
// and 64 on the host: a sketch keeps them in a uint32_t, which would narrow on
// the host alone, and they overflow as the Uno's do, micros() after 71 minutes,
// millis() after 49 days.
uint32_t millis();
uint32_t micros();
void delay(unsigned long milliseconds);

/* Print as far as the library and the examples use it: what prints a char is
 * the class below, the rest is built on it, as in the Arduino core; a line ends
 * in \r\n, as the core prints it */
class Print {
 public:
  virtual size_t write(uint8_t c) = 0;
  size_t write(char const* text);
  size_t print(char const* text);
  size_t println(char const* text);
  size_t print(__FlashStringHelper const* text);
  size_t println(__FlashStringHelper const* text);
  size_t println();
};

/* Serial as far as the examples use it, declared with the types of the Arduino
 * core: it reads what a test or the simulator has put into mock::serial_input
 * and appends what the sketch prints to mock::serial_output */
class HardwareSerial : public Print {
 public:
  void begin(unsigned long baud);
  int available();
  int read();
  size_t write(uint8_t c) override;
};

extern HardwareSerial Serial;

namespace mock {

/* bitClear() of the core, without its narrowing */
template <typename T>
T& BitClear(T& value, uint8_t bit) {
  return value = static_cast<T>(value & ~(1UL << bit));
}

/* A port register as the library writes it, by |= and &= with the mask of a
 * pin: an object rather than a byte, so that every bit it changes reaches its
 * pin as digitalWrite() would, with the edges the chain sees. */
class PortRegister {
 public:
  uint8_t port;

  PortRegister& operator|=(uint8_t mask);
  PortRegister& operator&=(uint8_t mask);
};
}  // namespace mock

/* The ports of the AVR core, by which the library bit bangs: a pin is bit pin %
 * 8 of port pin / 8 here, so that the pins 10, 11 and 13 of the examples share
 * a port, as they share PORTB on the Uno. */
uint8_t digitalPinToPort(uint8_t pin);
uint8_t digitalPinToBitMask(uint8_t pin);
mock::PortRegister* portOutputRegister(uint8_t port);

/* the status register and cli(), which the library holds the interrupts off
 * with while it writes a port: cli() clears the I bit, 0x80, which the library
 * puts back by restoring SREG */
extern uint8_t SREG;
inline void cli() { SREG &= 0x7F; }
// NOLINTEND(readability-identifier-naming)

namespace mock {
uint8_t const kNumberOfPins = 64;

extern uint8_t pin_mode[kNumberOfPins];
extern uint8_t pin_level[kNumberOfPins];
extern uint8_t data_in_pin;
extern uint8_t chip_select_pin;
extern uint8_t clock_pin;
extern bool transaction_open;
extern int32_t errors;
extern int32_t frames;
extern std::vector<byte> frame;
extern std::vector<byte> last_frame;
extern Max7219Chain chain;
/* the bytes Serial has yet to read, and what the sketch has printed */
extern std::string serial_input;
extern std::string serial_output;
/* called after every latched frame and every millisecond delay() waits, so that
 * the simulator can draw while a sketch blocks in delay(); none in the tests */
extern void (*tick)();

/* every pin an input and low, the chain at its pins with number_of_chips chips,
 * every count at 0 */
void Reset(uint8_t data_in, uint8_t chip_select, uint8_t clock,
           uint8_t number_of_chips);
/* a pin goes to level, by digitalWrite() or a port register */
void SetPin(uint8_t pin, uint8_t level);
/* holds millis() at Milliseconds until the next Reset(); delay() then moves it
 * on without waiting */
void SetMillis(uint32_t milliseconds);
void Transfer(uint8_t value);
bool UsesSpi();
}  // namespace mock

#endif  // MAXMATRIX_TEST_MOCK_ARDUINO_H_
