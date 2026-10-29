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
/**     \file       MaxMatrixChain.cpp
 *      \brief      Transfer to the chain of MAX7219
 *
 *      \details    Every module in the chain gets one address/value pair per frame, module 0 first; a module that is not
 *                  addressed gets the no-op. The bytes go out through transfer(), the only place that knows whether
 *                  they are bit banged or sent by hardware SPI - before, every function that sent a frame had its own
 *                  loop with its own #if.
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_CHAIN_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixChain.h"
#if(MAXMATRIX_USE_SPI == STD_ON)
    #include "SPI.h"
#endif
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    #include "digitalWriteFast.h"
#endif


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrixChain
******************************************************************************************************************************************************/
/*! \brief          MaxMatrixChain constructor
 *  \details        Instantiation of the transfer to the chain
 *
 *  \param[in]      sDataInPin         number of the Data Input Pin
 *  \param[in]      sChipSelectPin     number of the Chip Select (CS) Pin
 *  \param[in]      sClockPin          number of the Clock Pin
 *  \return         -
 *****************************************************************************************************************************************************/
MaxMatrixChain::MaxMatrixChain(byte sDataInPin, byte sChipSelectPin, byte sClockPin)
{
    DataInPin = sDataInPin;
    ChipSelectPin = sChipSelectPin;
    ClockPin = sClockPin;
} /* MaxMatrixChain */


/******************************************************************************************************************************************************
  init()
******************************************************************************************************************************************************/
/*! \brief          initialization of the pins and the MAX7219
 *  \details        this function initializes the IO hardware and sets every MAX7219 up to drive an 8x8 matrix:
 *                  all eight digits, no decoding, normal operation
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixChain::init()
{
    pinMode(ChipSelectPin, OUTPUT);
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ChipSelectPin, HIGH);
#else
    digitalWrite(ChipSelectPin, HIGH);
#endif
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.begin();
#else
    pinMode(DataInPin, OUTPUT);
    pinMode(ClockPin, OUTPUT);
    /* clock idles low, MAX7219 samples data on the rising edge */
# if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ClockPin, LOW);
# else
    digitalWrite(ClockPin, LOW);
# endif
#endif

    writeRegisterAll(MAX7219_REG_SCAN_LIMIT_ADDRESS, MAX7219_REG_SCAN_LIMIT_DISPLAY_DIGIT_0_TO_7);     // display all digits
    writeRegisterAll(MAX7219_REG_DECODE_MODE_ADDRESS, MAX7219_REG_DECODE_MODE_NO_DECODE);              // using an led matrix (not digits)
    writeRegisterAll(MAX7219_REG_SHUTDOWN_ADDRESS, MAX7219_REG_SHUTDOWN_MODE_NORMAL_OPERATION);        // normal operation mode
    writeRegisterAll(MAX7219_REG_DISPLAY_TEST_ADDRESS, MAX7219_REG_DISPLAY_TEST_NORMAL_OPERATION);     // no display test
} /* init */


/******************************************************************************************************************************************************
  writeRegister()
******************************************************************************************************************************************************/
/*! \brief          write a register of one MAX7219
 *  \details        this function sends one frame, in which every other module gets the no-op
 *
 *  \param[in]      Module              module whose register is written, 0 is the first one in the frame
 *  \param[in]      RegisterAddress     address of the MAX7219 register
 *  \param[in]      RegisterValue       register value
 *  \return         -
 *  \pre            Module is within the chain
 *****************************************************************************************************************************************************/
void MaxMatrixChain::writeRegister(byte Module, byte RegisterAddress, byte RegisterValue)
{
    startTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++)
    {
        if(i == Module) transfer(RegisterAddress, RegisterValue);
        else transfer(MAX7219_REG_NO_OP_ADDRESS, 0);
    }
    stopTransmission();
} /* writeRegister */


/******************************************************************************************************************************************************
  writeRegisterAll()
******************************************************************************************************************************************************/
/*! \brief          write the same register of every MAX7219
 *  \details        this function sends one frame with the same address and value for every module
 *
 *  \param[in]      RegisterAddress     address of the MAX7219 register
 *  \param[in]      RegisterValue       register value
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixChain::writeRegisterAll(byte RegisterAddress, byte RegisterValue)
{
    startTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) transfer(RegisterAddress, RegisterValue);
    stopTransmission();
} /* writeRegisterAll */


/******************************************************************************************************************************************************
  writeRegisterEach()
******************************************************************************************************************************************************/
/*! \brief          write the same register of every MAX7219, each with its own value
 *  \details        this function sends one frame with the same address and one value per module
 *
 *  \param[in]      RegisterAddress     address of the MAX7219 register
 *  \param[in]      RegisterValues      one value per module, module 0 first
 *  \return         -
 *****************************************************************************************************************************************************/
void MaxMatrixChain::writeRegisterEach(byte RegisterAddress, const byte* RegisterValues)
{
    startTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) transfer(RegisterAddress, RegisterValues[i]);
    stopTransmission();
} /* writeRegisterEach */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  startTransmission()
******************************************************************************************************************************************************/
/*! \brief          start transmission to MAX7219
 *  \details        this function pulls chip select low, in SPI mode the SPI bus will be reserved too
 *
 *  \return         -
 *  \post           stopTransmission() has to be called after data was transferred
 *****************************************************************************************************************************************************/
void MaxMatrixChain::startTransmission()
{
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.beginTransaction(SPISettings(MAXMATRIX_SPI_CLOCK_SPEED, MSBFIRST, SPI_MODE0));
#endif
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ChipSelectPin, LOW);
#else
    digitalWrite(ChipSelectPin, LOW);
#endif
} /* startTransmission */


/******************************************************************************************************************************************************
  stopTransmission()
******************************************************************************************************************************************************/
/*! \brief          stop transmission to MAX7219
 *  \details        this function pulls chip select high, so the MAX7219 latches the transferred data.
 *                  In SPI mode the SPI bus will be released for other devices.
 *
 *  \return         -
 *  \pre            startTransmission() has to be called first
 *****************************************************************************************************************************************************/
void MaxMatrixChain::stopTransmission()
{
#if(MAXMATRIX_USE_DIGITAL_WRITE_FAST == STD_ON)
    digitalWriteFast(ChipSelectPin, HIGH);
#else
    digitalWrite(ChipSelectPin, HIGH);
#endif
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.endTransaction();
#endif
} /* stopTransmission */


/******************************************************************************************************************************************************
  transfer()
******************************************************************************************************************************************************/
/*! \brief          send one address/value pair
 *  \details        this function sends the pair for one module of a frame
 *
 *  \param[in]      RegisterAddress     address of the MAX7219 register
 *  \param[in]      RegisterValue       register value
 *  \return         -
 *  \pre            startTransmission() has to be called first
 *****************************************************************************************************************************************************/
void MaxMatrixChain::transfer(byte RegisterAddress, byte RegisterValue)
{
#if(MAXMATRIX_USE_SPI == STD_ON)
    SPI.transfer(RegisterAddress);
    SPI.transfer(RegisterValue);
#else
    shiftOut(DataInPin, ClockPin, MSBFIRST, RegisterAddress);
    shiftOut(DataInPin, ClockPin, MSBFIRST, RegisterValue);
#endif
} /* transfer */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
