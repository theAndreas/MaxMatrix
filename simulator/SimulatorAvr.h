/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       SimulatorAvr.h
 *      \brief      An example as the Uno runs it, in simavr, wired to the MAX7219 model
 *
 *      \details    The .elf avr-gcc made runs on a simulated ATmega328P at 16 MHz. Pins 10, 11 and 13 - CS, DIN and CLK,
 *                  which are SS, MOSI and SCK as well - and the SPI output drive Mock::Chain, and the UART reads
 *                  Mock::SerialInput and writes Mock::SerialOutput: the variables the Arduino stand-in drives for the
 *                  simulator a sketch is built into, so that the panel, the terminal and the page work on either.
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_AVR_H_
#define _SIMULATOR_AVR_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdint.h>
#include <sim_avr.h>
#include <sim_elf.h>


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorAvr Parameter */
#define SIMULATOR_AVR_MCU                                           "atmega328p"
#define SIMULATOR_AVR_FREQUENCY                                     16000000u


/******************************************************************************************************************************************************
 *  CLASS  SimulatorAvr
 *****************************************************************************************************************************************************/
/* simavr reports a pin or a byte through a C function and a pointer it was given, so each of those is a static function
 * that is handed the object. The pins are followed rather than the port registers: digitalWrite() and shiftOut() write
 * the port bit by bit, and only a change is a step on the wire. */
class SimulatorAvr
{
  private:
    avr_t* Avr;
    elf_firmware_t Firmware;
    bool DataIn;
    bool Clock;
    bool ChipSelect;
    /* the bits of the byte on the wire so far, for the frame the page shows; the model has them one by one already */
    uint8_t Bits;
    uint8_t BitCount;
    /* simavr's receive buffer has room */
    bool ReceiveReady;

    void receiveSerial();
    void takeByte(uint8_t) const;
    static void onDataIn(avr_irq_t*, uint32_t, void*);
    static void onClock(avr_irq_t*, uint32_t, void*);
    static void onChipSelect(avr_irq_t*, uint32_t, void*);
    static void onSpiByte(avr_irq_t*, uint32_t, void*);
    static void onSerialByte(avr_irq_t*, uint32_t, void*);
    static void onReceiveReady(avr_irq_t*, uint32_t, void*);
    static void onReceiveFull(avr_irq_t*, uint32_t, void*);

  public:
    SimulatorAvr();
    ~SimulatorAvr();

    /* the simulated AVR is the object's own, and simavr holds a pointer to the object */
    SimulatorAvr(SimulatorAvr const&) = delete;
    SimulatorAvr& operator=(SimulatorAvr const&) = delete;

    // get methods
    uint32_t getMillis() const;

    // methods
    bool load(char const*);
    bool run(uint32_t);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
