/* Tests of the simulator's command line and panel, against the MAX7219 model rather than a sketch.
 *
 * simulator/Basics and simulator/web run a whole sketch and see the panel; what they cannot see is a value on the
 * command line that is quietly wrong, or a line of the terminal that shows the chips in the wrong order. */
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"
#include "SimulatorWeb.h"

static bool parse(SimulatorOptions& Options, std::vector<std::string> Arguments)
{
    std::vector<char*> Argv;

    Arguments.insert(Arguments.begin(), "Simulator");
    for(std::string& Argument : Arguments) Argv.push_back(&Argument[0]);
    return Options.parse(static_cast<int>(Argv.size()), Argv.data());
}

/* a chain whose chips each have their own intensity, Chips[i] the intensity i, so that an order shows */
static void resetChainWithDistinctIntensities()
{
    Mock::reset(10, MAXMATRIX_NUMBER_OF_MODULES);
    for(size_t Chip = 0; Chip < Mock::Chain.Chips.size(); Chip++) Mock::Chain.Chips[Chip].Intensity = static_cast<uint8_t>(Chip);
}

TEST(SimulatorTest, PortInRangeIsTaken)
{
    SimulatorOptions Options;

    ASSERT_TRUE(parse(Options, { "--port", "65535" }));
    EXPECT_EQ(Options.getPort(), 65535);
}

/* Bug: atoi() and a cast to unsigned short turned --port 70000 into port 4464, so the simulator listened somewhere
 * else than asked, without a word. */
TEST(SimulatorTest, PortOutOfRangeIsRefused)
{
    SimulatorOptions Options;

    EXPECT_FALSE(parse(Options, { "--port", "70000" }));
    EXPECT_FALSE(parse(Options, { "--port", "-1" }));
    EXPECT_FALSE(parse(Options, { "--port", "0" }));
}

TEST(SimulatorTest, PortThatIsNoNumberIsRefused)
{
    SimulatorOptions Options;

    EXPECT_FALSE(parse(Options, { "--port", "web" }));
    EXPECT_FALSE(parse(Options, { "--port", "80x" }));
}

/* chipAt() places the chips as the viewer sees them: upright the leftmost module is the chip at the far end */
TEST(SimulatorTest, IntensityIsCountedFromTheLeft)
{
    SimulatorPanel Panel;

    resetChainWithDistinctIntensities();
    EXPECT_EQ(Panel.getIntensity(0), MAXMATRIX_NUMBER_OF_MODULES - 1u);
    Panel.setUpsideDown(true);
    EXPECT_EQ(Panel.getIntensity(0), 0u);
}

TEST(SimulatorTest, StatusLineListsTheIntensitiesFromTheLeft)
{
    SimulatorPanel Panel;

    resetChainWithDistinctIntensities();
    EXPECT_EQ(SimulatorTerminal::getStatusLine(Panel), "FC16, 4 modules, intensity 3 2 1 0, 0 frames");
}

/* Bug: the terminal listed the intensities in the order of the chain, whatever the mounting, while the page lists
 * them from the left as the viewer sees the panel; upside down, the terminal had them backwards. */
TEST(SimulatorTest, StatusLineFollowsTheMounting)
{
    SimulatorPanel Panel;

    resetChainWithDistinctIntensities();
    Panel.setUpsideDown(true);
    EXPECT_EQ(SimulatorTerminal::getStatusLine(Panel), "FC16, 4 modules, upside down, intensity 0 1 2 3, 0 frames");
}

/* What a sketch prints goes to the page inside a JSON string, in a WebSocket text frame. A quote or backslash would end
 * or break the string, and a byte that is not valid UTF-8, such as a latin-1 umlaut, makes the browser close the
 * connection; so everything outside printable ASCII is written as \u00XX. */
TEST(SimulatorTest, SerialOutputIsEscapedForJson)
{
    EXPECT_EQ(SimulatorWeb::convertToJsonString("ok\r\n"), "ok\\u000D\\u000A");
    EXPECT_EQ(SimulatorWeb::convertToJsonString("say \"hi\" \\ bye"), "say \\\"hi\\\" \\\\ bye");
    EXPECT_EQ(SimulatorWeb::convertToJsonString("\xE4"), "\\u00E4");
    EXPECT_EQ(SimulatorWeb::convertToJsonString(std::string("\x00\x1F\x7F", 3)), "\\u0000\\u001F\\u007F");
}
