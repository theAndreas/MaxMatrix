// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Errors: every way a call can fail, that it changes nothing, and what the
 * report and the texts say. */

#include <gtest/gtest.h>
#include <stdint.h>

#include <array>
#include <ostream>
#include <string>
#include <vector>

#include "Arduino.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* a whole line to a command of its own, and why it failed */
Error FeedText(Matrix& matrix, std::string const& text) {
  Command command;
  TextPrint answer;
  Result<CommandState> result{CommandState::kPending};

  for (char const c : text) {
    result = command.Feed(matrix, c, answer);
  }
  return result.GetError();
}

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class MaxMatrixErrorTest : public MaxMatrixTest {};

TEST_F(MaxMatrixErrorTest, OutOfRangeChangesNothing) {
  int32_t const frames_before{mock::frames};

  Error const error{
      matrix_.SetIntensity(max7219::kIntensityMaxValue + 1).GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(Bytes(buffer_), std::vector<uint8_t>(kNumberOfColumns, 0));
  EXPECT_EQ(mock::frames, frames_before);
}

/* a way a call can fail, with the reason it gives and the function of Matrix
 * that reports it, if any */
struct ErrorCase {
  char const* name;
  Error (*call)(Matrix&);
  Error expected;
  char const* api;
};

void PrintTo(ErrorCase const& error_case, std::ostream* out) {
  *out << error_case.name;
}

std::string ErrorCaseName(::testing::TestParamInfo<ErrorCase> const& info) {
  return info.param.name;
}

/* what MAXMATRIX_DEV_ERROR_DETECT reports of a case: its function once, or
 * nothing without the switch */
std::vector<Report> ReportsOf(ErrorCase const& error_case) {
  std::vector<Report> expected;
  if (kDevErrorDetect && (error_case.api != nullptr)) {
    expected.emplace_back(error_case.api, error_case.expected);
  }
  return expected;
}

class ErrorCaseTest : public MaxMatrixTest,
                      public ::testing::WithParamInterface<ErrorCase> {};

/* every way a call can fail, with the reason it gives: with stdReturnType a
 * test stayed green when another check than the one meant failed. A failing
 * call changes nothing, draws nothing and starts no shift. "999x" is no number
 * rather than too large, although the digits run past the maximum before the
 * letter comes. With MAXMATRIX_DEV_ERROR_DETECT, the function of MaxMatrix that
 * failed reports it once, under its own name. The command reports nothing of
 * its own, as a wrong line is no mistake of the sketch and its answer says what
 * was wrong; only a line that fails in a function of Matrix, text= with a
 * char the font lacks or intensity= before Init(), is reported by that
 * function. */
TEST_P(ErrorCaseTest, FailsWithItsReasonAndChangesNothing) {
  int32_t const frames_before{mock::frames};

  Error const error{GetParam().call(matrix_)};

  EXPECT_EQ(error, GetParam().expected);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }));
  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(ChipOfModule(0).intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  EXPECT_EQ(reports, ReportsOf(GetParam()));
}

