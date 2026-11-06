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
#include "Web/SimulatorWeb.h"

static bool Parse(SimulatorOptions& options, std::vector<std::string> arguments)
{
    std::vector<char*> argv;

    arguments.insert(arguments.begin(), "Simulator");
    for(std::string& argument : arguments) argv.push_back(&argument[0]);
    return options.Parse(static_cast<int>(argv.size()), argv.data());
}

/* a chain whose chips each have their own intensity, Chips[i] the intensity i, so that an order shows */
static void ResetChainWithDistinctIntensities()
{
    mock::Reset(11, 10, 13, MAXMATRIX_NUMBER_OF_MODULES);
    for(size_t chip = 0; chip < mock::chain.chips.size(); chip++) mock::chain.chips[chip].intensity = static_cast<uint8_t>(chip);
}

TEST(SimulatorTest, PortInRangeIsTaken)
{
    SimulatorOptions options;

    ASSERT_TRUE(Parse(options, { "--port", "65535" }));
    EXPECT_EQ(options.GetPort(), 65535);
}

/* Bug: atoi() and a cast to unsigned short turned --port 70000 into port 4464, so the simulator listened somewhere
 * else than asked, without a word. */
TEST(SimulatorTest, PortOutOfRangeIsRefused)
{
    SimulatorOptions options;

    EXPECT_FALSE(Parse(options, { "--port", "70000" }));
    EXPECT_FALSE(Parse(options, { "--port", "-1" }));
    EXPECT_FALSE(Parse(options, { "--port", "0" }));
}

TEST(SimulatorTest, PortThatIsNoNumberIsRefused)
{
    SimulatorOptions options;

    EXPECT_FALSE(Parse(options, { "--port", "web" }));
    EXPECT_FALSE(Parse(options, { "--port", "80x" }));
}

TEST(SimulatorTest, RunForIsTakenInMilliseconds)
{
    SimulatorOptions options;

    ASSERT_TRUE(Parse(options, { "--for", "1.5" }));
    EXPECT_EQ(options.GetRunFor(), 1500u);
}

/* Bug: atof() took --for 2x as two seconds, and turned -1 and 1e10 into a value a cast to uint32_t is undefined for. */
TEST(SimulatorTest, RunForThatIsNoDurationIsRefused)
{
    SimulatorOptions options;

    EXPECT_FALSE(Parse(options, { "--for", "2x" }));
    EXPECT_FALSE(Parse(options, { "--for", "-1" }));
    EXPECT_FALSE(Parse(options, { "--for", "1e10" }));
    EXPECT_FALSE(Parse(options, { "--for", "nan" }));
    EXPECT_FALSE(Parse(options, { "--for", "0" }));
}

/* ChipAt() places the chips as the viewer sees them: upright the leftmost module is the chip at the far end */
TEST(SimulatorTest, IntensityIsCountedFromTheLeft)
{
    SimulatorPanel panel;

    ResetChainWithDistinctIntensities();
    EXPECT_EQ(panel.GetIntensity(0), MAXMATRIX_NUMBER_OF_MODULES - 1u);
    panel.SetUpsideDown(true);
    EXPECT_EQ(panel.GetIntensity(0), 0u);
}

TEST(SimulatorTest, StatusLineListsTheIntensitiesFromTheLeft)
{
    SimulatorPanel panel;

    ResetChainWithDistinctIntensities();
    EXPECT_EQ(SimulatorTerminal::GetStatusLine(panel), "FC16, 4 modules, intensity 3 2 1 0, 0 frames");
}

/* Bug: the terminal listed the intensities in the order of the chain, whatever the mounting, while the page lists
 * them from the left as the viewer sees the panel; upside down, the terminal had them backwards. */
TEST(SimulatorTest, StatusLineFollowsTheMounting)
{
    SimulatorPanel panel;

    ResetChainWithDistinctIntensities();
    panel.SetUpsideDown(true);
    EXPECT_EQ(SimulatorTerminal::GetStatusLine(panel), "FC16, 4 modules, upside down, intensity 0 1 2 3, 0 frames");
}

/* What a sketch prints goes to the page inside a JSON string, in a WebSocket text frame. A quote or backslash would end
 * or break the string, and a byte that is not valid UTF-8, such as a latin-1 umlaut, makes the browser close the
 * connection; so everything outside printable ASCII is written as \u00XX. */
TEST(SimulatorTest, SerialOutputIsEscapedForJson)
{
    EXPECT_EQ(SimulatorWeb::ConvertToJsonString("ok\r\n"), "ok\\u000D\\u000A");
    EXPECT_EQ(SimulatorWeb::ConvertToJsonString("say \"hi\" \\ bye"), "say \\\"hi\\\" \\\\ bye");
    EXPECT_EQ(SimulatorWeb::ConvertToJsonString("\xE4"), "\\u00E4");
    EXPECT_EQ(SimulatorWeb::ConvertToJsonString(std::string("\x00\x1F\x7F", 3)), "\\u0000\\u001F\\u007F");
}
