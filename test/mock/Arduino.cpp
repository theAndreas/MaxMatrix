#include "Arduino.h"
#include "SPI.h"
#include <chrono>
#include <thread>

namespace Mock {
    uint8_t PinMode[NumberOfPins];
    uint8_t PinLevel[NumberOfPins];
    uint8_t ChipSelectPin;
    bool TransactionOpen;
    int Errors;
    int Frames;
    std::vector<byte> Frame;
    std::vector<byte> LastFrame;
    Max7219Chain Chain;
    void (*Tick)() = NULL;
    bool SpiUsed;

    /* the host clock from the first call on, as an Arduino counts from reset */
    static std::chrono::steady_clock::time_point Start = std::chrono::steady_clock::now();
    /* or a clock that a test holds and sets, so that a step 50 ms on does not depend on how fast the host is */
    static bool ClockHeld = false;
    static unsigned long HeldMicros = 0;

    void setMillis(unsigned long Milliseconds)
    {
        ClockHeld = true;
        HeldMicros = Milliseconds * 1000;
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
    if(Mock::ClockHeld) return Mock::HeldMicros;
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
        for(unsigned long i = 0; i < Milliseconds; i++) {
            Mock::HeldMicros += 1000;
            if(Mock::Tick) Mock::Tick();
        }
        return;
    }
    unsigned long Begin = millis();
    while(millis() - Begin < Milliseconds) {
        if(Mock::Tick) Mock::Tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