INSTANTIATE_TEST_SUITE_P(
    EveryError, ErrorCaseTest,
    ::testing::Values(
        ErrorCase{"InitWithAPinBeyondTheBoard",
                  [](Matrix& m) {
                    m.SetChipSelectPin(NUM_DIGITAL_PINS);
                    Error const error{m.Init().GetError()};
                    m.SetChipSelectPin(kChipSelectPin);
                    return error;
                  },
                  Error::kOutOfRange, "Init"},
        ErrorCase{"InitWithABufferThatIsNoChain",
                  [](Matrix&) {
                    Matrix fresh{DataInPin{kDataInPin},
                                 ChipSelectPin{kChipSelectPin},
                                 ClockPin{kClockPin}, span<Byte>{}};
                    return fresh.Init().GetError();
                  },
                  Error::kOutOfRange, "Init"},
        ErrorCase{"GetColumnLeft",
                  [](Matrix& m) { return m.GetColumn(X{-1}).GetError(); },
                  Error::kOutOfRange, "GetColumn"},
        ErrorCase{"GetColumnRight",
                  [](Matrix& m) {
                    return m.GetColumn(X{kNumberOfColumns}).GetError();
                  },
                  Error::kOutOfRange, "GetColumn"},
        ErrorCase{"GetColumnNoModule",
                  [](Matrix& m) {
                    return m.GetColumn(Module{kNumberOfModules}, 0).GetError();
                  },
                  Error::kOutOfRange, "GetColumn"},
        ErrorCase{
            "GetColumnOffItsModule",
            [](Matrix& m) {
              return m.GetColumn(Module{0}, kColumnNumberOfModule).GetError();
            },
            Error::kOutOfRange, "GetColumn"},
        ErrorCase{"GetRowAbove",
                  [](Matrix& m) {
                    PanelRow row;
                    return m.GetRow(Y{-1}, row).GetError();
                  },
                  Error::kOutOfRange, "GetRow"},
        ErrorCase{"GetRowBelow",
                  [](Matrix& m) {
                    PanelRow row;
                    return m.GetRow(Y{kRowNumberOfModule}, row).GetError();
                  },
                  Error::kOutOfRange, "GetRow"},
        ErrorCase{"GetRowNoModule",
                  [](Matrix& m) {
                    return m.GetRow(Module{kNumberOfModules}, 0).GetError();
                  },
                  Error::kOutOfRange, "GetRow"},
        ErrorCase{"GetRowOffItsModule",
                  [](Matrix& m) {
                    return m.GetRow(Module{0}, kRowNumberOfModule).GetError();
                  },
                  Error::kOutOfRange, "GetRow"},
        ErrorCase{
            "GetDotLeft",
            [](Matrix& m) { return m.GetDot(Point{X{-1}, Y{0}}).GetError(); },
            Error::kOutOfRange, "GetDot"},
        ErrorCase{
            "GetDotRight",
            [](Matrix& m) {
              return m.GetDot(Point{X{kNumberOfColumns}, Y{0}}).GetError();
            },
            Error::kOutOfRange, "GetDot"},
        ErrorCase{
            "GetDotAbove",
            [](Matrix& m) { return m.GetDot(Point{X{0}, Y{-1}}).GetError(); },
            Error::kOutOfRange, "GetDot"},
        ErrorCase{
            "GetDotBelow",
            [](Matrix& m) {
              return m.GetDot(Point{X{0}, Y{kRowNumberOfModule}}).GetError();
            },
            Error::kOutOfRange, "GetDot"},
        ErrorCase{"SetFont",
                  [](Matrix& m) { return m.SetFont(kNoFont).GetError(); },
                  Error::kNotAFont, "SetFont"},
        ErrorCase{
            "SetIntensity",
            [](Matrix& m) {
              return m.SetIntensity(max7219::kIntensityMaxValue + 1).GetError();
            },
            Error::kOutOfRange, "SetIntensity"},
        ErrorCase{"SetChar",
                  [](Matrix& m) {
                    return m.SetChar(Point{X{0}, Y{0}}, '\x01').GetError();
                  },
                  Error::kUnknownChar, "SetChar"},
        ErrorCase{"SetTextUnknownChar",
                  [](Matrix& m) { return m.SetText("\x01").GetError(); },
                  Error::kUnknownChar, "SetText"},
        ErrorCase{"SetTextEmptySpan",
                  [](Matrix& m) { return m.SetText({}).GetError(); },
                  Error::kNoText, "SetText"},
        ErrorCase{
            "SetCharWithShift",
            [](Matrix& m) { return m.SetCharWithShift('\x01').GetError(); },
            Error::kUnknownChar, "SetCharWithShift"},
        ErrorCase{"SetTextWithShiftEmptySpan",
                  [](Matrix& m) { return m.SetTextWithShift({}).GetError(); },
                  Error::kNoText, "SetTextWithShift"},
        ErrorCase{
            "SetTextWithShiftEmptySpanRepeating",
            [](Matrix& m) { return m.SetTextWithShift({}, true).GetError(); },
            Error::kNoText, "SetTextWithShift"},
        ErrorCase{"SetCharWithShiftBeforeInit",
                  [](Matrix&) {
                    MatrixOf<kNumberOfModules> fresh{
                        DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}};
                    return fresh.SetCharWithShift('A').GetError();
                  },
                  Error::kNotSetUp, "SetCharWithShift"},
        ErrorCase{"SetTextWithShiftBeforeInit",
                  [](Matrix&) {
                    MatrixOf<kNumberOfModules> fresh{
                        DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}};
                    return fresh.SetTextWithShift("Hi").GetError();
                  },
                  Error::kNotSetUp, "SetTextWithShift"},
        ErrorCase{"SetIntensityBeforeInit",
                  [](Matrix&) {
                    MatrixOf<kNumberOfModules> fresh{
                        DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}};
                    return fresh.SetIntensity(1).GetError();
                  },
                  Error::kNotSetUp, "SetIntensity"},
        ErrorCase{"SetShutdownBeforeInit",
                  [](Matrix&) {
                    MatrixOf<kNumberOfModules> fresh{
                        DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}};
                    return fresh.SetShutdown(true).GetError();
                  },
                  Error::kNotSetUp, "SetShutdown"},
        ErrorCase{"SetDisplayTestBeforeInit",
                  [](Matrix&) {
                    MatrixOf<kNumberOfModules> fresh{
                        DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}};
                    return fresh.SetDisplayTest(true).GetError();
                  },
                  Error::kNotSetUp, "SetDisplayTest"},
        ErrorCase{"IntensityLineBeforeInit",
                  [](Matrix&) {
                    MatrixOf<kNumberOfModules> fresh{
                        DataInPin{kDataInPin}, ChipSelectPin{kChipSelectPin},
                        ClockPin{kClockPin}};
                    return FeedText(fresh, "intensity=5\n");
                  },
                  Error::kNotSetUp, "SetIntensity"},
        ErrorCase{"NumberEmpty",
                  [](Matrix& m) { return FeedText(m, "space=\n"); },
                  Error::kNotANumber, nullptr},
        ErrorCase{"NumberWithALetter",
                  [](Matrix& m) { return FeedText(m, "space=12a\n"); },
                  Error::kNotANumber, nullptr},
        ErrorCase{"NumberTooLarge",
                  [](Matrix& m) { return FeedText(m, "space=256\n"); },
                  Error::kOutOfRange, nullptr},
        ErrorCase{"NumberTooLargeThenALetter",
                  [](Matrix& m) { return FeedText(m, "space=999x\n"); },
                  Error::kNotANumber, nullptr},
        ErrorCase{"UnknownCommand",
                  [](Matrix& m) { return FeedText(m, "blink\n"); },
                  Error::kUnknownCommand, nullptr},
        ErrorCase{"LineTooLong",
                  [](Matrix& m) {
                    return FeedText(
                        m, "text=" +
                               std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') +
                               "\n");
                  },
                  Error::kLineTooLong, nullptr}),
    ErrorCaseName);

