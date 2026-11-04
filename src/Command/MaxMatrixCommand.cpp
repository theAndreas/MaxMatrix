// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Commands for the matrix, one per line of text.
 *
 * The names are compared with strcmp_P against strings in flash: in RAM, on the Uno, they would
 * take their length again for nothing.
 */

#include "MaxMatrixCommand.h"
#include "MaxMatrix.h"

/* MaxMatrixCommand Parameter */
#define MAXMATRIX_COMMAND_LINE_END                                  '\n'
#define MAXMATRIX_COMMAND_CARRIAGE_RETURN                           '\r'
#define MAXMATRIX_COMMAND_VALUE_SEPARATOR                           '='
#define MAXMATRIX_COMMAND_LIST_SEPARATOR                            ','

/* the largest numbers the commands take: a column, row or space fits a byte, the shift interval a uint16_t */
#define MAXMATRIX_COMMAND_MAX_BYTE                                  0xFFu
#define MAXMATRIX_COMMAND_MAX_INTERVAL                              0xFFFFu

MaxMatrixCommand::MaxMatrixCommand()
{
    line_[0] = STD_NULL_CHARACTER;
    shift_text_[0] = STD_NULL_CHARACTER;
    line_length_ = 0;
    line_too_long_ = false;
}

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
}

// Splits the line at its first '=' into name and value, overwriting the '=' with the end of the
// name, and passes it on by name.
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
}

// textShift=TEXT copies the text out of the line buffer first: the shift reads it for as long as
// it runs, and the next line would overwrite it there.
MaxMatrixStatus MaxMatrixCommand::ExecuteTextShift(MaxMatrix& matrix, char const* value)
{
    /*
     * The text is part of Line, so it fits into shift_text_ as long as that is no shorter;
     * strlcpy() would check the same at run time, for 30 bytes of flash in SerialControl
     */
    static_assert(sizeof(shift_text_) >= sizeof(line_), "ShiftText has to hold any text of a line");

    if(*value == STD_NULL_CHARACTER) return MaxMatrixError::kNoText;
    strcpy(shift_text_, value); /* NOLINT(clang-analyzer-security.insecureAPI.strcpy) */
    return matrix.SetTextWithShift(shift_text_);
}

// text=TEXT clears the matrix first and sends clear and text as one picture; an empty text only
// clears it.
MaxMatrixStatus MaxMatrixCommand::ExecuteText(MaxMatrix& matrix, char const* value)
{
    MaxMatrixBatch batch(matrix);

    matrix.Clear();
    return matrix.SetText(value);
}

/*
 * setDot=COLUMN,ROW,VALUE reads three numbers separated by commas, overwriting the commas with
 * ends of text; the value is 0 or 1. The dot is checked against the panel here, because SetDot()
 * leaves a dot off the panel out without a word, and whoever typed the line should learn that it
 * did nothing.
 */
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
}

// The commands that set a number: intensity=0..15, speed=MILLISECONDS and space=COLUMNS.
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
}

/*
 * Converts a decimal number of digits only, at least one, up to `maximum`. It stops counting as
 * soon as the number is past `maximum`, so that a long run of digits cannot overflow into a small
 * number, but reads on to the end all the same, so that "999x" is no number rather than too large.
 */
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
}
