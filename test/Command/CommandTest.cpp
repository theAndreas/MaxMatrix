// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The lines a control page or program sends, carried out on a matrix. */

#include <gtest/gtest.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "Max7219.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class CommandTest : public MaxMatrixTest {
 protected:
  Command command_;
  TextPrint answer_;

  /* sends a line as a program does, char by char, and returns what the last
   * char brought; answer_ holds the answer to this line alone */
  Result<CommandState> Send(std::string const& line) {
    answer_.text.clear();
    return FeedLine(command_, matrix_, line, answer_);
  }
};

/* the panel dark, laid out as Display() lays it out */
std::vector<uint8_t> Dark() { return std::vector<uint8_t>(kNumberOfColumns); }

/* "Hi!" is 7F 08 08 7F, the space, 44 7D 40, the space, 5F; the "xx" before it
 * reached to the eleventh column */
TEST_F(CommandTest, TextReplacesThePanelWithItsText) {
  matrix_.SetText("xx");

  ASSERT_EQ(Send("text=Hi!\n"), CommandState::kDone);

  EXPECT_EQ(LeftEdge(matrix_, 11),
            (std::vector<uint8_t>{0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D,
                                  0x40, 0x00, 0x5F, 0x00}));
}

/* SetTextWithShift() keeps only a pointer: a shift that read its text out of
 * the line buffer would shift whatever line came next; "AB" is 7E 11 11 7E,
 * the space, 7F 49 49 36, the space */
TEST_F(CommandTest, TextShiftKeepsItsTextWhileLinesFollow) {
  ASSERT_EQ(Send("textShift=AB\n"), CommandState::kDone);
  ASSERT_EQ(Send("intensity=3\n"), CommandState::kDone);

  ShiftUntilReady(matrix_);

  EXPECT_EQ(RightEdge(matrix_, 10),
            (std::vector<uint8_t>{0x7E, 0x11, 0x11, 0x7E, 0x00, 0x7F, 0x49,
                                  0x49, 0x36, 0x00}));
}

TEST_F(CommandTest, IntensityReachesEveryChip) {
  ASSERT_EQ(Send("intensity=8\n"), CommandState::kDone);

  EXPECT_EQ(ChipValues(&Max7219::intensity),
            (std::vector<uint8_t>{8, 8, 8, 8}));
}

TEST_F(CommandTest, SpeedSetsTheShiftInterval) {
  ASSERT_EQ(Send("speed=120\n"), CommandState::kDone);

  EXPECT_EQ(matrix_.GetShiftInterval(), 120);
}

TEST_F(CommandTest, SpaceSetsTheSpaceBetweenChars) {
  ASSERT_EQ(Send("space=3\n"), CommandState::kDone);

  EXPECT_EQ(matrix_.GetSpaceBetweenChars(), 3);
}

TEST_F(CommandTest, SetDotLightsTheDot) {
  ASSERT_EQ(Send("setDot=5,3,1\n"), CommandState::kDone);

  EXPECT_TRUE(matrix_.GetDot(Point{X{5}, Y{3}}).GetValue());
}

TEST_F(CommandTest, SetDotOfZeroDarkensTheDot) {
  matrix_.SetDot(Point{X{5}, Y{3}}, true);

  ASSERT_EQ(Send("setDot=5,3,0\n"), CommandState::kDone);

  EXPECT_FALSE(matrix_.GetDot(Point{X{5}, Y{3}}).GetValue());
}

TEST_F(CommandTest, ClearDarkensThePanel) {
  matrix_.SetText("Hi");

  ASSERT_EQ(Send("clear\n"), CommandState::kDone);

  EXPECT_EQ(Display(), Dark());
}

/* a line the matrix refuses, and why */
struct RefusedLine {
  char const* name;
  char const* line;
  Error error;
};