/* an error and its text, as SerialControl answers with it */
struct TextCase {
  char const* name;
  Error error;
  char const* text;
};

void PrintTo(TextCase const& text_case, std::ostream* out) {
  *out << text_case.name;
}

std::string TextCaseName(::testing::TestParamInfo<TextCase> const& info) {
  return info.param.name;
}

constexpr std::array<TextCase, 9> kTextCases{{
    {"Ok", Error::kOk, "ok"},
    {"OutOfRange", Error::kOutOfRange, "out of range"},
    {"UnknownChar", Error::kUnknownChar, "unknown char"},
    {"NoText", Error::kNoText, "no text"},
    {"NotANumber", Error::kNotANumber, "not a number"},
    {"UnknownCommand", Error::kUnknownCommand, "unknown command"},
    {"LineTooLong", Error::kLineTooLong, "line too long"},
    {"NotAFont", Error::kNotAFont, "not a font"},
    {"NotSetUp", Error::kNotSetUp, "not set up"},
}};
// a new error that kErrorNames has needs its text here as well
static_assert(kTextCases.size() == kNumberOfErrors, "every error has a case");

class ErrorTextTest : public ::testing::TestWithParam<TextCase> {};

/* every error has its text; the texts lie in a table of their own, so one moved
 * by a place would give every error after it the text of the next */
TEST_P(ErrorTextTest, ErrorHasItsText) {
  Status const status{GetParam().error};

  std::string const text{Text(status.GetErrorText())};

  EXPECT_EQ(text, GetParam().text);
}

INSTANTIATE_TEST_SUITE_P(EveryError, ErrorTextTest,
                         ::testing::ValuesIn(kTextCases), TextCaseName);

TEST_F(MaxMatrixErrorTest, ResultHasTheTextOfItsError) {
  Result<uint8_t> const result{Error::kUnknownChar};

  std::string const text{Text(result.GetErrorText())};

  EXPECT_EQ(text, "unknown char");
}

/* a byte off the enum, as a new error without its text in the table, gets a
 * text of its own */
TEST_F(MaxMatrixErrorTest, ErrorOffTheEnumHasATextToo) {
  // TS-25: the byte off the enum is the argument outside the contract this test
  // is of.
  // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
  Status const status{static_cast<Error>(kNumberOfErrors)};

  std::string const text{Text(status.GetErrorText())};

  EXPECT_EQ(text, "unknown error");
}

/* a status and a result known when compiling are constants, as FN-9 asks of
 * whatever can be constexpr; neither was */
TEST_F(MaxMatrixErrorTest, StatusAndResultAreConstexpr) {
  constexpr Status kFailed{Error::kOutOfRange};
  constexpr Result<uint8_t> kValue{uint8_t{7}};
  constexpr Result<uint8_t> kNone{Error::kUnknownChar};

  static_assert(!kFailed.IsOk(), "a status of an error is no success");
  static_assert(kFailed.GetError() == Error::kOutOfRange, "it keeps its error");
  static_assert(kFailed == Error::kOutOfRange, "it compares with its error");
  static_assert(kFailed != Error::kOk, "and with another");
  static_assert(Status{}.IsOk(), "a status without an error is a success");
  static_assert(kValue.IsOk(), "a result of a value is a success");
  static_assert(kValue.GetValue() == 7, "it keeps its value");
  static_assert(kValue.GetError() == Error::kOk, "and no error");
  static_assert(kNone == Error::kUnknownChar, "a result of an error keeps it");
  static_assert(kNone != Error::kOk, "and is no success");
  static_assert(kNone.GetValue() == 0, "its value is 0");
}

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/* the default report prints what the tests record instead, so the printing is a
 * function of its own and tested here */
TEST_F(MaxMatrixErrorTest, PrintedReportNamesFunctionAndError) {
  mock::serial_output.clear();

  PrintError(F("SetText"), Error::kUnknownChar);

  EXPECT_EQ(mock::serial_output, "MaxMatrix SetText: unknown char\r\n");
}

#endif

}  // namespace
}  // namespace max_matrix
