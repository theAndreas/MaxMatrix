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
/**     \file       max_matrix_command.h
 *      \brief      Commands for the matrix, one per line of text
 *
 *      \details    The lines a control page or program sends to a board over Serial, read char by char and carried out
 *                  on a MaxMatrix. Knows nothing of where the chars come from.
 *
 *****************************************************************************************************************************************************/
#ifndef MAXMATRIX_SRC_COMMAND_MAX_MATRIX_COMMAND_H_
#define MAXMATRIX_SRC_COMMAND_MAX_MATRIX_COMMAND_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "max_matrix_standard_types.h"
#include "max_matrix_error.h"
#include "max_matrix_config.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/*! what Feed() says about the char it was given, when the line did not fail; why it failed is the error beside it */
enum class MaxMatrixCommandState : uint8_t {
    kPending,              /*!< the line goes on, or was empty */
    kDone                  /*!< the line was a command, and it is carried out */
};

class MaxMatrix;


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixCommand
 *****************************************************************************************************************************************************/
/*! The commands of the Windows tool in AndreasBur/Led-Matrix, one per line: text=, textShift=, intensity=, speed=,
 * space=, setDot=column,row,value and clear. The text of textShift= is kept in a buffer of its own, because
 * SetTextWithShift() only keeps a pointer and the next line overwrites the line buffer. As MaxMatrixText is, the class
 * is handed the matrix by every call rather than keeping a reference to it. */
class MaxMatrixCommand
{
  private:
    char line_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
    char shift_text_[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
    byte line_length_;
    bool line_too_long_;

    // functions
    MaxMatrixStatus Execute(MaxMatrix&, char*);
    MaxMatrixStatus ExecuteTextShift(MaxMatrix&, char const*);
    static MaxMatrixStatus ExecuteText(MaxMatrix&, char const*);
    static MaxMatrixStatus ExecuteDot(MaxMatrix&, char*);
    static MaxMatrixStatus ExecuteSetting(MaxMatrix&, char const*, char const*);
    static MaxMatrixResult<uint16_t> ConvertToNumber(char const*, uint16_t);

  public:
    MaxMatrixCommand();

    // methods
    MaxMatrixResult<MaxMatrixCommandState> Feed(MaxMatrix&, char);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
