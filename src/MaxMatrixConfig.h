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
#ifndef MAXMATRIX_SRC_MAXMATRIXCONFIG_H_
#define MAXMATRIX_SRC_MAXMATRIXCONFIG_H_

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
/*! the modules in the chain, at most 31 */
#ifndef MAXMATRIX_NUMBER_OF_MODULES
#define MAXMATRIX_NUMBER_OF_MODULES                                 4
#endif
/*! STD_ON sends through the hardware SPI, whose pins are fixed; STD_OFF bit bangs on any three pins */
#ifndef MAXMATRIX_USE_SPI
#define MAXMATRIX_USE_SPI                                           STD_OFF
#endif
/*! meant for the digitalWriteFast library, for chip select; has no effect yet */
#ifndef MAXMATRIX_USE_DIGITAL_WRITE_FAST
#define MAXMATRIX_USE_DIGITAL_WRITE_FAST                            STD_OFF
#endif
/*! the SPI clock in Hz; the MAX7219 takes 10 MHz at most */
#ifndef MAXMATRIX_SPI_CLOCK_SPEED
#define MAXMATRIX_SPI_CLOCK_SPEED                                   10000000
#endif
/*! the empty columns between two chars of a text, until SetSpaceBetweenChars() */
#ifndef MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE
#define MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE                    1
#endif
/*! the intensity Init() sets, 0 to 15 */
#ifndef MAXMATRIX_INTENSITY_INIT_VALUE
#define MAXMATRIX_INTENSITY_INIT_VALUE                              5
#endif
/*! the chars of a line MaxMatrixCommand takes; it costs twice as many bytes of RAM */
#ifndef MAXMATRIX_COMMAND_LINE_LENGTH
#define MAXMATRIX_COMMAND_LINE_LENGTH                               64
#endif
/*! the milliseconds per column of a shifting text, until SetShiftInterval(); 0 for a step on every call */
#ifndef MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE
#define MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE                         50
#endif
/* STD_ON reports every error to MaxMatrixReportError(), see MaxMatrixError.h */
#ifndef MAXMATRIX_DEV_ERROR_DETECT
#define MAXMATRIX_DEV_ERROR_DETECT                                  STD_OFF
#endif

/* MaxMatrix Parameter */
#define MAXMATRIX_COLUMN_NUMBER_OF_MODULE                           8   /*!< the columns of one module */
#define MAXMATRIX_ROW_NUMBER_OF_MODULE                              8   /*!< the rows of one module, and of the matrix */
#define MAXMATRIX_NUMBER_OF_COLUMNS                                 (MAXMATRIX_NUMBER_OF_MODULES * MAXMATRIX_COLUMN_NUMBER_OF_MODULE)   /*!< the columns of the matrix */
#define MAXMATRIX_BUFFER_SIZE                                       MAXMATRIX_NUMBER_OF_COLUMNS   /*!< the bytes of the picture, one per column */

/* a column is counted in a byte throughout, as an index into matrix_buffer_ and in every loop over the columns */
static_assert(MAXMATRIX_NUMBER_OF_COLUMNS <= 255, "at most 31 modules: their columns have to be counted in a byte");

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
