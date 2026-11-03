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
 *                  which are SS, MOSI and SCK as well - and the SPI output drive mock::chain, and the UART reads
 *                  mock::serial_input and writes mock::serial_output: the variables the Arduino stand-in drives for the
 *                  simulator a sketch is built into, so that the panel, the terminal and the page work on either.
 *
 *****************************************************************************************************************************************************/
#ifndef MAXMATRIX_SIMULATOR_SIMULATORAVR_H_
#define MAXMATRIX_SIMULATOR_SIMULATORAVR_H_

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
    avr_t* avr_;
    elf_firmware_t firmware_;
    bool data_in_;
    bool clock_;
    bool chip_select_;
    /* the bits of the byte on the wire so far, for the frame the page shows; the model has them one by one already */
    uint8_t bits_;
    uint8_t bit_count_;
    /* simavr's receive buffer has room */
    bool receive_ready_;

    void ReceiveSerial();
    void TakeByte(uint8_t) const;
    static void OnDataIn(avr_irq_t*, uint32_t, void*);
    static void OnClock(avr_irq_t*, uint32_t, void*);
    static void OnChipSelect(avr_irq_t*, uint32_t, void*);
    static void OnSpiByte(avr_irq_t*, uint32_t, void*);
    static void OnSerialByte(avr_irq_t*, uint32_t, void*);
    static void OnReceiveReady(avr_irq_t*, uint32_t, void*);
    static void OnReceiveFull(avr_irq_t*, uint32_t, void*);

  public:
    SimulatorAvr();
    ~SimulatorAvr();

    /* the simulated AVR is the object's own, and simavr holds a pointer to the object */
    SimulatorAvr(SimulatorAvr const&) = delete;
    SimulatorAvr& operator=(SimulatorAvr const&) = delete;

    // get methods
    uint32_t GetMillis() const;

    // methods
    bool Load(char const*);
    bool Run(uint32_t);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
