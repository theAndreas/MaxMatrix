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
/**     \file       SimulatorAvr.cpp
 *      \brief      An example as the Uno runs it, in simavr, wired to the MAX7219 model
 *
 *      \details    A frame is counted as the Arduino stand-in counts it: from the falling to the rising edge of chip
 *                  select. A clock edge or an SPI byte outside a frame, and a frame that is not a whole number of
 *                  bytes, are errors in Mock::Errors, as a transfer outside a frame is there.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_AVR_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <avr_ioport.h>
#include <avr_spi.h>
#include <avr_uart.h>
#include <sim_time.h>
#include "Arduino.h"
#include "SimulatorAvr.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* the Uno's pins 10, 11 and 13 are PB2, PB3 and PB5 */
#define SIMULATOR_AVR_PORT                                          'B'
#define SIMULATOR_AVR_CHIP_SELECT_BIT                               2u
#define SIMULATOR_AVR_DATA_IN_BIT                                   3u
#define SIMULATOR_AVR_CLOCK_BIT                                     5u

/* Serial is the first and only UART of the ATmega328P */
#define SIMULATOR_AVR_UART                                          '0'


/******************************************************************************************************************************************************
 * L O C A L   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  logSimavr()
******************************************************************************************************************************************************/
/*! \brief          simavr's log, errors only
 *  \details        this function passes on what is an error; simavr also reports every section of the .elf it loads,
 *                  which would stand between the panel and the command line
 *
 *  \param[in]      Avr             the AVR, or NULL while the .elf is read
 *  \param[in]      Level           LOG_ERROR or less is passed on
 *  \param[in]      Format          as printf()
 *  \param[in]      Arguments       as vprintf()
 *****************************************************************************************************************************************************/
static void logSimavr(avr_t* Avr, int const Level, char const* Format, va_list Arguments)
{
    (void)Avr;
    if(Level <= LOG_ERROR) vfprintf(stderr, Format, Arguments);
} /* logSimavr */


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorAvr
******************************************************************************************************************************************************/
/*! \brief          SimulatorAvr constructor
 *  \details        Instantiation without an AVR; load() makes one. Chip select is high, as the pull-up of a real
 *                  module keeps it until init() drives the pin.
 *****************************************************************************************************************************************************/
SimulatorAvr::SimulatorAvr()
{
    Avr = nullptr;
    memset(&Firmware, 0, sizeof(Firmware));
    DataIn = false;
    Clock = false;
    ChipSelect = true;
    Bits = 0;
    BitCount = 0;
    ReceiveReady = false;
} /* SimulatorAvr */


/******************************************************************************************************************************************************
  DESTRUCTOR OF SimulatorAvr
******************************************************************************************************************************************************/
/*! \brief          SimulatorAvr destructor
 *  \details        Releases the AVR simavr made
 *****************************************************************************************************************************************************/
SimulatorAvr::~SimulatorAvr()
{
    if(Avr != nullptr) avr_terminate(Avr);
} /* ~SimulatorAvr */


/******************************************************************************************************************************************************
  getMillis()
******************************************************************************************************************************************************/
/*! \brief          the simulated time
 *  \details        this function counts the cycles the AVR has run, at its 16 MHz, not the host's clock
 *
 *  \return         the milliseconds since the AVR started, 0 before load()
 *****************************************************************************************************************************************************/
uint32_t SimulatorAvr::getMillis() const
{
    if(Avr == nullptr) return 0;
    return static_cast<uint32_t>(avr_cycles_to_usec(Avr, Avr->cycle) / 1000);
} /* getMillis */


/******************************************************************************************************************************************************
  load()
******************************************************************************************************************************************************/
/*! \brief          makes the AVR and loads an example into it
 *  \details        this function wires the pins, the SPI output and the UART to the model and Serial; Mock::reset() has
 *                  to have made the chain first. Called once.
 *
 *  \param[in]      ElfFile         the .elf arduino-cli made of the example
 *  \return         true
 *                  false           the file is no .elf simavr can read
 *****************************************************************************************************************************************************/
