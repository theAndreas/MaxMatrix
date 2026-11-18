// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Errors: every way a call can fail, that it changes nothing, and what the
 * report and the texts say. */

#include <gtest/gtest.h>
#include <stdint.h>

#include <array>
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

TEST_F(MaxMatrixTest, OutOfRangeChangesNothing) {
  std::vector<uint8_t> const before{Bytes(buffer_)};
  int32_t const frames_before{mock::frames};

  EXPECT_EQ(matrix_.SetIntensity(max7219::kIntensityMaxValue + 1).GetError(),
            Error::kOutOfRange);
  EXPECT_EQ(Bytes(buffer_), before);
  EXPECT_EQ(mock::frames, frames_before);
}

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
TEST_F(MaxMatrixTest, EveryErrorSaysWhy) {
  struct Case {
    char const* name;
    Error (*call)(Matrix&);
    Error expected;
    char const* api;
  };
  std::array<Case, 33> const cases{{
      {"Init with a pin beyond the board",
       [](Matrix& m) {
         m.SetChipSelectPin(NUM_DIGITAL_PINS);
         Error const error{m.Init().GetError()};
         m.SetChipSelectPin(kChipSelectPin);
         return error;
       },
       Error::kOutOfRange, "Init"},
      {"GetColumn left",
       [](Matrix& m) { return m.GetColumn(X{-1}).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn right",
       [](Matrix& m) { return m.GetColumn(X{kNumberOfColumns}).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn no module",
       [](Matrix& m) {
         return m.GetColumn(Module{kNumberOfModules}, 0).GetError();
       },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn off its module",
       [](Matrix& m) {
         return m.GetColumn(Module{0}, kColumnNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetColumn"},
      {"GetRow above",
       [](Matrix& m) {
         PanelRow row;
         return m.GetRow(Y{-1}, row).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow below",
       [](Matrix& m) {
         PanelRow row;
         return m.GetRow(Y{kRowNumberOfModule}, row).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow no module",
       [](Matrix& m) {
         return m.GetRow(Module{kNumberOfModules}, 0).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow off its module",
       [](Matrix& m) {
         return m.GetRow(Module{0}, kRowNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetDot left",
       [](Matrix& m) { return m.GetDot(Point{X{-1}, Y{0}}).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot right",
       [](Matrix& m) {
         return m.GetDot(Point{X{kNumberOfColumns}, Y{0}}).GetError();
       },
       Error::kOutOfRange, "GetDot"},
      {"GetDot above",
       [](Matrix& m) { return m.GetDot(Point{X{0}, Y{-1}}).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot below",
       [](Matrix& m) {
         return m.GetDot(Point{X{0}, Y{kRowNumberOfModule}}).GetError();
       },
       Error::kOutOfRange, "GetDot"},
      {"SetFont", [](Matrix& m) { return m.SetFont(kNoFont).GetError(); },
       Error::kNotAFont, "SetFont"},
      {"SetIntensity",
       [](Matrix& m) {
         return m.SetIntensity(max7219::kIntensityMaxValue + 1).GetError();
       },
       Error::kOutOfRange, "SetIntensity"},
      {"SetChar",
       [](Matrix& m) {
         return m.SetChar(Point{X{0}, Y{0}}, '\x01').GetError();
       },
       Error::kUnknownChar, "SetChar"},
      {"SetText unknown char",
       [](Matrix& m) { return m.SetText("\x01").GetError(); },
       Error::kUnknownChar, "SetText"},
      {"SetText empty span", [](Matrix& m) { return m.SetText({}).GetError(); },
       Error::kNoText, "SetText"},
      {"SetCharWithShift",
       [](Matrix& m) { return m.SetCharWithShift('\x01').GetError(); },
       Error::kUnknownChar, "SetCharWithShift"},
      {"SetTextWithShift empty span",
       [](Matrix& m) { return m.SetTextWithShift({}).GetError(); },
       Error::kNoText, "SetTextWithShift"},
      {"SetTextWithShift empty span repeating",
       [](Matrix& m) { return m.SetTextWithShift({}, true).GetError(); },
       Error::kNoText, "SetTextWithShift"},
      {"SetCharWithShift before Init",
       [](Matrix&) {
         MatrixOf<kNumberOfModules> fresh{DataInPin{kDataInPin},
                                          ChipSelectPin{kChipSelectPin},
                                          ClockPin{kClockPin}};
         return fresh.SetCharWithShift('A').GetError();
       },
       Error::kNotSetUp, "SetCharWithShift"},
      {"SetTextWithShift before Init",
       [](Matrix&) {
         MatrixOf<kNumberOfModules> fresh{DataInPin{kDataInPin},
                                          ChipSelectPin{kChipSelectPin},
                                          ClockPin{kClockPin}};
         return fresh.SetTextWithShift("Hi").GetError();
       },
       Error::kNotSetUp, "SetTextWithShift"},
      {"SetIntensity before Init",
       [](Matrix&) {
         MatrixOf<kNumberOfModules> fresh{DataInPin{kDataInPin},
                                          ChipSelectPin{kChipSelectPin},
                                          ClockPin{kClockPin}};
         return fresh.SetIntensity(1).GetError();
       },
       Error::kNotSetUp, "SetIntensity"},
      {"SetShutdown before Init",
       [](Matrix&) {
         MatrixOf<kNumberOfModules> fresh{DataInPin{kDataInPin},
                                          ChipSelectPin{kChipSelectPin},
                                          ClockPin{kClockPin}};
         return fresh.SetShutdown(true).GetError();
       },
       Error::kNotSetUp, "SetShutdown"},
      {"SetDisplayTest before Init",
       [](Matrix&) {
         MatrixOf<kNumberOfModules> fresh{DataInPin{kDataInPin},
                                          ChipSelectPin{kChipSelectPin},
                                          ClockPin{kClockPin}};
         return fresh.SetDisplayTest(true).GetError();
       },
       Error::kNotSetUp, "SetDisplayTest"},
      {"intensity= before Init",
       [](Matrix&) {
         MatrixOf<kNumberOfModules> fresh{DataInPin{kDataInPin},
                                          ChipSelectPin{kChipSelectPin},
                                          ClockPin{kClockPin}};
         return FeedText(fresh, "intensity=5\n");
       },
       Error::kNotSetUp, "SetIntensity"},
      {"number empty", [](Matrix& m) { return FeedText(m, "space=\n"); },
       Error::kNotANumber, nullptr},
      {"number with a letter",
       [](Matrix& m) { return FeedText(m, "space=12a\n"); }, Error::kNotANumber,
       nullptr},
      {"number too large", [](Matrix& m) { return FeedText(m, "space=256\n"); },
       Error::kOutOfRange, nullptr},
      {"number too large, then a letter",
       [](Matrix& m) { return FeedText(m, "space=999x\n"); },
       Error::kNotANumber, nullptr},
      {"unknown command", [](Matrix& m) { return FeedText(m, "blink\n"); },
       Error::kUnknownCommand, nullptr},
      {"line too long",
       [](Matrix& m) {
         return FeedText(
             m,
             "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");
       },
       Error::kLineTooLong, nullptr},
  }};

  for (auto const& test_case : cases) {
    matrix_.Clear();
    int32_t const frames_before{mock::frames};
    uint8_t const intensity_before{ChipOfModule(0).intensity};
    reports.clear();
    EXPECT_EQ(test_case.call(matrix_), test_case.expected) << test_case.name;
    EXPECT_EQ(ShownPicture(matrix_),
              Picture([](int16_t, int16_t) { return false; }))
        << test_case.name;
    EXPECT_EQ(mock::frames, frames_before) << test_case.name;
    EXPECT_EQ(ChipOfModule(0).intensity, intensity_before) << test_case.name;
    EXPECT_EQ(matrix_.GetState(), State::kReady) << test_case.name;
    std::vector<Report> expected;
    if (kDevErrorDetect && (test_case.api != nullptr)) {
      expected.emplace_back(test_case.api, test_case.expected);
    }
    EXPECT_EQ(reports, expected) << test_case.name;
  }
}

/* every error has its text, as SerialControl answers with it, and a byte off
 * the enum gets one as well; the texts lie in a table of their own, so one
 * moved by a place would give every error after it the text of the next */
TEST(MaxMatrixErrorTest, EveryErrorHasItsText) {
  std::array<char const*, 9> const texts{
      {"ok", "out of range", "unknown char", "no text", "not a number",
       "unknown command", "line too long", "not a font", "not set up"}};
  ASSERT_EQ(texts.size(), kNumberOfErrors);

  for (uint8_t error{0}; error < kNumberOfErrors; error++) {
    EXPECT_EQ(Text(Status(static_cast<Error>(error)).GetErrorText()),
              texts[error])
        << kErrorNames[error];
  }
  EXPECT_EQ(Text(Result<uint8_t>(Error::kUnknownChar).GetErrorText()),
            "unknown char");
  /* a new error that kErrorNames lacks has a text of its own here, and fails */
  EXPECT_EQ(Text(Status(static_cast<Error>(kNumberOfErrors)).GetErrorText()),
            "unknown error");
}

/* a status and a result known when compiling are constants, as FN-9 asks of
 * whatever can be constexpr; neither was */
TEST(MaxMatrixErrorTest, StatusAndResultAreConstexpr) {
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
TEST(MaxMatrixErrorTest, PrintedReportNamesFunctionAndError) {
  mock::serial_output.clear();
  PrintError(F("SetText"), Error::kUnknownChar);
  EXPECT_EQ(mock::serial_output, "MaxMatrix SetText: unknown char\r\n");
}

#endif

}  // namespace
}  // namespace max_matrix
