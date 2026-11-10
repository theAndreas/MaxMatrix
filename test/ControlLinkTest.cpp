// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the link between the control program and SerialControl, against a
 * port that is a string each way. */
#include "ControlLink.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "ControlCommand.h"

// A port whose other end is the test: what the link writes collects in
// `written`, what the test puts into `incoming` arrives at the next Read(), and
// `gone` ends the connection as a pulled cable would.
class FakePort : public ControlPort {
 public:
  bool open = true;
  bool gone = false;
  std::string written;
  std::string incoming;

  bool IsOpen() const override { return open; }
  bool Write(std::string const& data) override {
    if (!open) return false;
    written += data;
    return true;
  }
  bool Read(std::string* data) override {
    if (!open) return false;
    *data += incoming;
    incoming.clear();
    if (gone) open = false;
    return open;
  }
  void Close() override { open = false; }
};

// opening the port resets an Uno, and a line sent before SerialControl has
// started is lost; the link holds back until the sketch says ready
TEST(ControlLinkTest, WaitsForReadyOnABoard) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(true);
  EXPECT_FALSE(link.IsReady());
  EXPECT_FALSE(link.Send("text=Hi"));
  EXPECT_EQ(port.written, "");

  port.incoming = "ready\r\n";
  EXPECT_TRUE(link.Task(&lines));
  EXPECT_TRUE(link.IsReady());
  EXPECT_TRUE(link.Send("text=Hi"));
  EXPECT_EQ(port.written, "text=Hi\n");
}

// the simulator's sketch said ready long before the program connected; waiting
// for it would wait for ever
TEST(ControlLinkTest, IsReadyAtOnceWithTheSimulator) {
  FakePort port;
  ControlLink link(port);

  link.Begin(false);
  EXPECT_TRUE(link.IsReady());
  EXPECT_TRUE(link.Send("clear"));
  EXPECT_EQ(port.written, "clear\n");
}

// a serial port delivers a line in pieces; the link gives whole lines, without
// the \r\n of println()
TEST(ControlLinkTest, CutsWhatArrivesIntoLines) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(false);
  port.incoming = "o";
  link.Task(&lines);
  EXPECT_TRUE(lines.empty());
  port.incoming = "k\r\nerror: out of range\r\nre";
  link.Task(&lines);
  EXPECT_EQ(lines, (std::vector<std::string>{"ok", "error: out of range"}));
}

// a sketch that never ends a line would grow the buffer without end; its text
// is shown in pieces instead
TEST(ControlLinkTest, CutsALineThatNeverEnds) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(false);
  port.incoming = std::string(CONTROL_LINK_MAX_LINE_LENGTH + 10, 'x');
  link.Task(&lines);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0].size(), CONTROL_LINK_MAX_LINE_LENGTH);
}

// a board unplugged: the link says so, with the lines that came before, and
// takes no more commands
TEST(ControlLinkTest, SaysWhenThePortIsGone) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(false);
  port.incoming = "ok\r\n";
  port.gone = true;
  EXPECT_FALSE(link.Task(&lines));
  EXPECT_EQ(lines, (std::vector<std::string>{"ok"}));
  EXPECT_FALSE(link.IsReady());
  EXPECT_FALSE(link.Send("clear"));
}

// half a line left from the last connection would be glued to the first line of
// the next
TEST(ControlLinkTest, BeginForgetsHalfALine) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(true);
  port.incoming = "rea";
  link.Task(&lines);
  link.Begin(true);
  port.incoming = "dy\r\n";
  link.Task(&lines);
  EXPECT_EQ(lines, (std::vector<std::string>{"dy"}));
  EXPECT_FALSE(link.IsReady());
}

// The answer to picture with the first column `first` and the rest dark, as
// SerialControl prints it.
static std::string Picture(char const* first) {
  return std::string("picture=") + first +
         std::string(2 * CONTROL_NUMBER_OF_COLUMNS - 2, '0') + "\r\n";
}

// the picture is asked for once per interval, and its answer is kept rather
// than shown as a line
TEST(ControlLinkTest, AsksForThePictureOncePerInterval) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(false);
  EXPECT_TRUE(link.GetPicture().empty());
  link.AskForPicture(1000);
  link.AskForPicture(1010);
  EXPECT_EQ(port.written, "picture\n");
  port.incoming = Picture("81");
  link.Task(&lines);
  EXPECT_TRUE(lines.empty());
  ASSERT_EQ(link.GetPicture().size(),
            static_cast<size_t>(CONTROL_NUMBER_OF_COLUMNS));
  EXPECT_EQ(link.GetPicture()[0], 0x81);
  link.AskForPicture(1000 + CONTROL_LINK_PICTURE_INTERVAL_MS - 1);
  EXPECT_EQ(port.written, "picture\n");
  link.AskForPicture(1000 + CONTROL_LINK_PICTURE_INTERVAL_MS);
  EXPECT_EQ(port.written, "picture\npicture\n");
}

// an answer that never comes, a line lost on the wire say, would stop the
// picture for good
TEST(ControlLinkTest, AsksAgainWhenAnAnswerIsLost) {
  FakePort port;
  ControlLink link(port);

  link.Begin(false);
  link.AskForPicture(0);
  link.AskForPicture(CONTROL_LINK_PICTURE_TIMEOUT_MS - 1);
  EXPECT_EQ(port.written, "picture\n");
  link.AskForPicture(CONTROL_LINK_PICTURE_TIMEOUT_MS);
  EXPECT_EQ(port.written, "picture\npicture\n");
}

// a dot clicked while an answer is due: the answer shows the matrix before the
// click and would turn the dot off again on the screen for a moment, so it is
// dropped
TEST(ControlLinkTest, DropsAPictureAskedForBeforeACommand) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(false);
  link.AskForPicture(0);
  link.Send("setDot=0,0,1");
  port.incoming = Picture("00") + "ok\r\n";
  link.Task(&lines);
  EXPECT_TRUE(link.GetPicture().empty());
  EXPECT_EQ(lines, (std::vector<std::string>{"ok"}));
  link.AskForPicture(CONTROL_LINK_PICTURE_INTERVAL_MS);
  port.incoming = Picture("01");
  link.Task(&lines);
  ASSERT_FALSE(link.GetPicture().empty());
  EXPECT_EQ(link.GetPicture()[0], 0x01);
}

// a board reset by its button says ready again and answers nothing asked for
// before; the link asks again at the next interval rather than after the
// timeout
TEST(ControlLinkTest, AsksAgainWhenTheBoardHasStartedAgain) {
  FakePort port;
  ControlLink link(port);
  std::vector<std::string> lines;

  link.Begin(false);
  link.AskForPicture(0);
  port.incoming = "ready\r\n";
  link.Task(&lines);
  EXPECT_EQ(lines, (std::vector<std::string>{"ready"}));
  link.AskForPicture(CONTROL_LINK_PICTURE_INTERVAL_MS);
  EXPECT_EQ(port.written, "picture\npicture\n");
}

// a board that waits for ready is not asked, as it would lose the line
TEST(ControlLinkTest, AsksNothingBeforeReady) {
  FakePort port;
  ControlLink link(port);

  link.Begin(true);
  link.AskForPicture(1000);
  EXPECT_EQ(port.written, "");
}
