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
 *****************************************************************************************************************************************************/
MaxMatrixCommand::MaxMatrixCommand()
{
    line_[0] = STD_NULL_CHARACTER;
    shift_text_[0] = STD_NULL_CHARACTER;
    line_length_ = 0;
    line_too_long_ = false;
} /* MaxMatrixCommand */


/******************************************************************************************************************************************************
  feed()
******************************************************************************************************************************************************/
/*! \brief          takes the next char of a command line
 *  \details        this function collects the chars up to a line feed and then carries the line out. A carriage return
 *                  is skipped, so that lines may end in \\r\\n as well; an empty line is no command. A line too long for
 *                  the buffer is dropped whole, rather than carried out cut short.
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in]      c        the next char, as Serial.read() returns it
 *  \return         MaxMatrixCommandState::kPending while the line goes on or after an empty line
 *                  MaxMatrixCommandState::kDone      the line was carried out
 *                  MaxMatrixError::kLineTooLong   the line did not fit and is dropped
 *                  or why the command failed, as execute() returns it
 *****************************************************************************************************************************************************/
MaxMatrixResult<MaxMatrixCommandState> MaxMatrixCommand::Feed(MaxMatrix& matrix, char c)
{
    if(c == MAXMATRIX_COMMAND_CARRIAGE_RETURN) return MaxMatrixCommandState::kPending;
    if(c != MAXMATRIX_COMMAND_LINE_END) {
        if(line_length_ < MAXMATRIX_COMMAND_LINE_LENGTH) line_[line_length_++] = c;
        else line_too_long_ = true;
        return MaxMatrixCommandState::kPending;
    }

    bool const too_long = line_too_long_;
    bool const empty = line_length_ == 0;

    line_[line_length_] = STD_NULL_CHARACTER;
    line_length_ = 0;
    line_too_long_ = false;
    if(too_long) return MaxMatrixError::kLineTooLong;
    if(empty) return MaxMatrixCommandState::kPending;
    MaxMatrixStatus status = Execute(matrix, line_);
    if(!status.IsOk()) return status.GetError();
    return MaxMatrixCommandState::kDone;
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
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kUnknownCommand     no command has this name
 *                  or why the command failed
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrixCommand::Execute(MaxMatrix& matrix, char* text)
{
    char* value = strchr(text, MAXMATRIX_COMMAND_VALUE_SEPARATOR);

    if(value == NULL) {
        if(0 != strcmp_P(text, PSTR("clear"))) return MaxMatrixError::kUnknownCommand;
        matrix.Clear();
        return MaxMatrixError::kOk;
    }
    *value++ = STD_NULL_CHARACTER;
    if(0 == strcmp_P(text, PSTR("text"))) return ExecuteText(matrix, value);
    if(0 == strcmp_P(text, PSTR("textShift"))) return ExecuteTextShift(matrix, value);
    if(0 == strcmp_P(text, PSTR("setDot"))) return ExecuteDot(matrix, value);
    return ExecuteSetting(matrix, text, value);
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
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kNoText     the text is empty
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrixCommand::ExecuteTextShift(MaxMatrix& matrix, char const* value)
{
    /* the text is part of Line, so it fits into ShiftText as long as that is no shorter; strlcpy() would check the
       same at run time, for 30 bytes of flash in SerialControl */
    static_assert(sizeof(shift_text_) >= sizeof(line_), "ShiftText has to hold any text of a line");

    if(*value == STD_NULL_CHARACTER) return MaxMatrixError::kNoText;
    strcpy(shift_text_, value); /* NOLINT(clang-analyzer-security.insecureAPI.strcpy) */
    return matrix.SetTextWithShift(shift_text_);
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
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kUnknownChar    a char of the text is not in the font; the chars before it are shown
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrixCommand::ExecuteText(MaxMatrix& matrix, char const* value)
{
    MaxMatrixBatch batch(matrix);

    matrix.Clear();
    return matrix.SetText(value);
} /* executeText */


/******************************************************************************************************************************************************
  executeDot()
******************************************************************************************************************************************************/
/*! \brief          setDot=COLUMN,ROW,VALUE sets one dot
 *  \details        this function reads three numbers separated by commas; the value is 0 or 1. The dot is checked
 *                  against the panel here, because setDot() leaves a dot off the panel out without a word, and
 *                  whoever typed the line should learn that it did nothing.
 *
 *  \param[in,out]  Matrix      the matrix the command acts on
 *  \param[in,out]  Value       the three numbers; the commas are overwritten with ends of text
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kNotANumber    a number is missing, empty or holds anything but digits
 *                  MaxMatrixError::kOutOfRange    the dot is not on the matrix, or the value is neither 0 nor 1
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrixCommand::ExecuteDot(MaxMatrix& matrix, char* value)
{
    char* row_text = strchr(value, MAXMATRIX_COMMAND_LIST_SEPARATOR);
    char* lit_text = row_text == NULL ? NULL : strchr(row_text + 1, MAXMATRIX_COMMAND_LIST_SEPARATOR);

    if(lit_text == NULL) return MaxMatrixError::kNotANumber;
    *row_text++ = STD_NULL_CHARACTER;
    *lit_text++ = STD_NULL_CHARACTER;
    MaxMatrixResult<uint16_t> column = ConvertToNumber(value, MAXMATRIX_NUMBER_OF_COLUMNS - 1);
    MaxMatrixResult<uint16_t> row = ConvertToNumber(row_text, MAXMATRIX_ROW_NUMBER_OF_MODULE - 1);
    MaxMatrixResult<uint16_t> lit = ConvertToNumber(lit_text, 1);
    if(!column.IsOk()) return column.GetError();
    if(!row.IsOk()) return row.GetError();
    if(!lit.IsOk()) return lit.GetError();
    matrix.SetDot(static_cast<int16_t>(column.GetValue()), static_cast<int16_t>(row.GetValue()), lit.GetValue() == 1);
    return MaxMatrixError::kOk;
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
 *  \return         MaxMatrixError::kOk
 *                  MaxMatrixError::kUnknownCommand     no command has this name
 *                  MaxMatrixError::kNotANumber        the value is empty or holds anything but digits
 *                  MaxMatrixError::kOutOfRange        the number is too large for the setting
 *****************************************************************************************************************************************************/
MaxMatrixStatus MaxMatrixCommand::ExecuteSetting(MaxMatrix& matrix, char const* name, char const* value)
{
    if(0 == strcmp_P(name, PSTR("intensity"))) {
        MaxMatrixResult<uint16_t> number = ConvertToNumber(value, MAX7219_REG_INTENSITY_MAX_VALUE);
        if(!number.IsOk()) return number.GetError();
        matrix.SetIntensity(number.GetValue());
    } else if(0 == strcmp_P(name, PSTR("speed"))) {
        MaxMatrixResult<uint16_t> number = ConvertToNumber(value, MAXMATRIX_COMMAND_MAX_INTERVAL);
        if(!number.IsOk()) return number.GetError();
        matrix.SetShiftInterval(number.GetValue());
    } else if(0 == strcmp_P(name, PSTR("space"))) {
        MaxMatrixResult<uint16_t> number = ConvertToNumber(value, MAXMATRIX_COMMAND_MAX_BYTE);
        if(!number.IsOk()) return number.GetError();
        matrix.SetSpaceBetweenChars(number.GetValue());
    } else return MaxMatrixError::kUnknownCommand;
    return MaxMatrixError::kOk;
} /* executeSetting */


/******************************************************************************************************************************************************
  convertToNumber()
******************************************************************************************************************************************************/
/*! \brief          converts a decimal number
 *  \details        this function takes digits only, at least one, and stops counting as soon as the number is past
 *                  Maximum, so that a long run of digits cannot overflow into a small number. It reads on to the
 *                  end all the same, so that "999x" is no number rather than too large.
 *
 *  \param[in]      Text        the digits
 *  \param[in]      Maximum     the largest number taken
 *  \return         the number
 *                  MaxMatrixError::kNotANumber    the text is empty or holds anything but digits
 *                  MaxMatrixError::kOutOfRange    the number is larger than Maximum
 *****************************************************************************************************************************************************/
MaxMatrixResult<uint16_t> MaxMatrixCommand::ConvertToNumber(char const* text, uint16_t maximum)
{
    uint32_t value = 0;

    if(*text == STD_NULL_CHARACTER) return MaxMatrixError::kNotANumber;
    for(; *text != STD_NULL_CHARACTER; text++) {
        if(*text < '0' || *text > '9') return MaxMatrixError::kNotANumber;
        if(value <= maximum) value = value * 10 + (*text - '0');
    }
    if(value > maximum) return MaxMatrixError::kOutOfRange;
    return static_cast<uint16_t>(value);
} /* convertToNumber */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
