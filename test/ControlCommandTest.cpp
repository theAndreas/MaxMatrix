/* Tests of the lines the control program sends, which MaxMatrixCommand reads on the board. */
#include <gtest/gtest.h>
#include <string>
#include "ControlCommand.h"
#include "MaxMatrixConfig.h"

// every command SerialControl knows, spelled as MaxMatrixCommand reads it
TEST(ControlCommandTest, MakesTheLinesOfSerialControl)
{
    EXPECT_EQ(ControlCommand::MakeText("Hello", false), "text=Hello");
    EXPECT_EQ(ControlCommand::MakeText("Hello", true), "textShift=Hello");
    EXPECT_EQ(ControlCommand::MakeIntensity(15), "intensity=15");
    EXPECT_EQ(ControlCommand::MakeSpeed(500), "speed=500");
    EXPECT_EQ(ControlCommand::MakeSpace(0), "space=0");
    EXPECT_EQ(ControlCommand::MakeDot(31, 7, true), "setDot=31,7,1");
    EXPECT_EQ(ControlCommand::MakeDot(0, 0, false), "setDot=0,0,0");
    EXPECT_EQ(ControlCommand::MakeClear(), "clear");
}

// a line longer than the board's buffer is dropped there whole, so the program does not send it; the buffer counts
// bytes, and an umlaut takes two of them in UTF-8
TEST(ControlCommandTest, FitsCountsTheBytesOfTheLine)
{
    std::string const longest(MAXMATRIX_COMMAND_LINE_LENGTH, 'x');

    EXPECT_TRUE(ControlCommand::Fits(longest));
    EXPECT_FALSE(ControlCommand::Fits(longest + "x"));
    EXPECT_FALSE(ControlCommand::Fits(longest.substr(1) + "\xC3\xA4"));
    EXPECT_TRUE(ControlCommand::Fits(ControlCommand::MakeText("Gr\xC3\xBC\xC3\x9F" "e", false)));
}

// a line break in a text would end the line early and send the rest as a command of its own
TEST(ControlCommandTest, FitsRefusesALineBreak)
{
    EXPECT_FALSE(ControlCommand::Fits(ControlCommand::MakeText("one\nclear", false)));
    EXPECT_FALSE(ControlCommand::Fits(ControlCommand::MakeText("one\r", false)));
}