constexpr std::array<RefusedLine, 16> kRefusedLines{{
    {"UnknownCommand", "blink\n", Error::kUnknownCommand},
    {"ClearWithAValue", "clear=1\n", Error::kUnknownCommand},
    {"IntensityAbove15", "intensity=16\n", Error::kOutOfRange},
    {"IntensityWithoutAValue", "intensity=\n", Error::kNotANumber},
    {"IntensityOfALetter", "intensity=x\n", Error::kNotANumber},
    {"SpeedAbove65535", "speed=65536\n", Error::kOutOfRange},
    {"SpeedAboveEveryCount", "speed=99999999999999999999\n",
     Error::kOutOfRange},
    /* 2^32, which a count in 32 bits wraps round to 0, a speed it would take */
    {"SpeedOf2To32", "speed=4294967296\n", Error::kOutOfRange},
    {"NegativeSpace", "space=-1\n", Error::kNotANumber},
    {"SetDotOfOneNumber", "setDot=5\n", Error::kNotANumber},
    {"SetDotOfTwoNumbers", "setDot=1,2\n", Error::kNotANumber},
    {"SetDotRightOfThePanel", "setDot=32,0,1\n", Error::kOutOfRange},
    {"SetDotBelowThePanel", "setDot=1,8,1\n", Error::kOutOfRange},
    {"SetDotOfValue2", "setDot=1,2,2\n", Error::kOutOfRange},
    {"TextShiftWithoutText", "textShift=\n", Error::kNoText},
    {"TextWithAnUnknownChar", "text=a\x01\n", Error::kUnknownChar},
}};

class RefusedLineTest : public CommandTest,
                        public ::testing::WithParamInterface<RefusedLine> {};

INSTANTIATE_TEST_SUITE_P(Lines, RefusedLineTest,
                         ::testing::ValuesIn(kRefusedLines),
                         CaseName<RefusedLine>);

TEST_P(RefusedLineTest, IsRefusedWithItsError) {
  Result<CommandState> const result{Send(GetParam().line)};

  EXPECT_EQ(result, GetParam().error);
}

TEST_F(CommandTest, SpeedOutOfRangeLeavesTheShiftInterval) {
  Send("speed=65536\n");

  EXPECT_EQ(matrix_.GetShiftInterval(), 0);
}

TEST_F(CommandTest, LineEndsInCarriageReturnNewline) {
  ASSERT_EQ(Send("intensity=4\r\n"), CommandState::kDone);

  EXPECT_EQ(ChipValues(&Max7219::intensity),
            (std::vector<uint8_t>{4, 4, 4, 4}));
}

TEST_F(CommandTest, EmptyLineLeavesItPending) {
  Result<CommandState> const result{Send("\n")};

  EXPECT_EQ(result, CommandState::kPending);
}

/* a line is carried out at its end, and not before: the chips keep the 5 Init()
 * gave them */
TEST_F(CommandTest, LineWithoutItsEndIsPending) {
  Result<CommandState> const result{Send("intensity=8")};

  EXPECT_EQ(result, CommandState::kPending);
  EXPECT_EQ(ChipValues(&Max7219::intensity),
            (std::vector<uint8_t>{5, 5, 5, 5}));
}

TEST_F(CommandTest, LineWithoutItsEndIsNotAnswered) {
  Send("intensity=8");

  EXPECT_EQ(answer_.text, "");
}

/* a line cut short would carry out a different command, so a line too long is
 * dropped whole; the next one is read */
TEST_F(CommandTest, LineTooLongDrawsNothing) {
  Send("text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");

  EXPECT_EQ(Display(), Dark());
}

TEST_F(CommandTest, LineAfterALineTooLongIsCarriedOut) {
  Send("text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");

  ASSERT_EQ(Send("intensity=2\n"), CommandState::kDone);

  EXPECT_EQ(ChipValues(&Max7219::intensity),
            (std::vector<uint8_t>{2, 2, 2, 2}));
}

/* a line and the one line back a program pairs it with; an empty line has
 * none */
struct AnsweredLine {
  char const* name;
  char const* line;
  char const* answer;
};

constexpr std::array<AnsweredLine, 6> kAnsweredLines{{
    {"Done", "intensity=8\n", "ok\r\n"},
    {"DoneAfterCarriageReturn", "clear\r\n", "ok\r\n"},
    {"OutOfRange", "intensity=16\n", "error: out of range\r\n"},
    {"UnknownCommand", "blink\n", "error: unknown command\r\n"},
    {"Empty", "\n", ""},
    {"EmptyAfterCarriageReturn", "\r\n", ""},
}};

class AnsweredLineTest : public CommandTest,
                         public ::testing::WithParamInterface<AnsweredLine> {};

