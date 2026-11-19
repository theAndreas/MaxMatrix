// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the link between the control program and SerialControl, against a
 * port that is a string each way. */
#include "ControlLink.h"

#include <gtest/gtest.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "ControlCommand.h"
#include "ControlPort.h"
#include "MaxMatrixAttributes.h"

namespace {

// A port whose other end is the test: what the link writes collects in
// `written`, what the test puts into `incoming` arrives at the next Read(), and
// `gone` ends the connection as a pulled cable would.
class FakePort : public ControlPort {
 public:
  bool open{true};
  bool gone{false};
  std::string written;
  std::string incoming;

  MAXMATRIX_NODISCARD bool IsOpen() const override { return open; }
  bool Write(std::string const& data) override {
    if (!open) {
      return false;
    }
    written += data;
    return true;
  }
  bool Read(std::string& data) override {
    if (!open) {
      return false;
    }
    data += incoming;
    incoming.clear();
    if (gone) {
      open = false;
    }
    return open;
  }
  void Close() override { open = false; }
};

/* A link over a port that is a string each way, and the lines it has read. */
class ControlLinkTest : public ::testing::Test {
 protected:
  FakePort port_;
  ControlLink link_{port_};
  std::vector<std::string> lines_;

  // Calls Task() and appends the lines it read to lines_, as the window logs
  // them one after another; returns whether the port is still open.
  bool Task() {
    ControlLinkLines const read{link_.Task()};

    lines_.insert(lines_.end(), read.lines.begin(), read.lines.end());
    return read.open;
  }
};

/* Opening the port resets an Uno, and a line sent before SerialControl has
 * started is lost; the link holds back until the sketch says ready. */
TEST_F(ControlLinkTest, IsNotReadyOnABoardBeforeItSaysSo) {
  link_.Begin(true);

  EXPECT_FALSE(link_.IsReady());
}

TEST_F(ControlLinkTest, SendsNothingToABoardBeforeItIsReady) {
  link_.Begin(true);

  bool const sent{link_.Send("text=Hi")};

  EXPECT_FALSE(sent);
  EXPECT_EQ(port_.written, "");
}

TEST_F(ControlLinkTest, IsReadyOnceTheBoardSaysSo) {
  link_.Begin(true);
  port_.incoming = "ready\r\n";

  ASSERT_TRUE(Task());

  EXPECT_TRUE(link_.IsReady());
}

TEST_F(ControlLinkTest, SendsALineOnceTheBoardIsReady) {
  link_.Begin(true);
  port_.incoming = "ready\r\n";
  ASSERT_TRUE(Task());

  bool const sent{link_.Send("text=Hi")};

  EXPECT_TRUE(sent);
  EXPECT_EQ(port_.written, "text=Hi\n");
}

/* The simulator's sketch said ready long before the program connected; waiting
 * for it would wait for ever. */
TEST_F(ControlLinkTest, IsReadyAtOnceWithTheSimulator) {
  link_.Begin(false);

  EXPECT_TRUE(link_.IsReady());
}

TEST_F(ControlLinkTest, SendsALineAtOnceToTheSimulator) {
  link_.Begin(false);

  bool const sent{link_.Send("clear")};

  EXPECT_TRUE(sent);
  EXPECT_EQ(port_.written, "clear\n");
}

/* A serial port delivers a line in pieces; the link gives whole lines, without
 * the \r\n of println(). */
TEST_F(ControlLinkTest, GivesNoLineBeforeItsEnd) {
  link_.Begin(false);
  port_.incoming = "o";

  Task();

  EXPECT_TRUE(lines_.empty());
}

TEST_F(ControlLinkTest, CutsWhatArrivesIntoLines) {
  link_.Begin(false);
  port_.incoming = "o";
  Task();
  port_.incoming = "k\r\nerror: out of range\r\nre";

  Task();

  EXPECT_EQ(lines_, (std::vector<std::string>{"ok", "error: out of range"}));
}

/* a sketch that never ends a line would grow the buffer without end; its text
 * is shown in pieces instead */
TEST_F(ControlLinkTest, CutsALineThatNeverEnds) {
  link_.Begin(false);
  port_.incoming = std::string(kControlLinkMaxLineLength + 10, 'x');

  Task();

  ASSERT_EQ(lines_.size(), 1u);
  EXPECT_EQ(lines_[0].size(), kControlLinkMaxLineLength);
}

/* A board unplugged: the link says so, with the lines that came before, and
 * takes no more commands. */
class GoneLinkTest : public ControlLinkTest {
 protected:
  void SetUp() override {
    link_.Begin(false);
    port_.incoming = "ok\r\n";
    port_.gone = true;
  }
};

TEST_F(GoneLinkTest, SaysThePortIsGone) {
  bool const open{Task()};

  EXPECT_FALSE(open);
}

TEST_F(GoneLinkTest, GivesTheLinesThatCameBefore) {
  Task();

  EXPECT_EQ(lines_, (std::vector<std::string>{"ok"}));
}

TEST_F(GoneLinkTest, IsNoLongerReady) {
  Task();

  EXPECT_FALSE(link_.IsReady());
}

TEST_F(GoneLinkTest, SendsNoMoreCommands) {
  Task();

  bool const sent{link_.Send("clear")};

  EXPECT_FALSE(sent);
}

/* half a line left from the last connection would be glued to the first line of
 * the next */
TEST_F(ControlLinkTest, BeginForgetsHalfALine) {
  link_.Begin(true);
  port_.incoming = "rea";
  Task();
  link_.Begin(true);
  port_.incoming = "dy\r\n";

  Task();

  EXPECT_EQ(lines_, (std::vector<std::string>{"dy"}));
  EXPECT_FALSE(link_.IsReady());
}

// The answer to picture with the first column `first` and the rest dark, as
// SerialControl prints it.
std::string Picture(char const* first) {
  return std::string("picture=") + first +
         std::string((2 * kControlNumberOfColumns) - 2, '0') + "\r\n";
}

/* The picture is asked for once per interval, and its answer is kept rather
 * than shown as a line. */
TEST_F(ControlLinkTest, HasNoPictureBeforeAnAnswer) {
  link_.Begin(false);

  EXPECT_TRUE(link_.GetPicture().empty());
}

TEST_F(ControlLinkTest, AsksForThePictureOncePerInterval) {
  link_.Begin(false);

  link_.AskForPicture(1000);
  link_.AskForPicture(1010);

  EXPECT_EQ(port_.written, "picture\n");
}

TEST_F(ControlLinkTest, KeepsTheAnswerToPicture) {
  link_.Begin(false);
  link_.AskForPicture(1000);
  port_.incoming = Picture("81");

  Task();

  ASSERT_EQ(link_.GetPicture().size(), 32u);
  EXPECT_EQ(link_.GetPicture()[0], 0x81);
}

TEST_F(ControlLinkTest, ShowsNoAnswerToPictureAsALine) {
  link_.Begin(false);
  link_.AskForPicture(1000);
  port_.incoming = Picture("81");

  Task();

  EXPECT_TRUE(lines_.empty());
}

/* the interval is 50 ms */
TEST_F(ControlLinkTest, AsksNotAgainBeforeTheInterval) {
  link_.Begin(false);
  link_.AskForPicture(1000);
  port_.incoming = Picture("81");
  Task();

  link_.AskForPicture(1049);

  EXPECT_EQ(port_.written, "picture\n");
}

TEST_F(ControlLinkTest, AsksAgainAfterTheInterval) {
  link_.Begin(false);
  link_.AskForPicture(1000);
  port_.incoming = Picture("81");
  Task();

  link_.AskForPicture(1050);

  EXPECT_EQ(port_.written, "picture\npicture\n");
}

/* An answer that never comes, a line lost on the wire say, would stop the
 * picture for good; the link waits a second for it. */
TEST_F(ControlLinkTest, WaitsForALostAnswerUntilTheTimeout) {
  link_.Begin(false);
  link_.AskForPicture(0);

  link_.AskForPicture(999);

  EXPECT_EQ(port_.written, "picture\n");
}

TEST_F(ControlLinkTest, AsksAgainWhenAnAnswerIsLost) {
  link_.Begin(false);
  link_.AskForPicture(0);

  link_.AskForPicture(1000);

  EXPECT_EQ(port_.written, "picture\npicture\n");
}

/* A dot clicked while an answer is due: the answer shows the matrix before the
 * click and would turn the dot off again on the screen for a moment, so it is
 * dropped. */
TEST_F(ControlLinkTest, DropsAPictureAskedForBeforeACommand) {
  link_.Begin(false);
  link_.AskForPicture(0);
  link_.Send("setDot=0,0,1");
  port_.incoming = Picture("00") + "ok\r\n";

  Task();

  EXPECT_TRUE(link_.GetPicture().empty());
  EXPECT_EQ(lines_, (std::vector<std::string>{"ok"}));
}

TEST_F(ControlLinkTest, KeepsThePictureAskedForAfterTheCommand) {
  link_.Begin(false);
  link_.AskForPicture(0);
  link_.Send("setDot=0,0,1");
  port_.incoming = Picture("00") + "ok\r\n";
  Task();
  link_.AskForPicture(50);
  port_.incoming = Picture("01");

  Task();

  ASSERT_FALSE(link_.GetPicture().empty());
  EXPECT_EQ(link_.GetPicture()[0], 0x01);
}

/* A board reset by its button says ready again and answers nothing asked for
 * before; the link asks again at the next interval rather than after the
 * timeout. */
TEST_F(ControlLinkTest, ShowsTheReadyOfABoardStartedAgain) {
  link_.Begin(false);
  link_.AskForPicture(0);
  port_.incoming = "ready\r\n";

  Task();

  EXPECT_EQ(lines_, (std::vector<std::string>{"ready"}));
}

TEST_F(ControlLinkTest, AsksAgainWhenTheBoardHasStartedAgain) {
  link_.Begin(false);
  link_.AskForPicture(0);
  port_.incoming = "ready\r\n";
  Task();

  link_.AskForPicture(50);

  EXPECT_EQ(port_.written, "picture\npicture\n");
}

/* a board that waits for ready is not asked, as it would lose the line */
TEST_F(ControlLinkTest, AsksNothingBeforeReady) {
  link_.Begin(true);

  link_.AskForPicture(1000);

  EXPECT_EQ(port_.written, "");
}

}  // namespace