bool SimulatorAvr::load(char const* ElfFile)
{
    avr_global_logger_set(logSimavr);
    if(elf_read_firmware(ElfFile, &Firmware) != 0) return false;
    Avr = avr_make_mcu_by_name(SIMULATOR_AVR_MCU);
    if(Avr == nullptr) return false;
    avr_init(Avr);
    Avr->frequency = SIMULATOR_AVR_FREQUENCY;
    avr_load_firmware(Avr, &Firmware);

    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_IOPORT_GETIRQ(SIMULATOR_AVR_PORT), SIMULATOR_AVR_DATA_IN_BIT),
                            onDataIn, this);
    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_IOPORT_GETIRQ(SIMULATOR_AVR_PORT), SIMULATOR_AVR_CLOCK_BIT),
                            onClock, this);
    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_IOPORT_GETIRQ(SIMULATOR_AVR_PORT), SIMULATOR_AVR_CHIP_SELECT_BIT),
                            onChipSelect, this);
    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_SPI_GETIRQ(0), SPI_IRQ_OUTPUT), onSpiByte, this);

    /* the UART would print every line on the console itself, between the panel and the command line */
    uint32_t Flags = 0;
    avr_ioctl(Avr, AVR_IOCTL_UART_GET_FLAGS(SIMULATOR_AVR_UART), &Flags);
    Flags &= ~static_cast<uint32_t>(AVR_UART_FLAG_STDIO);
    avr_ioctl(Avr, AVR_IOCTL_UART_SET_FLAGS(SIMULATOR_AVR_UART), &Flags);
    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART), UART_IRQ_OUTPUT), onSerialByte, this);
    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART), UART_IRQ_OUT_XON), onReceiveReady, this);
    avr_irq_register_notify(avr_io_getirq(Avr, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART), UART_IRQ_OUT_XOFF), onReceiveFull, this);
    return true;
} /* load */


/******************************************************************************************************************************************************
  run()
******************************************************************************************************************************************************/
/*! \brief          runs the AVR on
 *  \details        this function runs until the simulated time has reached Milliseconds, and passes on what Serial has
 *                  to read on the way, when the UART has room for it
 *
 *  \param[in]      Milliseconds    the simulated time to stop at
 *  \pre            load() has succeeded
 *  \return         true
 *                  false           the AVR stopped or crashed: the example ran into a sleep with every interrupt off,
 *                                  or the watchdog fired
 *****************************************************************************************************************************************************/
bool SimulatorAvr::run(uint32_t Milliseconds)
{
    avr_cycle_count_t const End = avr_usec_to_cycles(Avr, Milliseconds * 1000u);

    receiveSerial();
    while(Avr->cycle < End) {
        int const State = avr_run(Avr);
        if(State == cpu_Done || State == cpu_Crashed) return false;
    }
    return true;
} /* run */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  receiveSerial()
******************************************************************************************************************************************************/
/*! \brief          hands the UART what Serial has to read
 *  \details        this function sends one byte after the other until the UART says its buffer is full; the rest waits
 *                  for the next time it has room
 *****************************************************************************************************************************************************/
void SimulatorAvr::receiveSerial()
{
    avr_irq_t* const Input = avr_io_getirq(Avr, AVR_IOCTL_UART_GETIRQ(SIMULATOR_AVR_UART), UART_IRQ_INPUT);

    while(ReceiveReady && !Mock::SerialInput.empty()) {
        uint8_t const Byte = static_cast<uint8_t>(Mock::SerialInput[0]);
        Mock::SerialInput.erase(0, 1);
        avr_raise_irq(Input, Byte);
    }
} /* receiveSerial */


/******************************************************************************************************************************************************
  takeByte()
******************************************************************************************************************************************************/
/*! \brief          a byte of a frame, as the page shows it
 *  \details        this function adds it to the frame; one outside a frame is an error, as in the Arduino stand-in
 *
 *  \param[in]      Byte            the byte on the wire
 *****************************************************************************************************************************************************/
void SimulatorAvr::takeByte(uint8_t Byte) const
{
    if(ChipSelect) Mock::Errors++;
    else Mock::Frame.push_back(Byte);
} /* takeByte */


/******************************************************************************************************************************************************
  onDataIn()
******************************************************************************************************************************************************/
/*! \brief          DIN changed
 *
 *  \param[in]      Irq             the pin
 *  \param[in]      Value           its level
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onDataIn(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    (void)Irq;
    static_cast<SimulatorAvr*>(Object)->DataIn = Value != 0;
} /* onDataIn */