INSTANTIATE_TEST_SUITE_P(Lines, AnsweredLineTest,
                         ::testing::ValuesIn(kAnsweredLines),
                         CaseName<AnsweredLine>);

TEST_P(AnsweredLineTest, IsAnsweredOnce) {
  Send(GetParam().line);

  EXPECT_EQ(answer_.text, GetParam().answer);
}

TEST_F(CommandTest, LineTooLongIsAnsweredWithItsError) {
  Send("text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");

  EXPECT_EQ(answer_.text, "error: line too long\r\n");
}

/* picture answers in place of ok, with two hex digits for every column: "Hi" is
 * 7F 08 08 7F, the space, 44 7D 40 */
TEST_F(CommandTest, PictureIsAnsweredWithTheColumns) {
  matrix_.SetText("Hi");

  Send("picture\n");

  EXPECT_EQ(answer_.text,
            "picture=7F08087F00447D40"
            "000000000000000000000000000000000000000000000000\r\n");
}

TEST_F(CommandTest, PictureIsAnswered) {
  Result<CommandState> const result{Send("picture\n")};

  EXPECT_EQ(result, CommandState::kAnswered);
}

TEST_F(CommandTest, PictureChangesNothing) {
  matrix_.SetText("Hi");

  Send("picture\n");

  EXPECT_EQ(Display(), (std::vector<uint8_t>{
                           0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D, 0x40,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                           0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST_F(CommandTest, PictureWithAValueIsUnknown) {
  Result<CommandState> const result{Send("picture=1\n")};

  EXPECT_EQ(result, Error::kUnknownCommand);
}

/* MaxMatrixMappingTest is the facade in every mapping; the tests of the panel
 * instantiate it in their binary as well */
INSTANTIATE_TEST_SUITE_P(
    AllMappings, MaxMatrixMappingTest,
    ::testing::Combine(::testing::ValuesIn(kMatrixOrientations),
                       ::testing::ValuesIn(kModuleTypes)),
    MappingPairName);

/* picture answers with the panel as the viewer sees it, so that a program draws
 * it without knowing the orientations; one dot in a column of every module,
 * each in another row: columns 0, 9, 16 and 31 are 01, 02, 10 and 80 */
TEST_P(MaxMatrixMappingTest, PictureAnswerIsWhatTheViewerSees) {
  Command command;
  TextPrint answer;
  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  matrix_.SetDot(Point{X{9}, Y{1}}, true);
  matrix_.SetDot(Point{X{16}, Y{4}}, true);
  matrix_.SetDot(Point{X{31}, Y{7}}, true);

  FeedLine(command, matrix_, "picture\n", answer);

  EXPECT_EQ(answer.text,
            "picture="
            "0100000000000000"
            "0002000000000000"
            "1000000000000000"
            "0000000000000080\r\n");
}

TEST_F(CommandTest, LineOfFullLengthIsTaken) {
  std::string const line{
      "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH - 5, 'x') + "\n"};

  Result<CommandState> const result{Send(line)};

  EXPECT_EQ(result, CommandState::kDone);
}

TEST_F(CommandTest, LineOneCharTooLongIsRefused) {
  std::string const line{
      "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH - 4, 'x') + "\n"};

  Result<CommandState> const result{Send(line)};

  EXPECT_EQ(result, Error::kLineTooLong);
}

TEST_F(CommandTest, IntensityTakesItsGreatestValue) {
  ASSERT_EQ(Send("intensity=15\n"), CommandState::kDone);

  EXPECT_EQ(ChipOfModule(0).intensity, 15);
}

TEST_F(CommandTest, SpaceTakesItsGreatestValue) {
  ASSERT_EQ(Send("space=255\n"), CommandState::kDone);

  EXPECT_EQ(matrix_.GetSpaceBetweenChars(), 255);
}

TEST_F(CommandTest, SpeedTakesItsGreatestValue) {
  ASSERT_EQ(Send("speed=65535\n"), CommandState::kDone);

  EXPECT_EQ(matrix_.GetShiftInterval(), 65535);
}

TEST_F(CommandTest, SetDotTakesTheLastDot) {
  ASSERT_EQ(Send("setDot=31,7,1\n"), CommandState::kDone);

  EXPECT_TRUE(matrix_.GetDot(Point{X{31}, Y{7}}).GetValue());
}

}  // namespace
}  // namespace max_matrix
