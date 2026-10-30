#include "Arduino.h"
#include "SPI.h"
#include <chrono>
#include <thread>

namespace Mock {
    uint8_t PinMode[NumberOfPins];
    uint8_t PinLevel[NumberOfPins];
    uint8_t ChipSelectPin;
    bool TransactionOpen;
    int32_t Errors;
    int32_t Frames;
    std::vector<byte> Frame;
    std::vector<byte> LastFrame;
    Max7219Chain Chain;
    std::string SerialInput;
    std::string SerialOutput;
    void (*Tick)() = NULL;
    bool SpiUsed;

    /* the host clock from the first call on, as an Arduino counts from reset */
    static std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();
    /* or a clock that a test holds and sets, so that a step 50 ms on does not depend on how fast the host is */
    static bool ClockHeld = false;
    static uint32_t HeldMillis = 0;

    void setMillis(uint32_t Milliseconds)
    {
        ClockHeld = true;
        HeldMillis = Milliseconds;
    }

    void reset(uint8_t sChipSelectPin, uint8_t NumberOfChips)
    {
        ClockHeld = false;
        memset(PinMode, INPUT, sizeof(PinMode));
        memset(PinLevel, LOW, sizeof(PinLevel));
        ChipSelectPin = sChipSelectPin;
        TransactionOpen = false;
        Errors = 0;
        Frames = 0;
        Frame.clear();
        LastFrame.clear();
        SpiUsed = false;
        SerialInput.clear();
        SerialOutput.clear();
        Chain.reset(NumberOfChips);
    }

    /* a byte only reaches the chain while chip select is low, and over SPI only inside a transaction */
    void transfer(uint8_t Value)
    {
        if(PinMode[ChipSelectPin] != OUTPUT || PinLevel[ChipSelectPin] != LOW) Errors++;
        Frame.push_back(Value);
        Chain.shift(Value);
    }

    bool usesSpi() { return SpiUsed; }
}

void pinMode(uint8_t Pin, uint8_t Mode) { Mock::PinMode[Pin] = Mode; }

void digitalWrite(uint8_t Pin, uint8_t Value)
{
    /* the rising edge of chip select is where the MAX7219 latches the frame */
    if(Pin == Mock::ChipSelectPin) {
        if(Mock::PinLevel[Pin] == HIGH && Value == LOW) Mock::Frame.clear();
        if(Mock::PinLevel[Pin] == LOW && Value == HIGH) {
            Mock::LastFrame = Mock::Frame;
            Mock::Frames++;
            Mock::Chain.latch();
            if(Mock::Tick) Mock::Tick();
        }
    }
    Mock::PinLevel[Pin] = Value;
}

void shiftOut(uint8_t, uint8_t, uint8_t, uint8_t Value) { Mock::transfer(Value); }

HardwareSerial Serial;

void HardwareSerial::begin(unsigned long) {}

int HardwareSerial::available()
{
    return static_cast<int>(Mock::SerialInput.size());
}

/* -1 when nothing is waiting, as on the board */
int HardwareSerial::read()
{
    if(Mock::SerialInput.empty()) return -1;
    const uint8_t Byte = static_cast<uint8_t>(Mock::SerialInput[0]);
    Mock::SerialInput.erase(0, 1);
    return Byte;
}

size_t HardwareSerial::print(const char* Text)
{
    Mock::SerialOutput += Text;
    return strlen(Text);
}

/* a line ends in \r\n, as the Arduino core prints it */
size_t HardwareSerial::println(const char* Text)
{
    return print(Text) + print("\r\n");
}

SPIClass SPI;

void SPIClass::beginTransaction(SPISettings)
{
    if(Mock::TransactionOpen) Mock::Errors++;
    Mock::TransactionOpen = true;
    Mock::SpiUsed = true;
}

void SPIClass::endTransaction()
{
    if(!Mock::TransactionOpen) Mock::Errors++;
    Mock::TransactionOpen = false;
}

uint8_t SPIClass::transfer(uint8_t Value)
{
    if(!Mock::TransactionOpen) Mock::Errors++;
    Mock::transfer(Value);
    return 0;
}

unsigned long micros()
{
    if(Mock::ClockHeld) return static_cast<unsigned long>(Mock::HeldMillis) * 1000;
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - Mock::Start).count();
}

unsigned long millis()
{
    return micros() / 1000;
}

/* waits for real, a millisecond at a time, so that a sketch runs at its own pace in the simulator; a held clock is
 * moved on instead, which no waiting would */
void delay(unsigned long Milliseconds)
{
    if(Mock::ClockHeld) {
        for(uint32_t i = 0; i < Milliseconds; i++) {
            Mock::HeldMillis++;
            if(Mock::Tick) Mock::Tick();
        }
        return;
    }
    const uint32_t Begin = millis();
    while(static_cast<uint32_t>(millis()) - Begin < Milliseconds) {
        if(Mock::Tick) Mock::Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
