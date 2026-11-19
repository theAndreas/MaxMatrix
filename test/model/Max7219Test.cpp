// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the MAX7219 model in mock/, against the datasheet rather than
 * against the library.
 *
 * The library tests take the model's word for what is lit, so a mistake here
 * would hide one there. */
#include "Max7219.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

#include "CaseName.h"
#include "Utility/Bit.h"

namespace {

using max_matrix::Bit;
using Pair = std::pair<uint8_t, uint8_t>;

/* one frame: every pair shifted in, address first, then the rising edge of chip
 * select */
void SendFrame(Max7219Chain& chain, std::initializer_list<Pair> pairs) {
  for (Pair const& pair : pairs) {
    chain.Shift(pair.first);
    chain.Shift(pair.second);
  }
  chain.Latch();
}

Max7219Chain ChainInNormalOperation(uint8_t number_of_chips) {
  Max7219Chain chain;
  chain.Reset(number_of_chips);
  for (uint8_t i{0}; i < number_of_chips; i++) {
    chain.chips[i].shutdown = 1;
    chain.chips[i].scan_limit = 7;
  }
  return chain;
}

TEST(Max7219Test, FirstPairSentEndsUpAtTheFarEnd) {
  Max7219Chain chain{ChainInNormalOperation(3)};

  SendFrame(chain, {{0x01, 0x11}, {0x01, 0x22}, {0x01, 0x33}});

  EXPECT_EQ(chain.chips[2].digit[0], 0x11);
  EXPECT_EQ(chain.chips[1].digit[0], 0x22);
  EXPECT_EQ(chain.chips[0].digit[0], 0x33);
}

/* the chip at the far end still holds a pair of the frame before and latches it
 * again */
TEST(Max7219Test, ShortFrameLatchesWhatIsLeftInTheChain) {
  Max7219Chain chain{ChainInNormalOperation(2)};
  SendFrame(chain, {{0x01, 0xAA}, {0x02, 0xBB}});

  SendFrame(chain, {{0x03, 0xCC}});

  EXPECT_EQ(chain.chips[0].digit[2], 0xCC);
  EXPECT_EQ(chain.chips[1].digit[1], 0xBB);
}

/* the register shifts bit by bit: a frame one bit short, as bit banging could
 * send it, latches every pair one bit to the right - 0x02 0x22 as digit 0 with
 * 0x11 */
TEST(Max7219Test, FrameOneBitShortIsLatchedOneBitOff) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  uint16_t const pair{0x0222};
  for (uint8_t bit{15}; bit >= 1; bit--) {
    chain.ShiftBit(Bit<uint16_t>::ReadBit(pair, bit));
  }

  chain.Latch();

  EXPECT_EQ(chain.chips[0].digit[0], 0x11);
  EXPECT_EQ(chain.chips[0].digit[1], 0x00);
}

TEST(Max7219Test, NoOpChangesNothing) {
  Max7219Chain chain{ChainInNormalOperation(2)};
  SendFrame(chain, {{0x01, 0x11}, {0x01, 0x22}});

  SendFrame(chain, {{0x00, 0xFF}, {0x00, 0xFF}});

  EXPECT_EQ(chain.chips[1].digit[0], 0x11);
  EXPECT_EQ(chain.chips[0].digit[0], 0x22);
}

TEST(Max7219Test, OnlyTheLowNibbleOfTheAddressCounts) {
  Max7219Chain chain{ChainInNormalOperation(1)};

  SendFrame(chain, {{0xF1, 0x5A}});

  EXPECT_EQ(chain.chips[0].digit[0], 0x5A);
}

TEST(Max7219Test, PowerUpIsShutDownWithOneDigitAndMinimumIntensity) {
  Max7219Chain chain;

  chain.Reset(1);

  Max7219 const& chip{chain.chips[0]};
  EXPECT_EQ(chip.shutdown, 0);
  EXPECT_EQ(chip.scan_limit, 0);
  EXPECT_EQ(chip.decode_mode, 0);
  EXPECT_EQ(chip.intensity, 0);
  EXPECT_EQ(chip.display_test, 0);
}

TEST(Max7219Test, DigitAfterPowerUpIsDark) {
  Max7219Chain chain;
  chain.Reset(1);

  SendFrame(chain, {{0x01, 0xFF}});

  EXPECT_EQ(chain.chips[0].Segments(0), 0x00);
}

TEST(Max7219Test, ShutdownBlanksTheDigits) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  SendFrame(chain, {{0x01, 0x81}});

  SendFrame(chain, {{0x0C, 0x00}});

  EXPECT_EQ(chain.chips[0].Segments(0), 0x00);
}

