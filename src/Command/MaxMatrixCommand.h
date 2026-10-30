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
/**     \file       MaxMatrixCommand.h
 *      \brief      Commands for the matrix, one per line of text
 *
 *      \details    The lines a control page or program sends to a board over Serial, read char by char and carried out
 *                  on a MaxMatrix. Knows nothing of where the chars come from.
 *
 *****************************************************************************************************************************************************/
#ifndef _MAXMATRIX_COMMAND_H_
#define _MAXMATRIX_COMMAND_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "Arduino.h"
#include "MaxMatrixStandardTypes.h"
#include "MaxMatrixConfig.h"


/******************************************************************************************************************************************************
 *  GLOBAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* what feed() says about the char it was given */
typedef enum {
    MAXMATRIX_COMMAND_PENDING,              /* the line goes on, or was empty */
    MAXMATRIX_COMMAND_DONE,                 /* the line was a command, and it is carried out */
    MAXMATRIX_COMMAND_UNKNOWN,              /* no command has this name */
    MAXMATRIX_COMMAND_INVALID_VALUE,        /* the value is missing, no number, out of range, or a char the font lacks */
    MAXMATRIX_COMMAND_TOO_LONG              /* the line did not fit into MAXMATRIX_COMMAND_LINE_LENGTH and is dropped */
} MaxMatrixCommandResultType;

class MaxMatrix;


/******************************************************************************************************************************************************
 *  CLASS  MaxMatrixCommand
 *****************************************************************************************************************************************************/
/* The commands of the Windows tool in AndreasBur/Led-Matrix, one per line: text=, textShift=, intensity=, speed=,
 * space=, setDot=column,row,value and clear. The text of textShift= is kept in a buffer of its own, because
 * setTextWithShift() only keeps a pointer and the next line overwrites the line buffer. As MaxMatrixText is, the class
 * is handed the matrix by every call rather than keeping a reference to it. */
class MaxMatrixCommand
{
  private:
    char Line[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
    char ShiftText[MAXMATRIX_COMMAND_LINE_LENGTH + 1];
    byte LineLength;
    bool LineTooLong;

    // functions
    MaxMatrixCommandResultType execute(MaxMatrix&, char*);
    MaxMatrixCommandResultType executeTextShift(MaxMatrix&, const char*);
    static MaxMatrixCommandResultType executeText(MaxMatrix&, const char*);
    static MaxMatrixCommandResultType executeDot(MaxMatrix&, char*);
    static MaxMatrixCommandResultType executeSetting(MaxMatrix&, const char*, const char*);
    static stdReturnType convertToNumber(const char*, uint16_t, uint16_t*);

  public:
    MaxMatrixCommand();

    // methods
    MaxMatrixCommandResultType feed(MaxMatrix&, char);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
