/* Tests of the link between the control program and SerialControl, against a port that is a string each way. */
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "ControlLink.h"

// A port whose other end is the test: what the link writes collects in `written`, what the test puts into `incoming`
// arrives at the next Read(), and `gone` ends the connection as a pulled cable would.
class FakePort : public ControlPort
{
  public:
    bool open = true;
    bool gone = false;
    std::string written;
    std::string incoming;

    bool IsOpen() const override { return open; }
    bool Write(std::string const& data) override
    {
        if(!open) return false;
        written += data;
        return true;
    }
    bool Read(std::string* data) override
    {
        if(!open) return false;
        *data += incoming;
        incoming.clear();
        if(gone) open = false;
        return open;
    }
    void Close() override { open = false; }
};

// opening the port resets an Uno, and a line sent before SerialControl has started is lost; the link holds back until
// the sketch says ready
TEST(ControlLinkTest, WaitsForReadyOnABoard)
{
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

// the simulator's sketch said ready long before the program connected; waiting for it would wait for ever
TEST(ControlLinkTest, IsReadyAtOnceWithTheSimulator)
{
    FakePort port;
    ControlLink link(port);

    link.Begin(false);
    EXPECT_TRUE(link.IsReady());
    EXPECT_TRUE(link.Send("clear"));
    EXPECT_EQ(port.written, "clear\n");
}

// a serial port delivers a line in pieces; the link gives whole lines, without the \r\n of println()
TEST(ControlLinkTest, CutsWhatArrivesIntoLines)
{
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

// a sketch that never ends a line would grow the buffer without end; its text is shown in pieces instead
TEST(ControlLinkTest, CutsALineThatNeverEnds)
{
    FakePort port;
    ControlLink link(port);
    std::vector<std::string> lines;

    link.Begin(false);
    port.incoming = std::string(CONTROL_LINK_MAX_LINE_LENGTH + 10, 'x');
    link.Task(&lines);
    ASSERT_EQ(lines.size(), 1u);
    EXPECT_EQ(lines[0].size(), CONTROL_LINK_MAX_LINE_LENGTH);
}

// a board unplugged: the link says so, with the lines that came before, and takes no more commands
TEST(ControlLinkTest, SaysWhenThePortIsGone)
{
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

// half a line left from the last connection would be glued to the first line of the next
TEST(ControlLinkTest, BeginForgetsHalfALine)
{
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
