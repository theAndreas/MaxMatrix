// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the lines the control program sends, which MaxMatrixCommand reads on
 * the board. */
#include "ControlCommand.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "MaxMatrixConfig.h"

// every command SerialControl knows, spelled as MaxMatrixCommand reads it
TEST(ControlCommandTest, MakesTheLinesOfSerialControl) {
  EXPECT_EQ(ControlCommand::MakeText("Hello", false), "text=Hello");
  EXPECT_EQ(ControlCommand::MakeText("Hello", true), "textShift=Hello");
  EXPECT_EQ(ControlCommand::MakeIntensity(15), "intensity=15");
  EXPECT_EQ(ControlCommand::MakeSpeed(500), "speed=500");
  EXPECT_EQ(ControlCommand::MakeSpace(0), "space=0");
  EXPECT_EQ(ControlCommand::MakeDot(31, 7, true), "setDot=31,7,1");
  EXPECT_EQ(ControlCommand::MakeDot(0, 0, false), "setDot=0,0,0");
  EXPECT_EQ(ControlCommand::MakeClear(), "clear");
  EXPECT_EQ(ControlCommand::MakePicture(), "picture");
}

// a line longer than the board's buffer is dropped there whole, so the program
// does not send it; the buffer counts bytes, and an umlaut takes two of them in
// UTF-8
TEST(ControlCommandTest, FitsCountsTheBytesOfTheLine) {
  std::string const longest(MAXMATRIX_COMMAND_LINE_LENGTH, 'x');

  EXPECT_TRUE(ControlCommand::Fits(longest));
  EXPECT_FALSE(ControlCommand::Fits(longest + "x"));
  EXPECT_FALSE(ControlCommand::Fits(longest.substr(1) + "\xC3\xA4"));
  EXPECT_TRUE(
      ControlCommand::Fits(ControlCommand::MakeText("Gr\xC3\xBC\xC3\x9F"
                                                    "e",
                                                    false)));
}

// what the field next to the text shows: the bytes the text may still take,
// counted as Fits() counts them
TEST(ControlCommandTest, GetBytesLeftCountsTheBytesOfTheLine) {
  std::string const longest(MAXMATRIX_COMMAND_LINE_LENGTH, 'x');

  EXPECT_EQ(ControlCommand::GetBytesLeft(""), MAXMATRIX_COMMAND_LINE_LENGTH);
  EXPECT_EQ(ControlCommand::GetBytesLeft(longest), 0);
  EXPECT_EQ(ControlCommand::GetBytesLeft(longest + "xy"), -2);
  EXPECT_EQ(
      ControlCommand::GetBytesLeft(ControlCommand::MakeText("\xC3\xA4", false)),
      MAXMATRIX_COMMAND_LINE_LENGTH - 7);
}

// a line break in a text would end the line early and send the rest as a
// command of its own
TEST(ControlCommandTest, FitsRefusesALineBreak) {
  EXPECT_FALSE(
      ControlCommand::Fits(ControlCommand::MakeText("one\nclear", false)));
  EXPECT_FALSE(ControlCommand::Fits(ControlCommand::MakeText("one\r", false)));
}

// the answer to picture, as MaxMatrixCommand prints it: "Hi" from the left edge
// and the empty columns after it
TEST(ControlCommandTest, DecodesThePicture) {
  std::vector<uint8_t> columns;
  std::vector<uint8_t> expected = {0x7F, 0x08, 0x08, 0x7F,
                                   0x00, 0x44, 0x7D, 0x40};

  expected.resize(CONTROL_NUMBER_OF_COLUMNS, 0x00);
  ASSERT_TRUE(ControlCommand::DecodePicture(
      "picture=7F08087F00447D40" +
          std::string(2 * CONTROL_NUMBER_OF_COLUMNS - 16, '0'),
      &columns));
  EXPECT_EQ(columns, expected);
  ASSERT_TRUE(ControlCommand::DecodePicture(
      "picture=ff" + std::string(2 * CONTROL_NUMBER_OF_COLUMNS - 2, '0'),
      &columns));
  EXPECT_EQ(columns[0], 0xFF);
}

// a picture of another chain, or one cut short, is still an answer to picture,
// but has no columns to draw; any other line is no answer to it
TEST(ControlCommandTest, DecodesAPictureItCannotDrawAsEmpty) {
  std::vector<uint8_t> columns = {1, 2, 3};

  EXPECT_TRUE(ControlCommand::DecodePicture(
      "picture=" + std::string(2 * CONTROL_NUMBER_OF_COLUMNS + 2, '0'),
      &columns));
  EXPECT_TRUE(columns.empty());
  EXPECT_TRUE(ControlCommand::DecodePicture(
      "picture=" + std::string(2 * CONTROL_NUMBER_OF_COLUMNS - 1, '0') + "x",
      &columns));
  EXPECT_TRUE(columns.empty());
  EXPECT_FALSE(ControlCommand::DecodePicture("ok", &columns));
  EXPECT_FALSE(
      ControlCommand::DecodePicture("error: unknown command", &columns));
}
