// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The lines a control page or program sends, carried out on a matrix. */

#include <array>
#include <string>
#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::Command;
using max_matrix::CommandState;
using max_matrix::Error;
using max_matrix::Result;
using max_matrix::State;

namespace {

class MaxMatrixCommandTest : public MaxMatrixTest {
 protected:
  Command command_;
  TextPrint answer_;

  /* feeds a line char by char, as Serial would; every char but the last has to
   * leave the line pending, and answer nothing */
  Result<CommandState> FeedLine(std::string const& text) {
    answer_.text.clear();
    for (size_t i = 0; i + 1 < text.size(); i++) {
      EXPECT_EQ(command_.Feed(matrix_, text[i], answer_),
                CommandState::kPending)
          << "char " << i << " of " << text;
    }
    EXPECT_EQ(answer_.text, "") << "answered before the end of " << text;
    return command_.Feed(matrix_, text.back(), answer_);
  }

  /* the display after every step of the shift running now, until it is done */
  std::vector<std::vector<byte>> ShiftSteps() {
    std::vector<std::vector<byte>> steps;
    for (uint16_t step = 0; step < 1000 && matrix_.GetState() != State::kReady;
         step++) {
      matrix_.ShiftTask();
      steps.push_back(Display());
    }
    return steps;
  }
};

TEST_F(MaxMatrixCommandTest, TextShowsWhatSetTextShows) {
  matrix_.SetText("xx");
  ASSERT_EQ(FeedLine("text=Hi!\n"), CommandState::kDone);
  std::vector<byte> const shown = Display();

  matrix_.Clear();
  matrix_.SetText("Hi!");
  EXPECT_EQ(shown, Display());
}

TEST_F(MaxMatrixCommandTest, SettingsAndDotsReachTheMatrix) {
  EXPECT_EQ(FeedLine("intensity=8\n"), CommandState::kDone);
  EXPECT_EQ(ChipOfModule(0).intensity, 8);
  EXPECT_EQ(FeedLine("speed=120\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetShiftInterval(), 120);
  EXPECT_EQ(FeedLine("space=3\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetSpaceBetweenChars(), 3);

  EXPECT_EQ(FeedLine("setDot=5,3,1\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetDot(5, 3).GetError(), Error::kOk);
  EXPECT_TRUE(matrix_.GetDot(5, 3).GetValue());
  EXPECT_EQ(FeedLine("setDot=5,3,0\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetDot(5, 3).GetError(), Error::kOk);
  EXPECT_FALSE(matrix_.GetDot(5, 3).GetValue());

  matrix_.SetText("Hi");
  EXPECT_EQ(FeedLine("clear\n"), CommandState::kDone);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));
}

/* SetTextWithShift() keeps only a pointer: a shift that read its text out of
 * the line buffer would shift whatever line came next */
TEST_F(MaxMatrixCommandTest, TextShiftKeepsItsTextWhileLinesFollow) {
  char const text[] = "AB";

  ASSERT_EQ(FeedLine("textShift=AB\n"), CommandState::kDone);
  ASSERT_EQ(FeedLine("intensity=3\n"), CommandState::kDone);
  std::vector<std::vector<byte>> const commanded = ShiftSteps();

  matrix_.Clear();
  ASSERT_EQ(matrix_.SetTextWithShift(text).GetError(), Error::kOk);
  EXPECT_EQ(commanded, ShiftSteps());
}

TEST_F(MaxMatrixCommandTest, WrongLinesAreRefused) {
  struct Refused {
    char const* line;
    Error error;
  };
  std::array<Refused, 15> const cases{{
      {"blink\n", Error::kUnknownCommand},
      {"clear=1\n", Error::kUnknownCommand},
      {"intensity=16\n", Error::kOutOfRange},
      {"intensity=\n", Error::kNotANumber},
      {"intensity=x\n", Error::kNotANumber},
      {"speed=65536\n", Error::kOutOfRange},
      {"speed=99999999999999999999\n", Error::kOutOfRange},
      /* 2^32, which a count in 32 bits wraps round to 0, a speed it would take
       */
      {"speed=4294967296\n", Error::kOutOfRange},
      {"space=-1\n", Error::kNotANumber},
      {"setDot=1,2\n", Error::kNotANumber},
      {"setDot=32,0,1\n", Error::kOutOfRange},
      {"setDot=1,8,1\n", Error::kOutOfRange},
      {"setDot=1,2,2\n", Error::kOutOfRange},
      {"textShift=\n", Error::kNoText},
      {"text=a\x01\n", Error::kUnknownChar},
  }};

  for (auto const& test_case : cases) {
    EXPECT_EQ(FeedLine(test_case.line), test_case.error) << test_case.line;
  }
  EXPECT_EQ(matrix_.GetShiftInterval(), 0) << "changed by a refused speed=";
}

TEST_F(MaxMatrixCommandTest, LineEndsInNewlineOrCarriageReturnNewline) {
  EXPECT_EQ(FeedLine("intensity=4\r\n"), CommandState::kDone);
  EXPECT_EQ(ChipOfModule(0).intensity, 4);
  EXPECT_EQ(FeedLine("\n"), CommandState::kPending) << "an empty line";
  EXPECT_EQ(FeedLine("intensity=5\n"), CommandState::kDone);
}

/* a line cut short would carry out a different command, so a line too long is
 * dropped whole; the next one is read */
TEST_F(MaxMatrixCommandTest, LineTooLongIsDroppedWhole) {
  std::string const long_text =
      "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n";

  EXPECT_EQ(FeedLine(long_text), Error::kLineTooLong);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));
  EXPECT_EQ(FeedLine("intensity=2\n"), CommandState::kDone);
  EXPECT_EQ(ChipOfModule(0).intensity, 2);
}

/* a program pairs every line it sent with one line back, so each line is
 * answered once, an empty line not at all */
TEST_F(MaxMatrixCommandTest, EveryLineIsAnsweredOnce) {
  struct Answered {
    char const* line;
    char const* answer;
  };
  std::array<Answered, 6> const cases{{
      {"intensity=8\n", "ok\r\n"},
      {"clear\r\n", "ok\r\n"},
      {"intensity=16\n", "error: out of range\r\n"},
      {"blink\n", "error: unknown command\r\n"},
      {"\n", ""},
      {"\r\n", ""},
  }};

  for (auto const& test_case : cases) {
    FeedLine(test_case.line);
    EXPECT_EQ(answer_.text, test_case.answer) << test_case.line;
  }
  FeedLine("text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");
  EXPECT_EQ(answer_.text, "error: line too long\r\n");
}

/* picture answers in place of ok, and changes nothing */
TEST_F(MaxMatrixCommandTest, PictureIsAnsweredWithTheColumns) {
  matrix_.SetText("Hi");
  std::vector<byte> const shown = Display();

  EXPECT_EQ(FeedLine("picture\n"), CommandState::kAnswered);
  ASSERT_EQ(answer_.text.size(), 8 + (2 * kNumberOfColumns) + 2);
  EXPECT_EQ(answer_.text.substr(0, 8), "picture=");
  EXPECT_EQ(answer_.text.substr(answer_.text.size() - 2), "\r\n");
  EXPECT_EQ(Display(), shown);
  EXPECT_EQ(FeedLine("picture=1\n"), Error::kUnknownCommand);
}

TEST_F(MaxMatrixCommandTest, LineOfFullLengthIsTaken) {
  std::string const text =
      "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH - 5, 'x') + "\n";

  EXPECT_EQ(FeedLine(text), CommandState::kDone);
}

/* picture answers with the panel as the viewer sees it, so that a program draws
 * it without knowing the orientations; one dot in a column of every module,
 * each in another row */
TEST_P(MaxMatrixMappingTest, PictureAnswerIsWhatTheViewerSees) {
  Command command;
  TextPrint answer;
  std::string expected = "picture=";

  matrix_.SetDot(0, 0, true);
  matrix_.SetDot(9, 1, true);
  matrix_.SetDot(16, 4, true);
  matrix_.SetDot(31, 7, true);
  for (char const c : std::string("picture\n")) {
    command.Feed(matrix_, c, answer);
  }
  std::vector<std::string> columns(kNumberOfColumns, "00");
  columns[0] = "01";
  columns[9] = "02";
  columns[16] = "10";
  columns[31] = "80";
  for (std::string const& column : columns) {
    expected += column;
  }
  EXPECT_EQ(answer.text, expected + "\r\n");
}

}  // namespace
