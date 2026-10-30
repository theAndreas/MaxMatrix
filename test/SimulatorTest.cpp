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

static stdReturnType parse(SimulatorOptions& Options, std::vector<std::string> Arguments)
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

    ASSERT_EQ(parse(Options, { "--port", "65535" }), E_OK);
    EXPECT_EQ(Options.getPort(), 65535);
}

/* Bug: atoi() and a cast to unsigned short turned --port 70000 into port 4464, so the simulator listened somewhere
 * else than asked, without a word. */
TEST(SimulatorTest, PortOutOfRangeIsRefused)
{
    SimulatorOptions Options;

    EXPECT_EQ(parse(Options, { "--port", "70000" }), E_NOT_OK);
    EXPECT_EQ(parse(Options, { "--port", "-1" }), E_NOT_OK);
    EXPECT_EQ(parse(Options, { "--port", "0" }), E_NOT_OK);
}

TEST(SimulatorTest, PortThatIsNoNumberIsRefused)
{
    SimulatorOptions Options;

    EXPECT_EQ(parse(Options, { "--port", "web" }), E_NOT_OK);
    EXPECT_EQ(parse(Options, { "--port", "80x" }), E_NOT_OK);
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
