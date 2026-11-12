// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Errors: every way a call can fail, that it changes nothing, and what the
 * report and the texts say. */

#include <string>
#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::Command;
using max_matrix::CommandState;
using max_matrix::Error;
using max_matrix::Matrix;
using max_matrix::Result;
using max_matrix::State;
using max_matrix::Status;

/* a whole line to a command of its own, and why it failed */
static Error FeedText(Matrix& matrix, std::string const& text) {
  Command command;
  TextPrint answer;
  Result<CommandState> result = CommandState::kPending;

  for (char c : text) result = command.Feed(matrix, c, answer);
  return result.GetError();
}
TEST_F(MaxMatrixTest, OutOfRangeChangesNothing) {
  std::vector<byte> before =
      Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  int32_t frames_before = mock::frames;

  EXPECT_EQ(matrix_.SetIntensity(max_matrix::max7219::kIntensityMaxValue + 1)
                .GetError(),
            Error::kOutOfRange);
  EXPECT_EQ(Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns),
            before);
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
 * char the font lacks, is reported by that function. */
TEST_F(MaxMatrixTest, EveryErrorSaysWhy) {
  struct {
    char const* name;
    Error (*call)(Matrix&);
    Error expected;
    char const* api;
  } const cases[] = {
      {"GetColumn left", [](Matrix& m) { return m.GetColumn(-1).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn right",
       [](Matrix& m) { return m.GetColumn(kNumberOfColumns).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn no module",
       [](Matrix& m) { return m.GetColumn(kNumberOfModules, 0).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn off its module",
       [](Matrix& m) {
         return m.GetColumn(0, max_matrix::kColumnNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetColumn"},
      {"GetRow above",
       [](Matrix& m) {
         PanelRow row;
         return m.GetRow(-1, row).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow below",
       [](Matrix& m) {
         PanelRow row;
         return m.GetRow(max_matrix::kRowNumberOfModule, row).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow no module",
       [](Matrix& m) { return m.GetRow(kNumberOfModules, 0).GetError(); },
       Error::kOutOfRange, "GetRow"},
      {"GetRow off its module",
       [](Matrix& m) {
         return m.GetRow(0, max_matrix::kRowNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetDot left", [](Matrix& m) { return m.GetDot(-1, 0).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot right",
       [](Matrix& m) { return m.GetDot(kNumberOfColumns, 0).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot above", [](Matrix& m) { return m.GetDot(0, -1).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot below",
       [](Matrix& m) {
         return m.GetDot(0, max_matrix::kRowNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetDot"},
      {"SetFont", [](Matrix& m) { return m.SetFont(kNoFont).GetError(); },
       Error::kNotAFont, "SetFont"},
      {"SetIntensity",
       [](Matrix& m) {
         return m.SetIntensity(max_matrix::max7219::kIntensityMaxValue + 1)
             .GetError();
       },
       Error::kOutOfRange, "SetIntensity"},
      {"SetChar", [](Matrix& m) { return m.SetChar(0, 0, '\x01').GetError(); },
       Error::kUnknownChar, "SetChar"},
      {"SetText unknown char",
       [](Matrix& m) { return m.SetText("\x01").GetError(); },
       Error::kUnknownChar, "SetText"},
      {"SetText NULL", [](Matrix& m) { return m.SetText(NULL).GetError(); },
       Error::kNoText, "SetText"},
      {"SetCharWithShift",
       [](Matrix& m) { return m.SetCharWithShift('\x01').GetError(); },
       Error::kUnknownChar, "SetCharWithShift"},
      {"SetTextWithShift NULL",
       [](Matrix& m) { return m.SetTextWithShift(NULL).GetError(); },
       Error::kNoText, "SetTextWithShift"},
      {"SetTextWithShift NULL repeating",
       [](Matrix& m) { return m.SetTextWithShift(NULL, true).GetError(); },
       Error::kNoText, "SetTextWithShift"},
      {"number empty",
       [](Matrix&) { return Command::ConvertToNumber("", 255).GetError(); },
       Error::kNotANumber, NULL},
      {"number with a letter",
       [](Matrix&) { return Command::ConvertToNumber("12a", 255).GetError(); },
       Error::kNotANumber, NULL},
      {"number too large",
       [](Matrix&) { return Command::ConvertToNumber("256", 255).GetError(); },
       Error::kOutOfRange, NULL},
      {"number too large, then a letter",
       [](Matrix&) { return Command::ConvertToNumber("999x", 255).GetError(); },
       Error::kNotANumber, NULL},
      {"unknown command", [](Matrix& m) { return FeedText(m, "blink\n"); },
       Error::kUnknownCommand, NULL},
      {"line too long",
       [](Matrix& m) {
         return FeedText(
             m,
             "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");
       },
       Error::kLineTooLong, NULL},
  };

  for (auto const& test_case : cases) {
    matrix_.Clear();
    int32_t frames_before = mock::frames;
    byte intensity_before = ChipOfModule(0).intensity;
    reports.clear();
    EXPECT_EQ(test_case.call(matrix_), test_case.expected) << test_case.name;
    EXPECT_EQ(ShownPicture(matrix_),
              Picture([](int16_t, int16_t) { return false; }))
        << test_case.name;
    EXPECT_EQ(mock::frames, frames_before) << test_case.name;
    EXPECT_EQ(ChipOfModule(0).intensity, intensity_before) << test_case.name;
    EXPECT_EQ(matrix_.GetState(), State::kReady) << test_case.name;
    std::vector<Report> expected;
    if (kDevErrorDetect && test_case.api != NULL) {
      expected.push_back(Report(test_case.api, test_case.expected));
    }
    EXPECT_EQ(reports, expected) << test_case.name;
  }
}

/* every error has its text, as SerialControl answers with it, and a byte off
 * the enum gets one as well; the texts lie in a table of their own, so one
 * moved by a place would give every error after it the text of the next */
TEST(MaxMatrixErrorTest, EveryErrorHasItsText) {
  char const* const texts[] = {
      "ok",           "out of range",    "unknown char",  "no text",
      "not a number", "unknown command", "line too long", "not a font"};
  ASSERT_EQ(sizeof(texts) / sizeof(texts[0]), kNumberOfErrors);

  for (uint8_t error = 0; error < kNumberOfErrors; error++) {
    EXPECT_EQ(Text(Status(static_cast<Error>(error)).GetErrorText()),
              texts[error])
        << kErrorNames[error];
  }
  EXPECT_EQ(Text(Result<byte>(Error::kUnknownChar).GetErrorText()),
            "unknown char");
  /* a new error that kErrorNames lacks has a text of its own here, and fails */
  EXPECT_EQ(Text(Status(static_cast<Error>(kNumberOfErrors)).GetErrorText()),
            "unknown error");
}

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/* the default report prints what the tests record instead, so the printing is a
 * function of its own and tested here */
TEST(MaxMatrixErrorTest, PrintedReportNamesFunctionAndError) {
  mock::serial_output.clear();
  max_matrix::PrintError(F("SetText"), Error::kUnknownChar);
  EXPECT_EQ(mock::serial_output, "MaxMatrix SetText: unknown char\r\n");
}
#endif
