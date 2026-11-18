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
 * on a pin that is no output, an SPI byte before SPI.begin(), a frame that
 * ends within a byte, and a port written with the interrupts on or written
 * that is none of the board's, as an error. Every bit and every rising chip
 * select also go to the MAX7219 model in Max7219.h, mock::chain. */
#ifndef MAXMATRIX_TEST_MOCK_ARDUINO_H_
#define MAXMATRIX_TEST_MOCK_ARDUINO_H_

#include <avr/interrupt.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "Max7219.h"
#include "binary.h"

/* The names of the Arduino core, which the library calls by them; those of
 * avr-libc are in avr/, which the core includes as this does. */
// NOLINTBEGIN(readability-identifier-naming)
typedef uint8_t byte;

/* F() has the type it has in the Arduino core, so that a function returning a
 * text in flash builds as on the board */
class __FlashStringHelper;  // NOLINT(bugprone-reserved-identifier): the core's
                            // name
#define F(text) (reinterpret_cast<__FlashStringHelper const*>(PSTR(text)))

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define LSBFIRST 0
#define MSBFIRST 1
/* the Uno's, of the pins_arduino.h the core includes */
#define NUM_DIGITAL_PINS 20
#define LED_BUILTIN 13

#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
// The core's bitSet() and bitClear() are ((value) |= (1UL << (bit))) and
// ((value) &= ~(1UL << (bit))), which narrow an unsigned long into the byte
// they change a bit of: avr-gcc warns of none, -Wconversion here of each, of
// bitSet() in the sanitize build. A system header does not keep a macro's
// warning from where it is expanded, so they are functions.
#define bitSet(value, bit) mock::BitSet((value), (bit))
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
// Without a virtual destructor, as the core's Print, which a sketch's own
// derives from. NOLINTNEXTLINE(clang-diagnostic-non-virtual-dtor)
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
// NOLINTNEXTLINE(clang-diagnostic-non-virtual-dtor): as Print
class HardwareSerial : public Print {
 public:
  void begin(unsigned long baud);
  int available();
  int read();
  size_t write(uint8_t c) override;
};

extern HardwareSerial Serial;

namespace mock {

/* bitSet() of the core, without its narrowing */
template <typename Integer>
Integer& BitSet(Integer& value, uint8_t bit) {
  return value = static_cast<Integer>(value | (1UL << bit));
}

/* bitClear() of the core, without its narrowing */
template <typename Integer>
Integer& BitClear(Integer& value, uint8_t bit) {
  return value = static_cast<Integer>(value & ~(1UL << bit));
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
 * 8 of port pin / 8 + 1 here, so that the pins 10, 11 and 13 of the examples
 * share a port, as they share PORTB on the Uno, and port 0 is none, the core's
 * NOT_A_PORT. */
uint8_t digitalPinToPort(uint8_t pin);
uint8_t digitalPinToBitMask(uint8_t pin);
mock::PortRegister* portOutputRegister(uint8_t port);

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
extern std::vector<uint8_t> frame;
extern std::vector<uint8_t> last_frame;
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
