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
/**     \file       MaxMatrixConfig.h
 *      \brief      Configuration of the MaxMatrix library
 *
 *      \details    The parameters every part of the library builds on: the size of the chain and how it is driven.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_CONFIG_H_
#define _MAXMATRIX_CONFIG_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixStandardTypes.h"


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* MaxMatrix Configuration Parameter
 * Each one can be given on the compiler command line instead, which is how the tests build both the bit banging and the
 * SPI variant. The Arduino IDE does not pass a sketch's defines to a library, so for a sketch they are changed here. */
#ifndef MAXMATRIX_NUMBER_OF_MODULES
#define MAXMATRIX_NUMBER_OF_MODULES                                 4
#endif
#ifndef MAXMATRIX_USE_SPI
#define MAXMATRIX_USE_SPI                                           STD_OFF
#endif
#ifndef MAXMATRIX_USE_DIGITAL_WRITE_FAST
#define MAXMATRIX_USE_DIGITAL_WRITE_FAST                            STD_OFF
#endif
#ifndef MAXMATRIX_SPI_CLOCK_SPEED
#define MAXMATRIX_SPI_CLOCK_SPEED                                   10000000   /* MAX7219 supports max. 10 MHz */
#endif
#ifndef MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE
#define MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE                    1
#endif
#ifndef MAXMATRIX_INTENSITY_INIT_VALUE
#define MAXMATRIX_INTENSITY_INIT_VALUE                              5
#endif
#ifndef MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE
#define MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE                         50         /* ms per column, 0 for a step on every call */
#endif

/* MaxMatrix Parameter */
#define MAXMATRIX_COLUMN_NUMBER_OF_MODULE                           8
#define MAXMATRIX_ROW_NUMBER_OF_MODULE                              8
#define MAXMATRIX_NUMBER_OF_COLUMNS                                 (MAXMATRIX_NUMBER_OF_MODULES * MAXMATRIX_COLUMN_NUMBER_OF_MODULE)
#define MAXMATRIX_BUFFER_SIZE                                       MAXMATRIX_NUMBER_OF_COLUMNS

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
