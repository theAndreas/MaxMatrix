// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the simulator's command line and panel, against the MAX7219 model
 * rather than a sketch.
 *
 * simulator/Basics and simulator/web run a whole sketch and see the panel; what
 * they cannot see is a value on the command line that is quietly wrong, or a
 * line of the terminal that shows the chips in the wrong order. */
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

#include "Arduino.h"
#include "CaseName.h"
#include "SimulatorOptions.h"
#include "SimulatorPanel.h"
#include "SimulatorTerminal.h"
#include "Web/SimulatorWeb.h"

namespace {

/* the chain the examples have */
uint8_t const kNumberOfModules{4};

bool Parse(SimulatorOptions& options, std::vector<std::string> arguments) {
  std::vector<char*> argv;

  arguments.insert(arguments.begin(), "Simulator");
  argv.reserve(arguments.size());
  for (std::string& argument : arguments) {
    argv.push_back(argument.data());
  }
  return options.Parse(static_cast<int>(argv.size()), argv.data());
}

/* a value of an option on the command line, and what it is taken as */
struct TakenValue {
  char const* name;
  char const* text;
  uint32_t value;
};

/* a value of an option on the command line that is refused */
struct RefusedValue {
  char const* name;
  char const* text;
};

constexpr std::array<TakenValue, 2> kTakenPorts{{
    {"Least", "1", 1},
    {"Greatest", "65535", 65535},
}};

/* Bug: atoi() and a cast to unsigned short turned --port 70000 into port 4464,
 * so the simulator listened somewhere else than asked, without a word. */
constexpr std::array<RefusedValue, 6> kRefusedPorts{{
    {"Zero", "0"},
    {"OneBeyondTheGreatest", "65536"},
    {"FarBeyondTheGreatest", "70000"},
    {"Negative", "-1"},
    {"Word", "web"},
    {"NumberAndALetter", "80x"},
}};

class TakenPortTest : public ::testing::TestWithParam<TakenValue> {};

INSTANTIATE_TEST_SUITE_P(Ports, TakenPortTest, ::testing::ValuesIn(kTakenPorts),
                         CaseName<TakenValue>);

TEST_P(TakenPortTest, IsTaken) {
  SimulatorOptions options;

  ASSERT_TRUE(Parse(options, {"--port", GetParam().text}));

  EXPECT_EQ(options.GetPort(), GetParam().value);
}

class RefusedPortTest : public ::testing::TestWithParam<RefusedValue> {};

INSTANTIATE_TEST_SUITE_P(Ports, RefusedPortTest,
                         ::testing::ValuesIn(kRefusedPorts),
                         CaseName<RefusedValue>);

TEST_P(RefusedPortTest, IsRefused) {
  SimulatorOptions options;

  bool const taken{Parse(options, {"--port", GetParam().text})};

  EXPECT_FALSE(taken);
}

/* --for takes the seconds as far as a millisecond, a fraction past it dropped,
 * up to the most milliseconds a uint32_t holds; as atof() read them, which went
 * for the double the coding style forbids (H-7) */
constexpr std::array<TakenValue, 6> kTakenDurations{{
    {"Fraction", "1.5", 1500},
    {"OneMillisecond", "0.001", 1},
    {"FractionAlone", ".5", 500},
    {"PointWithoutAFraction", "2.", 2000},
    {"PastAMillisecond", "1.2349", 1234},
    {"Greatest", "4294967.295", 4294967295u},
}};

/* Bug: atof() took --for 2x as two seconds, and turned -1 and 1e10 into a value
 * a cast to uint32_t is undefined for. */
constexpr std::array<RefusedValue, 9> kRefusedDurations{{
    {"OneBeyondTheGreatest", "4294967.296"},
    {"LessThanAMillisecond", "0.0009"},
    {"PointAlone", "."},
    {"TwoPoints", "1.2.3"},
    {"NumberAndALetter", "2x"},
    {"Negative", "-1"},
    {"Exponent", "1e10"},
    {"NotANumber", "nan"},
    {"Zero", "0"},
}};

class TakenDurationTest : public ::testing::TestWithParam<TakenValue> {};

INSTANTIATE_TEST_SUITE_P(Durations, TakenDurationTest,
                         ::testing::ValuesIn(kTakenDurations),
                         CaseName<TakenValue>);

TEST_P(TakenDurationTest, IsTakenInMilliseconds) {
  SimulatorOptions options;

  ASSERT_TRUE(Parse(options, {"--for", GetParam().text}));

  EXPECT_EQ(options.GetRunFor(), GetParam().value);
}

class RefusedDurationTest : public ::testing::TestWithParam<RefusedValue> {};

INSTANTIATE_TEST_SUITE_P(Durations, RefusedDurationTest,
                         ::testing::ValuesIn(kRefusedDurations),
                         CaseName<RefusedValue>);

TEST_P(RefusedDurationTest, IsRefused) {
  SimulatorOptions options;

  bool const taken{Parse(options, {"--for", GetParam().text})};

  EXPECT_FALSE(taken);
}

/* a chain whose chips each have their own intensity, Chips[i] the intensity i,
 * so that an order shows */
void ResetChainWithDistinctIntensities() {
  mock::Reset(11, 10, 13, kNumberOfModules);
  for (size_t chip{0}; chip < mock::chain.chips.size(); chip++) {
    mock::chain.chips[chip].intensity = static_cast<uint8_t>(chip);
  }
}

/* ChipAt() places the chips as the viewer sees them: upright the leftmost
 * module is the chip at the far end */
TEST(SimulatorTest, IntensityIsCountedFromTheLeft) {
  SimulatorPanel const panel;
  ResetChainWithDistinctIntensities();

  uint8_t const intensity{panel.GetIntensity(0)};

  EXPECT_EQ(intensity, kNumberOfModules - 1u);
}

TEST(SimulatorTest, IntensityUpsideDownIsCountedFromTheLeft) {
  SimulatorPanel panel;
  ResetChainWithDistinctIntensities();
  panel.SetUpsideDown(true);

  uint8_t const intensity{panel.GetIntensity(0)};

  EXPECT_EQ(intensity, 0u);
}

TEST(SimulatorTest, StatusLineListsTheIntensitiesFromTheLeft) {
  SimulatorPanel const panel;
  ResetChainWithDistinctIntensities();

  std::string const line{SimulatorTerminal::GetStatusLine(panel)};

  EXPECT_EQ(line, "FC16, 4 modules, intensity 3 2 1 0, 0 frames");
}

/* Bug: the terminal listed the intensities in the order of the chain, whatever
 * the mounting, while the page lists them from the left as the viewer sees the
 * panel; upside down, the terminal had them backwards. */
TEST(SimulatorTest, StatusLineFollowsTheMounting) {
  SimulatorPanel panel;
  ResetChainWithDistinctIntensities();
  panel.SetUpsideDown(true);

  std::string const line{SimulatorTerminal::GetStatusLine(panel)};

  EXPECT_EQ(line, "FC16, 4 modules, upside down, intensity 0 1 2 3, 0 frames");
}

/* What a sketch prints goes to the page inside a JSON string, in a WebSocket
 * text frame. A quote or backslash would end or break the string, and a byte
 * that is not valid UTF-8, such as a latin-1 umlaut, makes the browser close
 * the connection; so everything outside printable ASCII is written as \u00XX.
 */
struct JsonCase {
  char const* name;
  char const* text;
  size_t length;
  char const* json;
};

constexpr std::array<JsonCase, 4> kJsonCases{{
    {"LineEnd", "ok\r\n", 4, "ok\\u000D\\u000A"},
    {"QuoteAndBackslash", R"(say "hi" \ bye)", 14, R"(say \"hi\" \\ bye)"},
    {"Latin1", "\xE4", 1, "\\u00E4"},
    {"ControlChars", "\x00\x1F\x7F", 3, R"(\u0000\u001F\u007F)"},
}};

class SerialOutputTest : public ::testing::TestWithParam<JsonCase> {};

INSTANTIATE_TEST_SUITE_P(Json, SerialOutputTest,
                         ::testing::ValuesIn(kJsonCases), CaseName<JsonCase>);

TEST_P(SerialOutputTest, IsEscapedForJson) {
  std::string const text{GetParam().text, GetParam().length};

  std::string const json{SimulatorWeb::ConvertToJsonString(text)};

  EXPECT_EQ(json, GetParam().json);
}

}  // namespace