/******************************************************************************************************************************************************
  onClock()
******************************************************************************************************************************************************/
/*! \brief          CLK changed
 *  \details        this function shifts DIN into the chain on a rising edge, as the MAX7219 does, and collects the bits
 *                  into the byte the page shows. Only bit banging moves the pin; the SPI peripheral of simavr reports
 *                  its byte without it.
 *
 *  \param[in]      Irq             the pin
 *  \param[in]      Value           its level
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onClock(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    SimulatorAvr* const Simulator = static_cast<SimulatorAvr*>(Object);
    bool const Rising = Value != 0 && !Simulator->Clock;

    (void)Irq;
    Simulator->Clock = Value != 0;
    if(!Rising) return;
    Mock::Chain.shiftBit(Simulator->DataIn);
    Simulator->Bits = static_cast<uint8_t>((Simulator->Bits << 1) | (Simulator->DataIn ? 1 : 0));
    if(++Simulator->BitCount == 8) {
        Simulator->takeByte(Simulator->Bits);
        Simulator->BitCount = 0;
    }
} /* onClock */


/******************************************************************************************************************************************************
  onChipSelect()
******************************************************************************************************************************************************/
/*! \brief          CS changed
 *  \details        this function starts a frame on a falling edge and latches it on a rising one, as every MAX7219 of
 *                  the chain does; a frame that ends within a byte is an error
 *
 *  \param[in]      Irq             the pin
 *  \param[in]      Value           its level
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onChipSelect(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    SimulatorAvr* const Simulator = static_cast<SimulatorAvr*>(Object);
    bool const High = Value != 0;

    (void)Irq;
    if(Simulator->ChipSelect && !High) {
        Mock::Frame.clear();
    } else if(!Simulator->ChipSelect && High) {
        if(Simulator->BitCount != 0) Mock::Errors++;
        Simulator->BitCount = 0;
        Mock::Chain.latch();
        Mock::LastFrame = Mock::Frame;
        Mock::Frames++;
    }
    Simulator->ChipSelect = High;
} /* onChipSelect */


/******************************************************************************************************************************************************
  onSpiByte()
******************************************************************************************************************************************************/
/*! \brief          the SPI peripheral sent a byte
 *
 *  \param[in]      Irq             the SPI output
 *  \param[in]      Value           the byte
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onSpiByte(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    uint8_t const Byte = static_cast<uint8_t>(Value);

    (void)Irq;
    Mock::Chain.shift(Byte);
    static_cast<SimulatorAvr*>(Object)->takeByte(Byte);
} /* onSpiByte */


/******************************************************************************************************************************************************
  onSerialByte()
******************************************************************************************************************************************************/
/*! \brief          the sketch printed a byte on Serial
 *
 *  \param[in]      Irq             the UART output
 *  \param[in]      Value           the byte
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onSerialByte(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    (void)Irq;
    (void)Object;
    Mock::SerialOutput += static_cast<char>(Value);
} /* onSerialByte */


/******************************************************************************************************************************************************
  onReceiveReady()
******************************************************************************************************************************************************/
/*! \brief          the UART has room for a byte
 *  \details        this function sends what Serial has to read at once, as simavr asks: it says so again and again
 *                  while there is room, and XOFF, from within the sending, when there is none
 *
 *  \param[in]      Irq             XON of the UART
 *  \param[in]      Value           -
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onReceiveReady(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    SimulatorAvr* const Simulator = static_cast<SimulatorAvr*>(Object);

    (void)Irq;
    (void)Value;
    Simulator->ReceiveReady = true;
    Simulator->receiveSerial();
} /* onReceiveReady */


/******************************************************************************************************************************************************
  onReceiveFull()
******************************************************************************************************************************************************/
/*! \brief          the UART's buffer is full
 *
 *  \param[in]      Irq             XOFF of the UART
 *  \param[in]      Value           -
 *  \param[in]      Object          the SimulatorAvr
 *****************************************************************************************************************************************************/
void SimulatorAvr::onReceiveFull(avr_irq_t* Irq, uint32_t Value, void* Object)
{
    (void)Irq;
    (void)Value;
    static_cast<SimulatorAvr*>(Object)->ReceiveReady = false;
} /* onReceiveFull */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
