// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the lines the control program sends, which max_matrix::Command reads
 * on the board. */
#include "ControlCommand.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "CaseName.h"
#include "MaxMatrixConfig.h"

namespace {

/* a command SerialControl knows, and the line it is spelled as */
struct CommandLine {
  char const* name;
  std::string (*make)();
  char const* line;
};

constexpr std::array<CommandLine, 9> kCommandLines{{
    {"Text", [] { return ControlCommand::MakeText("Hello", false); },
     "text=Hello"},
    {"TextShift", [] { return ControlCommand::MakeText("Hello", true); },
     "textShift=Hello"},
    {"Intensity", [] { return ControlCommand::MakeIntensity(15); },
     "intensity=15"},
    {"Speed", [] { return ControlCommand::MakeSpeed(500); }, "speed=500"},
    {"Space", [] { return ControlCommand::MakeSpace(0); }, "space=0"},
    {"LitDot", [] { return ControlCommand::MakeDot(31, 7, true); },
     "setDot=31,7,1"},
    {"DarkDot", [] { return ControlCommand::MakeDot(0, 0, false); },
     "setDot=0,0,0"},
    {"Clear", [] { return ControlCommand::MakeClear(); }, "clear"},
    {"Picture", [] { return ControlCommand::MakePicture(); }, "picture"},
}};

class CommandLineTest : public ::testing::TestWithParam<CommandLine> {};

INSTANTIATE_TEST_SUITE_P(SerialControl, CommandLineTest,
                         ::testing::ValuesIn(kCommandLines),
                         CaseName<CommandLine>);

TEST_P(CommandLineTest, IsSpelledAsCommandReadsIt) {
  std::string const line{GetParam().make()};

  EXPECT_EQ(line, GetParam().line);
}

/* A line longer than the board's buffer is dropped there whole, so the program
 * does not send it; the buffer counts bytes, and an umlaut takes two of them in
 * UTF-8. */
std::string Longest() {
  return std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x');
}

TEST(ControlCommandTest, FitsALineAsLongAsTheBuffer) {
  bool const fits{ControlCommand::Fits(Longest())};

  EXPECT_TRUE(fits);
}

TEST(ControlCommandTest, RefusesALineOneByteLongerThanTheBuffer) {
  bool const fits{ControlCommand::Fits(Longest() + "x")};

  EXPECT_FALSE(fits);
}

TEST(ControlCommandTest, CountsAnUmlautAsTwoBytes) {
  bool const fits{ControlCommand::Fits(Longest().substr(1) + "\xC3\xA4")};

  EXPECT_FALSE(fits);
}

/* A line break in a text would end the line early and send the rest as a
 * command of its own. */
TEST(ControlCommandTest, RefusesANewlineInAText) {
  bool const fits{
      ControlCommand::Fits(ControlCommand::MakeText("one\nclear", false))};

  EXPECT_FALSE(fits);
}

TEST(ControlCommandTest, RefusesACarriageReturnInAText) {
  bool const fits{
      ControlCommand::Fits(ControlCommand::MakeText("one\r", false))};

  EXPECT_FALSE(fits);
}

/* What the field next to the text shows: the bytes the text may still take,
 * counted as Fits() counts them. */
TEST(ControlCommandTest, BytesLeftOfAnEmptyLineAreTheBuffer) {
  int32_t const left{ControlCommand::GetBytesLeft("")};

  EXPECT_EQ(left, MAXMATRIX_COMMAND_LINE_LENGTH);
}

TEST(ControlCommandTest, BytesLeftOfAFullLineAreNone) {
  int32_t const left{ControlCommand::GetBytesLeft(Longest())};

  EXPECT_EQ(left, 0);
}

TEST(ControlCommandTest, BytesLeftOfALineTooLongAreNegative) {
  int32_t const left{ControlCommand::GetBytesLeft(Longest() + "xy")};

  EXPECT_EQ(left, -2);
}

/* "text=" and an umlaut of two bytes */
TEST(ControlCommandTest, BytesLeftCountAnUmlautAsTwo) {
  int32_t const left{ControlCommand::GetBytesLeft(
      ControlCommand::MakeText("\xC3\xA4", false))};

  EXPECT_EQ(left, MAXMATRIX_COMMAND_LINE_LENGTH - 7);
}

/* the answer to picture, as max_matrix::Command prints it: "Hi" from the left
 * edge, 7F 08 08 7F, the space, 44 7D 40, and the empty columns after it */
TEST(ControlCommandTest, DecodesThePicture) {
  ControlPicture const hi{ControlCommand::DecodePicture(
      "picture=7F08087F00447D40"
      "000000000000000000000000000000000000000000000000")};

  ASSERT_TRUE(hi.answers_picture);
  EXPECT_EQ(hi.columns, (std::vector<uint8_t>{
                            0x7F, 0x08, 0x08, 0x7F, 0x00, 0x44, 0x7D, 0x40,  //
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  //
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}));
}

TEST(ControlCommandTest, DecodesHexDigitsInLowerCase) {
  ControlPicture const lower_case{ControlCommand::DecodePicture(
      "picture=ff"
      "00000000000000000000000000000000000000000000000000000000000000")};

  ASSERT_TRUE(lower_case.answers_picture);
  EXPECT_EQ(lower_case.columns[0], 0xFF);
}

/* A picture of another chain, or one cut short, is still an answer to picture,
 * but has no columns to draw. */
TEST(ControlCommandTest, DecodesAPictureTooLongAsEmpty) {
  ControlPicture const too_long{ControlCommand::DecodePicture(
      "picture="
      "000000000000000000000000000000000000000000000000000000000000000000")};

  EXPECT_TRUE(too_long.answers_picture);
  EXPECT_TRUE(too_long.columns.empty());
}

/* a char that is no hex digit, in the first column's high or low digit: each
 * lies just outside one of the three ranges of hex digits */
struct NoHexDigit {
  char const* name;
  char digit;
  size_t position;
};

constexpr std::array<NoHexDigit, 6> kNoHexDigits{{
    {"BelowTheDigits", '/', 0},
    {"AboveTheDigits", ':', 1},
    {"BelowTheCapitals", '@', 0},
    {"AboveTheCapitals", 'G', 1},
    {"BelowTheSmallLetters", '`', 0},
    {"AboveTheSmallLetters", 'g', 1},
}};

class NoHexDigitTest : public ::testing::TestWithParam<NoHexDigit> {};

INSTANTIATE_TEST_SUITE_P(Chars, NoHexDigitTest,
                         ::testing::ValuesIn(kNoHexDigits),
                         CaseName<NoHexDigit>);

TEST_P(NoHexDigitTest, DecodesThePictureAsEmpty) {
  std::string line{
      "picture=" +
      std::string(2u * static_cast<size_t>(kControlNumberOfColumns), '0')};
  line[8 + GetParam().position] = GetParam().digit;

  ControlPicture const no_hex{ControlCommand::DecodePicture(line)};

  EXPECT_TRUE(no_hex.answers_picture);
  EXPECT_TRUE(no_hex.columns.empty());
}

/* any other line is no answer to picture */
struct OtherLine {
  char const* name;
  char const* line;
};

constexpr std::array<OtherLine, 2> kOtherLines{{
    {"Ok", "ok"},
    {"Error", "error: unknown command"},
}};

class OtherLineTest : public ::testing::TestWithParam<OtherLine> {};

INSTANTIATE_TEST_SUITE_P(Answers, OtherLineTest,
                         ::testing::ValuesIn(kOtherLines), CaseName<OtherLine>);

TEST_P(OtherLineTest, IsNoAnswerToPicture) {
  ControlPicture const picture{ControlCommand::DecodePicture(GetParam().line)};

  EXPECT_FALSE(picture.answers_picture);
}

}  // namespace
