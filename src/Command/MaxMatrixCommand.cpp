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
/**     \file       MaxMatrixCommand.cpp
 *      \brief      Commands for the matrix, one per line of text
 *
 *      \details    The names are compared with strcmp_P against strings in flash: in RAM, on the Uno, they would take
 *                  their length again for nothing.
 *
 *****************************************************************************************************************************************************/
#define _MAXMATRIX_COMMAND_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "MaxMatrixCommand.h"
#include "MaxMatrix.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* MaxMatrixCommand Parameter */
#define MAXMATRIX_COMMAND_LINE_END                                  '\n'
#define MAXMATRIX_COMMAND_CARRIAGE_RETURN                           '\r'
#define MAXMATRIX_COMMAND_VALUE_SEPARATOR                           '='
#define MAXMATRIX_COMMAND_LIST_SEPARATOR                            ','

/* the largest numbers the commands take: a column, row or space fits a byte, the shift interval a uint16_t */
#define MAXMATRIX_COMMAND_MAX_BYTE                                  0xFFu
#define MAXMATRIX_COMMAND_MAX_INTERVAL                              0xFFFFu


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF MaxMatrixCommand
******************************************************************************************************************************************************/
/*! \brief          MaxMatrixCommand constructor
 *  \details        Instantiation of a reader at the start of an empty line
 *
 *  \return         -
 *****************************************************************************************************************************************************/
MaxMatrixCommand::MaxMatrixCommand()
{
    Line[0] = STD_NULL_CHARACTER;
    ShiftText[0] = STD_NULL_CHARACTER;
    LineLength = 0;
    LineTooLong = false;
} /* MaxMatrixCommand */


/******************************************************************************************************************************************************
  feed()
******************************************************************************************************************************************************/
/*! \brief          takes the next char of a command line
 *  \details        this function collects the chars up to a line feed and then carries the line out. A carriage return
 *                  is skipped, so that lines may end in \r\n as well; an empty line is no command. A line too long for
 *                  the buffer is dropped whole, rather than carried out cut short.
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in]      Char        the next char, as Serial.read() returns it
 *  \return         MAXMATRIX_COMMAND_PENDING while the line goes on, else what came of the line
 *****************************************************************************************************************************************************/
MaxMatrixCommandResultType MaxMatrixCommand::feed(MaxMatrix& Matrix, char Char)
{
    if(Char == MAXMATRIX_COMMAND_CARRIAGE_RETURN) return MAXMATRIX_COMMAND_PENDING;
    if(Char != MAXMATRIX_COMMAND_LINE_END) {
        if(LineLength < MAXMATRIX_COMMAND_LINE_LENGTH) Line[LineLength++] = Char;
        else LineTooLong = true;
        return MAXMATRIX_COMMAND_PENDING;
    }

    const bool TooLong = LineTooLong;
    const bool Empty = LineLength == 0;

    Line[LineLength] = STD_NULL_CHARACTER;
    LineLength = 0;
    LineTooLong = false;
    if(TooLong) return MAXMATRIX_COMMAND_TOO_LONG;
    if(Empty) return MAXMATRIX_COMMAND_PENDING;
    return execute(Matrix, Line);
} /* feed */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  execute()
******************************************************************************************************************************************************/
/*! \brief          carries out a whole line
 *  \details        this function splits the line at its first '=' into name and value and passes it on by name
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in,out]  Text        the line, without its line end; the '=' is overwritten with the end of the name
 *  \return         what came of the line
 *****************************************************************************************************************************************************/
MaxMatrixCommandResultType MaxMatrixCommand::execute(MaxMatrix& Matrix, char* Text)
{
    char* Value = strchr(Text, MAXMATRIX_COMMAND_VALUE_SEPARATOR);

    if(Value == NULL) {
        if(0 != strcmp_P(Text, PSTR("clear"))) return MAXMATRIX_COMMAND_UNKNOWN;
        Matrix.clear();
        return MAXMATRIX_COMMAND_DONE;
    }
    *Value++ = STD_NULL_CHARACTER;
    if(0 == strcmp_P(Text, PSTR("text"))) return executeText(Matrix, Value);
    if(0 == strcmp_P(Text, PSTR("textShift"))) return executeTextShift(Matrix, Value);
    if(0 == strcmp_P(Text, PSTR("setDot"))) return executeDot(Matrix, Value);
    return executeSetting(Matrix, Text, Value);
} /* execute */


/******************************************************************************************************************************************************
  executeTextShift()
******************************************************************************************************************************************************/
/*! \brief          textShift=TEXT shifts a text in from the right
 *  \details        this function copies the text out of the line buffer first: the shift reads it for as long as it
 *                  runs, and the next line would overwrite it there
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in]      Value       the text, UTF-8 or latin-1
 *  \return         MAXMATRIX_COMMAND_DONE
 *                  MAXMATRIX_COMMAND_INVALID_VALUE     the text is empty
 *****************************************************************************************************************************************************/
MaxMatrixCommandResultType MaxMatrixCommand::executeTextShift(MaxMatrix& Matrix, const char* Value)
{
    if(*Value == STD_NULL_CHARACTER) return MAXMATRIX_COMMAND_INVALID_VALUE;
    strcpy(ShiftText, Value);
    return E_OK == Matrix.setTextWithShift(ShiftText) ? MAXMATRIX_COMMAND_DONE : MAXMATRIX_COMMAND_INVALID_VALUE;
} /* executeTextShift */


