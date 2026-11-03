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
 *                  addressed gets the no-op. The bytes go out through Transfer(), the only place that knows whether
 *                  they are bit banged or sent by hardware SPI - before, every function that sent a frame had its own
 *                  loop with its own \#if.
 *
 *****************************************************************************************************************************************************/
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
 *  \param[in]      data_in_pin        number of the Data Input Pin
 *  \param[in]      chip_select_pin    number of the Chip Select (CS) Pin
 *  \param[in]      clock_pin          number of the Clock Pin
 *****************************************************************************************************************************************************/
MaxMatrixChain::MaxMatrixChain(byte data_in_pin, byte chip_select_pin, byte clock_pin)
{
    data_in_pin_ = data_in_pin;
    chip_select_pin_ = chip_select_pin;
    clock_pin_ = clock_pin;
} /* MaxMatrixChain */


/******************************************************************************************************************************************************
  Init()
******************************************************************************************************************************************************/
/*! \brief          initialization of the pins and the MAX7219
 *  \details        this function initializes the IO hardware and sets every MAX7219 up to drive an 8x8 matrix:
 *                  all eight digits, no decoding, normal operation
 *****************************************************************************************************************************************************/
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
} /* Init */


/******************************************************************************************************************************************************
  WriteRegisterAll()
******************************************************************************************************************************************************/
/*! \brief          write the same register of every MAX7219
 *  \details        this function sends one frame with the same address and value for every module
 *
 *  \param[in]      register_address    address of the MAX7219 register
 *  \param[in]      register_value      register value
 *****************************************************************************************************************************************************/
void MaxMatrixChain::WriteRegisterAll(byte register_address, byte register_value)
{
    StartTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) Transfer(register_address, register_value);
    StopTransmission();
} /* WriteRegisterAll */


/******************************************************************************************************************************************************
  WriteRegisterEach()
******************************************************************************************************************************************************/
/*! \brief          write the same register of every MAX7219, each with its own value
 *  \details        this function sends one frame with the same address and one value per module
 *
 *  \param[in]      register_address    address of the MAX7219 register
 *  \param[in]      register_values     one value per module, module 0 first
 *****************************************************************************************************************************************************/
void MaxMatrixChain::WriteRegisterEach(byte register_address, byte const* register_values)
{
    StartTransmission();
    for(byte i = 0; i < MAXMATRIX_NUMBER_OF_MODULES; i++) Transfer(register_address, register_values[i]);
    StopTransmission();
} /* WriteRegisterEach */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  StartTransmission()
******************************************************************************************************************************************************/
/*! \brief          start transmission to MAX7219
 *  \details        this function pulls chip select low, in SPI mode the SPI bus will be reserved too
 *
 *  \post           StopTransmission() has to be called after data was transferred
 *****************************************************************************************************************************************************/
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
} /* StartTransmission */


/******************************************************************************************************************************************************
  StopTransmission()
******************************************************************************************************************************************************/
/*! \brief          stop transmission to MAX7219
 *  \details        this function pulls chip select high, so the MAX7219 latches the transferred data.
 *                  In SPI mode the SPI bus will be released for other devices.
 *
 *  \pre            StartTransmission() has to be called first
 *****************************************************************************************************************************************************/
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
} /* StopTransmission */


/******************************************************************************************************************************************************
  Transfer()
******************************************************************************************************************************************************/
/*! \brief          send one address/value pair
 *  \details        this function sends the pair for one module of a frame
 *
 *  \param[in]      register_address    address of the MAX7219 register
 *  \param[in]      register_value      register value
 *  \pre            StartTransmission() has to be called first
 *****************************************************************************************************************************************************/
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
} /* Transfer */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
