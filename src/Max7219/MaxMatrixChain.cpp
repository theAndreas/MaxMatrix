// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Transfer to the chain of MAX7219.
 *
 * Every module in the chain gets one address/value pair per frame, module 0 first; a module that is
 * not addressed gets the no-op. The bytes go out through Transfer(), the only place that knows
 * whether they are bit banged or sent by hardware SPI - before, every function that sent a frame
 * had its own loop with its own \#if.
 */

#include "MaxMatrixChain.h"
#if(MAXMATRIX_USE_SPI == STD_ON)
    #include "SPI.h"
#endif
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    #include "digitalWriteFast.h"
#endif

MaxMatrixChain::MaxMatrixChain(byte data_in_pin, byte chip_select_pin, byte clock_pin)
{
    data_in_pin_ = data_in_pin;
    chip_select_pin_ = chip_select_pin;
    clock_pin_ = clock_pin;
}

void MaxMatrixChain::Init()
{
    pinMode(chip_select_pin_, OUTPUT);
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(chip_select_pin_, HIGH);
#else
    digitalWrite(chip_select_pin_, HIGH);
#endif
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.begin();
#else
    pinMode(data_in_pin_, OUTPUT);
    pinMode(clock_pin_, OUTPUT);
    /* clock idles low, MAX7219 samples data on the rising edge */
# if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(clock_pin_, LOW);
# else
    digitalWrite(clock_pin_, LOW);
# endif
#endif

    WriteRegisterAll(MAX7219_REG_SCAN_LIMIT_ADDRESS, MAX7219_REG_SCAN_LIMIT_DISPLAY_DIGIT_0_TO_7);     // display all digits
    WriteRegisterAll(MAX7219_REG_DECODE_MODE_ADDRESS, MAX7219_REG_DECODE_MODE_NO_DECODE);              // using an led matrix (not digits)
    WriteRegisterAll(MAX7219_REG_SHUTDOWN_ADDRESS, MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);        // normal operation mode
    WriteRegisterAll(MAX7219_REG_DISPLAY_TEST_ADDRESS, MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);     // no display test
}

void MaxMatrixChain::WriteRegisterAll(byte register_address, byte register_value)
{
    StartTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) Transfer(register_address, register_value);
    StopTransmission();
}

void MaxMatrixChain::WriteRegisterEach(byte register_address, byte const* register_values)
{
    StartTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) Transfer(register_address, register_values[i]);
    StopTransmission();
}

// Starts a frame: pulls chip select low, and in SPI mode reserves the bus. StopTransmission()
// has to follow once the frame is transferred.
void MaxMatrixChain::StartTransmission() /* NOLINT(readability-make-member-function-const) */
{
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.beginTransaction(SPISettings(MAXMATRIX_SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE0));
#endif
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(chip_select_pin_, LOW);
#else
    digitalWrite(chip_select_pin_, LOW);
#endif
}

/*
 * Ends a frame: pulls chip select high, so that every MAX7219 latches what it was sent, and in SPI
 * mode releases the bus for other devices. StartTransmission() has to come first.
 */
void MaxMatrixChain::StopTransmission() /* NOLINT(readability-make-member-function-const) */
{
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(chip_select_pin_, HIGH);
#else
    digitalWrite(chip_select_pin_, HIGH);
#endif
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.endTransaction();
#endif
}

// Sends the address and value of one module within a frame; StartTransmission() has to come first.
/* NOLINTNEXTLINE(readability-make-member-function-const, readability-convert-member-functions-to-static) */
void MaxMatrixChain::Transfer(byte register_address, byte register_value)
{
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.transfer(register_address);
    SPI.transfer(register_value);
#else
    shiftOut(data_in_pin_, clock_pin_, MSBFIRST, register_address);
    shiftOut(data_in_pin_, clock_pin_, MSBFIRST, register_value);
#endif
}