/******************************************************************************************************************************************************
  executeText()
******************************************************************************************************************************************************/
/*! \brief          text=TEXT shows a text from the left edge
 *  \details        this function clears the matrix first and sends clear and text as one picture; an empty text only
 *                  clears it
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in]      Value       the text, UTF-8 or latin-1
 *  \return         MAXMATRIX_COMMAND_DONE
 *                  MAXMATRIX_COMMAND_INVALID_VALUE     a char of the text is not in the font; the chars before it are shown
 *****************************************************************************************************************************************************/
MaxMatrixCommandResultType MaxMatrixCommand::executeText(MaxMatrix& Matrix, const char* Value)
{
    MaxMatrixBatch Batch(Matrix);

    Matrix.clear();
    return E_OK == Matrix.setText(Value) ? MAXMATRIX_COMMAND_DONE : MAXMATRIX_COMMAND_INVALID_VALUE;
} /* executeText */


/******************************************************************************************************************************************************
  executeDot()
******************************************************************************************************************************************************/
/*! \brief          setDot=COLUMN,ROW,VALUE sets one dot
 *  \details        this function reads three numbers separated by commas; the value is 0 or 1
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in,out]  Value       the three numbers; the commas are overwritten with ends of text
 *  \return         MAXMATRIX_COMMAND_DONE
 *                  MAXMATRIX_COMMAND_INVALID_VALUE     a number is missing or the dot is not on the matrix
 *****************************************************************************************************************************************************/
MaxMatrixCommandResultType MaxMatrixCommand::executeDot(MaxMatrix& Matrix, char* Value)
{
    char* RowText = strchr(Value, MAXMATRIX_COMMAND_LIST_SEPARATOR);
    char* LitText = RowText == NULL ? NULL : strchr(RowText + 1, MAXMATRIX_COMMAND_LIST_SEPARATOR);
    uint16_t Column, Row, Lit;

    if(LitText == NULL) return MAXMATRIX_COMMAND_INVALID_VALUE;
    *RowText++ = STD_NULL_CHARACTER;
    *LitText++ = STD_NULL_CHARACTER;
    if(E_OK != convertToNumber(Value, MAXMATRIX_COMMAND_MAX_BYTE, &Column) || E_OK != convertToNumber(RowText, MAXMATRIX_COMMAND_MAX_BYTE, &Row) ||
       E_OK != convertToNumber(LitText, 1, &Lit)) return MAXMATRIX_COMMAND_INVALID_VALUE;
    return E_OK == Matrix.setDot(Column, Row, Lit == 1) ? MAXMATRIX_COMMAND_DONE : MAXMATRIX_COMMAND_INVALID_VALUE;
} /* executeDot */


/******************************************************************************************************************************************************
  executeSetting()
******************************************************************************************************************************************************/
/*! \brief          the commands that set a number
 *  \details        intensity=0..15 sets the brightness, speed=MILLISECONDS the shift interval per column and
 *                  space=COLUMNS the columns between the chars of a text
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in]      Name        the name of the command
 *  \param[in]      Value       the number
 *  \return         MAXMATRIX_COMMAND_DONE
 *                  MAXMATRIX_COMMAND_UNKNOWN           no command has this name
 *                  MAXMATRIX_COMMAND_INVALID_VALUE     the value is no number or out of range
 *****************************************************************************************************************************************************/
MaxMatrixCommandResultType MaxMatrixCommand::executeSetting(MaxMatrix& Matrix, const char* Name, const char* Value)
{
    uint16_t Number;

    if(0 == strcmp_P(Name, PSTR("intensity"))) {
        if(E_OK != convertToNumber(Value, MAX7219_REG_INTENSITY_MAX_VALUE, &Number)) return MAXMATRIX_COMMAND_INVALID_VALUE;
        Matrix.setIntensity(Number);
    } else if(0 == strcmp_P(Name, PSTR("speed"))) {
        if(E_OK != convertToNumber(Value, MAXMATRIX_COMMAND_MAX_INTERVAL, &Number)) return MAXMATRIX_COMMAND_INVALID_VALUE;
        Matrix.setShiftInterval(Number);
    } else if(0 == strcmp_P(Name, PSTR("space"))) {
        if(E_OK != convertToNumber(Value, MAXMATRIX_COMMAND_MAX_BYTE, &Number)) return MAXMATRIX_COMMAND_INVALID_VALUE;
        Matrix.setSpaceBetweenChars(Number);
    } else return MAXMATRIX_COMMAND_UNKNOWN;
    return MAXMATRIX_COMMAND_DONE;
} /* executeSetting */


/******************************************************************************************************************************************************
  convertToNumber()
******************************************************************************************************************************************************/
/*! \brief          converts a decimal number
 *  \details        this function takes digits only, at least one, and stops counting as soon as the number is past
 *                  Maximum, so that a long run of digits cannot overflow into a small number
 *
 *  \param[in]      Text        the digits
 *  \param[in]      Maximum     the largest number taken
 *  \param[out]     Number      the number
 *  \return         E_OK
 *                  E_NOT_OK    the text is empty, holds anything but digits, or the number is larger than Maximum
 *****************************************************************************************************************************************************/
stdReturnType MaxMatrixCommand::convertToNumber(const char* Text, uint16_t Maximum, uint16_t* Number)
{
    uint32_t Value = 0;

    if(*Text == STD_NULL_CHARACTER) return E_NOT_OK;
    for(; *Text != STD_NULL_CHARACTER; Text++) {
        if(*Text < '0' || *Text > '9') return E_NOT_OK;
        Value = Value * 10 + (*Text - '0');
        if(Value > Maximum) return E_NOT_OK;
    }
    *Number = static_cast<uint16_t>(Value);
    return E_OK;
} /* convertToNumber */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
