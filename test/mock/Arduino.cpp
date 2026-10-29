#include "Arduino.h"
#include "SPI.h"

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
    bool SpiUsed;

    void reset(uint8_t sChipSelectPin, uint8_t NumberOfChips)
    {
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
