/* Stand-in for the Arduino core, so the library can be compiled and tested with the host compiler.
 *
 * Only what MaxMatrix uses is here. The pins are not simulated as hardware: the mock records what the
 * library sends between a falling and a rising chip select, which is the frame the MAX7219 chain latches,
 * and counts every transfer that happens outside such a frame or outside an SPI transaction as an error.
 * Every byte and every rising chip select also go to the MAX7219 model in Max7219.h, Mock::Chain. */
#ifndef _MOCK_ARDUINO_H_
#define _MOCK_ARDUINO_H_

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <string>
#include <vector>
#include "binary.h"
#include "Max7219.h"

typedef uint8_t byte;

#define PROGMEM
#define memcpy_P memcpy
#define PSTR(Text) (Text)
#define strcmp_P strcmp
#define F(Text) (Text)

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

void pinMode(uint8_t Pin, uint8_t Mode);
void digitalWrite(uint8_t Pin, uint8_t Value);
void shiftOut(uint8_t DataPin, uint8_t ClockPin, uint8_t BitOrder, uint8_t Value);
unsigned long millis();
unsigned long micros();
void delay(unsigned long Milliseconds);

/* Serial as far as the examples use it, declared with the types of the Arduino core: it reads what a test or the
 * simulator has put into Mock::SerialInput and appends what the sketch prints to Mock::SerialOutput */
class HardwareSerial {
  public:
    void begin(unsigned long Baud);
    int available();
    int read();
    size_t print(const char* Text);
    size_t println(const char* Text);
};

extern HardwareSerial Serial;

namespace Mock {
    const uint8_t NumberOfPins = 64;

    extern uint8_t PinMode[NumberOfPins];
    extern uint8_t PinLevel[NumberOfPins];
    extern uint8_t ChipSelectPin;
    extern bool TransactionOpen;
    extern int32_t Errors;
    extern int32_t Frames;
    extern std::vector<byte> Frame;
    extern std::vector<byte> LastFrame;
    extern Max7219Chain Chain;
    /* the bytes Serial has yet to read, and what the sketch has printed */
    extern std::string SerialInput;
    extern std::string SerialOutput;
    /* called after every latched frame and every millisecond delay() waits, so that the simulator can draw
     * while a sketch blocks in delay(); none in the tests */
    extern void (*Tick)();

    void reset(uint8_t sChipSelectPin, uint8_t NumberOfChips);
    /* holds millis() at Milliseconds until the next reset(); delay() then moves it on without waiting */
    void setMillis(uint32_t Milliseconds);
    void transfer(uint8_t Value);
    bool usesSpi();
}

#endif