TEST(Max7219Test, NormalOperationShowsTheDigitsKeptInShutdown) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  SendFrame(chain, {{0x01, 0x81}});
  SendFrame(chain, {{0x0C, 0x00}});

  SendFrame(chain, {{0x0C, 0x01}});

  EXPECT_EQ(chain.chips[0].Segments(0), 0x81);
}

/* a scan limit of 2 shows digits 0 to 2: digit 2 is register 03 */
TEST(Max7219Test, DigitAtTheScanLimitIsShown) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  SendFrame(chain, {{0x03, 0xFF}});

  SendFrame(chain, {{0x0B, 0x02}});

  EXPECT_EQ(chain.chips[0].Segments(2), 0xFF);
}

TEST(Max7219Test, DigitBeyondTheScanLimitIsDark) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  SendFrame(chain, {{0x04, 0xFF}});

  SendFrame(chain, {{0x0B, 0x02}});

  EXPECT_EQ(chain.chips[0].Segments(3), 0x00);
}

/* the display test lights every LED, even in shutdown after power-up: the
 * first and the last digit */
TEST(Max7219Test, DisplayTestLightsEverythingEvenInShutdown) {
  Max7219Chain chain;
  chain.Reset(1);

  SendFrame(chain, {{0x0F, 0x01}});

  EXPECT_EQ(chain.chips[0].Segments(0), 0xFF);
  EXPECT_EQ(chain.chips[0].Segments(7), 0xFF);
}

TEST(Max7219Test, DisplayTestEndedShowsTheDigitsAgain) {
  Max7219Chain chain;
  chain.Reset(1);
  SendFrame(chain, {{0x0F, 0x01}});

  SendFrame(chain, {{0x0F, 0x00}});

  EXPECT_EQ(chain.chips[0].Segments(0), 0x00);
}

/* decode mode 01 decodes digit 0 alone: 85 is DP, 0x80, and '5', 0x5B */
TEST(Max7219Test, CodeBDecodesASelectedDigit) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  SendFrame(chain, {{0x09, 0x01}});

  SendFrame(chain, {{0x01, 0x85}});

  EXPECT_EQ(chain.chips[0].Segments(0), 0xDBu);
}

TEST(Max7219Test, CodeBLeavesADigitNotSelected) {
  Max7219Chain chain{ChainInNormalOperation(1)};
  SendFrame(chain, {{0x09, 0x01}});

  SendFrame(chain, {{0x02, 0x85}});

  EXPECT_EQ(chain.chips[0].Segments(1), 0x85);
}

TEST(Max7219Test, IntensityKeepsFourBits) {
  Max7219Chain chain{ChainInNormalOperation(1)};

  SendFrame(chain, {{0x0A, 0xF7}});

  EXPECT_EQ(chain.chips[0].intensity, 0x07);
}

// FN-16: a test's helper; a swap lights another LED, which the test it
// serves reports
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Max7219Chain ChainWithOneSegment(uint8_t number_of_chips, uint8_t chip,
                                 uint8_t digit, uint8_t segment) {
  Max7219Chain chain{ChainInNormalOperation(number_of_chips)};
  chain.chips[chip].digit[digit] = Bit<uint8_t>::ToMask(segment);
  return chain;
}

/* an LED as the viewer sees it, column X and row Y */
using Led = std::pair<uint16_t, uint16_t>;

std::vector<Led> LitLeds(Max7219Chain const& chain, Max7219Wiring const& wiring,
                         bool upside_down = false) {
  std::vector<Led> leds;
  uint16_t const width{
      static_cast<uint16_t>(chain.chips.size() * Max7219::kNumberOfDigits)};
  for (uint16_t x{0}; x < width; x++) {
    for (uint8_t y{0}; y < Max7219::kNumberOfDigits; y++) {
      if (chain.IsLit(wiring, x, y, upside_down)) {
        leds.emplace_back(x, y);
      }
    }
  }
  return leds;
}

/* how many LEDs each of the 64 segments of a chip lights, one after another, in
 * `wiring` */
std::vector<size_t> LedsPerSegment(Max7219Wiring const& wiring) {
  std::vector<size_t> counts;
  for (uint8_t digit{0}; digit < Max7219::kNumberOfDigits; digit++) {
    for (uint8_t segment{0}; segment < 8; segment++) {
      counts.push_back(
          LitLeds(ChainWithOneSegment(1, 0, digit, segment), wiring).size());
    }
  }
  return counts;
}

