#include "Arduino.h"
#include "SPI.h"
#include <chrono>
#include <thread>

namespace mock {
    uint8_t pin_mode[kNumberOfPins];
    uint8_t pin_level[kNumberOfPins];
    uint8_t chip_select_pin;
    bool transaction_open;
    int32_t errors;
    int32_t frames;
    std::vector<byte> frame;
    std::vector<byte> last_frame;
    Max7219Chain chain;
    std::string serial_input;
    std::string serial_output;
    void (*tick)() = NULL;
    bool spi_used;

    /* the host clock from the first call on, as an Arduino counts from reset */
    static std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    /* or a clock that a test holds and sets, so that a step 50 ms on does not depend on how fast the host is */
    static bool clock_held = false;
    static uint32_t held_millis = 0;

    void SetMillis(uint32_t milliseconds)
    {
        clock_held = true;
        held_millis = milliseconds;
    }

    void Reset(uint8_t chip_select_pin, uint8_t number_of_chips)
    {
        clock_held = false;
        memset(pin_mode, INPUT, sizeof(pin_mode));
        memset(pin_level, LOW, sizeof(pin_level));
        mock::chip_select_pin = chip_select_pin;
        transaction_open = false;
        errors = 0;
        frames = 0;
        frame.clear();
        last_frame.clear();
        spi_used = false;
        serial_input.clear();
        serial_output.clear();
        chain.Reset(number_of_chips);
    }

    /* a byte only reaches the chain while chip select is low, and over SPI only inside a transaction */
    void Transfer(uint8_t value)
    {
        if(pin_mode[chip_select_pin] != OUTPUT || pin_level[chip_select_pin] != LOW) errors++;
        frame.push_back(value);
        chain.Shift(value);
    }

    bool UsesSpi() { return spi_used; }
}

void pinMode(uint8_t pin, uint8_t mode) { mock::pin_mode[pin] = mode; }

void digitalWrite(uint8_t pin, uint8_t value)
{
    /* the rising edge of chip select is where the MAX7219 latches the frame */
    if(pin == mock::chip_select_pin) {
        if(mock::pin_level[pin] == HIGH && value == LOW) mock::frame.clear();
        if(mock::pin_level[pin] == LOW && value == HIGH) {
            mock::last_frame = mock::frame;
            mock::frames++;
            mock::chain.Latch();
            if(mock::tick) mock::tick();
        }
    }
    mock::pin_level[pin] = value;
}

void shiftOut(uint8_t, uint8_t, uint8_t, uint8_t value) { mock::Transfer(value); }

HardwareSerial Serial;

void HardwareSerial::begin(unsigned long) {}

int HardwareSerial::available()
{
    return static_cast<int>(mock::serial_input.size());
}

/* -1 when nothing is waiting, as on the board */
int HardwareSerial::read()
{
    if(mock::serial_input.empty()) return -1;
    uint8_t const received = static_cast<uint8_t>(mock::serial_input[0]);
    mock::serial_input.erase(0, 1);
    return received;
}

size_t HardwareSerial::write(uint8_t c)
{
    mock::serial_output += static_cast<char>(c);
    return 1;
}

size_t Print::write(char const* text)
{
    size_t written = 0;

    while(*text != '\0') written += write(static_cast<uint8_t>(*text++));
    return written;
}

size_t Print::print(char const* text)
{
    return write(text);
}

size_t Print::println(char const* text)
{
    return print(text) + println();
}

size_t Print::print(__FlashStringHelper const* text)
{
    return print(reinterpret_cast<char const*>(text));
}

size_t Print::println(__FlashStringHelper const* text)
{
    return println(reinterpret_cast<char const*>(text));
}

size_t Print::println()
{
    return write("\r\n");
}

SPIClass SPI;

void SPIClass::beginTransaction(SPISettings)
{
    if(mock::transaction_open) mock::errors++;
    mock::transaction_open = true;
    mock::spi_used = true;
}

void SPIClass::endTransaction()
{
    if(!mock::transaction_open) mock::errors++;
    mock::transaction_open = false;
}

uint8_t SPIClass::transfer(uint8_t value)
{
    if(!mock::transaction_open) mock::errors++;
    mock::Transfer(value);
    return 0;
}

unsigned long micros()
{
    if(mock::clock_held) return static_cast<unsigned long>(mock::held_millis) * 1000;
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - mock::start).count();
}

unsigned long millis()
{
    return micros() / 1000;
}

/* waits for real, a millisecond at a time, so that a sketch runs at its own pace in the simulator; a held clock is
 * moved on instead, which no waiting would */
void delay(unsigned long milliseconds)
{
    if(mock::clock_held) {
        for(uint32_t i = 0; i < milliseconds; i++) {
            mock::held_millis++;
            if(mock::tick) mock::tick();
        }
        return;
    }
    uint32_t const begin = millis();
    while(static_cast<uint32_t>(millis()) - begin < milliseconds) {
        if(mock::tick) mock::tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