/* the LEDs the 64 segments of a chip light, one after another, in `wiring`,
 * each that lights exactly one */
std::vector<Led> LedOfEverySegment(Max7219Wiring const& wiring) {
  std::vector<Led> seen;
  for (uint8_t digit{0}; digit < Max7219::kNumberOfDigits; digit++) {
    for (uint8_t segment{0}; segment < 8; segment++) {
      std::vector<Led> const leds{
          LitLeds(ChainWithOneSegment(1, 0, digit, segment), wiring)};
      seen.insert(seen.end(), leds.begin(), leds.end());
    }
  }
  return seen;
}

/* every digit and segment drives one LED of its own, in every wiring */
class WiringTest : public ::testing::TestWithParam<Max7219Wiring> {};

INSTANTIATE_TEST_SUITE_P(Wirings, WiringTest,
                         ::testing::ValuesIn(kMax7219Wirings),
                         CaseName<Max7219Wiring>);

TEST_P(WiringTest, EverySegmentLightsOneLed) {
  std::vector<size_t> const counts{LedsPerSegment(GetParam())};

  EXPECT_EQ(counts, std::vector<size_t>(64, 1));
}

TEST_P(WiringTest, NoTwoSegmentsLightTheSameLed) {
  std::vector<Led> leds{LedOfEverySegment(GetParam())};

  std::sort(leds.begin(), leds.end());

  EXPECT_EQ(std::unique(leds.begin(), leds.end()), leds.end());
}

/* Digit 0, segment bit 0, where MD_MAX72XX's setPoint() puts it, with its
 * column 0 on the right. */
struct NamedWiring {
  char const* name;
  Max7219Wiring const* wiring;
  uint16_t x;
  uint16_t y;
};

constexpr std::array<NamedWiring, 4> kNamedWirings{{
    {"FC16", &kMax7219WiringFc16, 7, 0},
    {"GENERIC", &kMax7219WiringGeneric, 7, 7},
    {"PAROLA", &kMax7219WiringParola, 0, 0},
    {"ICSTATION", &kMax7219WiringIcstation, 0, 7},
}};

class NamedWiringTest : public ::testing::TestWithParam<NamedWiring> {};

INSTANTIATE_TEST_SUITE_P(MdMax72xx, NamedWiringTest,
                         ::testing::ValuesIn(kNamedWirings),
                         CaseName<NamedWiring>);

TEST_P(NamedWiringTest, LightsTheLedOfMdMax72xx) {
  std::vector<Led> const leds{
      LitLeds(ChainWithOneSegment(1, 0, 0, 0), *GetParam().wiring)};

  EXPECT_EQ(leds, (std::vector<Led>{Led(GetParam().x, GetParam().y)}));
}

/* the first chip of the chain drives the module on the right */
TEST(Max7219Test, ChainRunsFromRightToLeft) {
  Max7219Chain const chain{ChainWithOneSegment(2, 0, 0, 0)};

  std::vector<Led> const leds{LitLeds(chain, kMax7219WiringFc16)};

  EXPECT_EQ(leds, (std::vector<Led>{Led(15, 0)}));
}

/* upside down, the chain starts on the left and the LED is in the opposite
 * corner of its module */
TEST(Max7219Test, ChainUpsideDownRunsFromLeftToRight) {
  Max7219Chain const chain{ChainWithOneSegment(2, 0, 0, 0)};

  std::vector<Led> const leds{LitLeds(chain, kMax7219WiringFc16, true)};

  EXPECT_EQ(leds, (std::vector<Led>{Led(0, 7)}));
}

TEST(Max7219Test, ChipAtTheLeftColumnIsTheLastOfTheChain) {
  Max7219Chain const chain{ChainInNormalOperation(2)};

  Max7219 const& chip{chain.ChipAt(0)};

  EXPECT_EQ(&chip, &chain.chips[1]);
}

TEST(Max7219Test, ChipAtTheRightColumnIsTheFirstOfTheChain) {
  Max7219Chain const chain{ChainInNormalOperation(2)};

  Max7219 const& chip{chain.ChipAt(15)};

  EXPECT_EQ(&chip, chain.chips.data());
}

TEST(Max7219Test, ChipAtTheLeftColumnUpsideDownIsTheFirstOfTheChain) {
  Max7219Chain const chain{ChainInNormalOperation(2)};

  Max7219 const& chip{chain.ChipAt(0, true)};

  EXPECT_EQ(&chip, chain.chips.data());
}

}  // namespace
