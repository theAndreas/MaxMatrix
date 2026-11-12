// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the MAX7219 model in mock/, against the datasheet rather than
 * against the library.
 *
 * The library tests take the model's word for what is lit, so a mistake here
 * would hide one there. */
#include "Max7219.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <initializer_list>
#include <utility>
#include <vector>

namespace {

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
  EXPECT_EQ(chain.chips[1].digit[0], 0xAA);
  EXPECT_EQ(chain.chips[0].digit[1], 0xBB);

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
    chain.ShiftBit((pair & (1U << bit)) != 0U);
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

  SendFrame(chain, {{0x01, 0xFF}});
  EXPECT_EQ(chip.Segments(0), 0x00);
}

TEST(Max7219Test, ShutdownBlanksAndKeepsTheDigits) {
  Max7219Chain chain{ChainInNormalOperation(1)};

  SendFrame(chain, {{0x01, 0x81}});
  EXPECT_EQ(chain.chips[0].Segments(0), 0x81);
  SendFrame(chain, {{0x0C, 0x00}});
  EXPECT_EQ(chain.chips[0].Segments(0), 0x00);
  SendFrame(chain, {{0x0C, 0x01}});
  EXPECT_EQ(chain.chips[0].Segments(0), 0x81);
}

TEST(Max7219Test, DigitsBeyondTheScanLimitAreDark) {
  Max7219Chain chain{ChainInNormalOperation(1)};

  SendFrame(chain, {{0x03, 0xFF}});
  SendFrame(chain, {{0x04, 0xFF}});
  SendFrame(chain, {{0x0B, 0x02}});
  EXPECT_EQ(chain.chips[0].Segments(2), 0xFF);
  EXPECT_EQ(chain.chips[0].Segments(3), 0x00);
}

TEST(Max7219Test, DisplayTestLightsEverythingEvenInShutdown) {
  Max7219Chain chain;
  chain.Reset(1);

  SendFrame(chain, {{0x0F, 0x01}});
  for (uint8_t digit{0}; digit < Max7219::kNumberOfDigits; digit++) {
    EXPECT_EQ(chain.chips[0].Segments(digit), 0xFF);
  }
  SendFrame(chain, {{0x0F, 0x00}});
  EXPECT_EQ(chain.chips[0].Segments(0), 0x00);
}

TEST(Max7219Test, CodeBDecodesOnlyTheSelectedDigits) {
  Max7219Chain chain{ChainInNormalOperation(1)};

  SendFrame(chain, {{0x09, 0x01}});
  SendFrame(chain, {{0x01, 0x85}});
  SendFrame(chain, {{0x02, 0x85}});
  EXPECT_EQ(chain.chips[0].Segments(0), 0x80U | 0x5BU);  // DP and '5'
  EXPECT_EQ(chain.chips[0].Segments(1), 0x85);
}

TEST(Max7219Test, IntensityKeepsFourBits) {
  Max7219Chain chain{ChainInNormalOperation(1)};

  SendFrame(chain, {{0x0A, 0xF7}});
  EXPECT_EQ(chain.chips[0].intensity, 0x07);
}

Max7219Chain ChainWithOneSegment(uint8_t number_of_chips, uint8_t chip,
                                 uint8_t digit, uint8_t segment) {
  Max7219Chain chain{ChainInNormalOperation(number_of_chips)};
  chain.chips[chip].digit[digit] = static_cast<uint8_t>(1U << segment);
  return chain;
}

/* an LED as the viewer sees it, column X and row Y */
using Led = std::pair<uint16_t, uint16_t>;

std::vector<Led> LitLeds(Max7219Chain const& chain, Max7219Wiring const& wiring,
                         bool upside_down = false) {
  std::vector<Led> leds;
  for (uint16_t x{0}; x < chain.chips.size() * Max7219::kNumberOfDigits; x++) {
    for (uint8_t y{0}; y < Max7219::kNumberOfDigits; y++) {
      if (chain.IsLit(wiring, x, y, upside_down)) {
        leds.emplace_back(x, y);
      }
    }
  }
  return leds;
}

/* every digit and segment drives one LED of its own, in every wiring */
TEST(Max7219Test, EveryWiringLightsEveryLedOnce) {
  for (Max7219Wiring const& wiring : kMax7219Wirings) {
    SCOPED_TRACE(wiring.name);
    std::vector<Led> seen;
    for (uint8_t digit{0}; digit < Max7219::kNumberOfDigits; digit++) {
      for (uint8_t segment{0}; segment < 8; segment++) {
        std::vector<Led> leds =
            LitLeds(ChainWithOneSegment(1, 0, digit, segment), wiring);
        ASSERT_EQ(leds.size(), 1U);
        seen.push_back(leds[0]);
      }
    }
    std::sort(seen.begin(), seen.end());
    EXPECT_EQ(std::unique(seen.begin(), seen.end()), seen.end());
  }
}

/* digit 0, segment bit 0 - where MD_MAX72XX's setPoint() puts it, with its
 * column 0 on the right */
TEST(Max7219Test, NamedWiringsMatchMdMax72xx) {
  std::array<std::pair<Max7219Wiring const*, Led>, 4> const expected{{
      {&kMaX7219WiringFC16, Led(7, 0)},
      {&kMaX7219WiringGeneric, Led(7, 7)},
      {&kMaX7219WiringParola, Led(0, 0)},
      {&kMaX7219WiringIcstation, Led(0, 7)},
  }};

  for (auto const& test_case : expected) {
    SCOPED_TRACE(test_case.first->name);
    std::vector<Led> leds =
        LitLeds(ChainWithOneSegment(1, 0, 0, 0), *test_case.first);
    ASSERT_EQ(leds.size(), 1U);
    EXPECT_EQ(leds[0], test_case.second);
  }
}

TEST(Max7219Test, ChainRunsFromRightToLeft) {
  Max7219Chain chain{ChainWithOneSegment(2, 0, 0, 0)};

  std::vector<Led> leds = LitLeds(chain, kMaX7219WiringFC16);
  ASSERT_EQ(leds.size(), 1U);
  EXPECT_EQ(leds[0], Led(15, 0));

  /* upside down, the chain starts on the left and the LED is in the opposite
   * corner of its module */
  leds = LitLeds(chain, kMaX7219WiringFC16, true);
  ASSERT_EQ(leds.size(), 1U);
  EXPECT_EQ(leds[0], Led(0, 7));

  EXPECT_EQ(&chain.ChipAt(0), &chain.chips[1]);
  EXPECT_EQ(&chain.ChipAt(15), chain.chips.data());
  EXPECT_EQ(&chain.ChipAt(0, true), chain.chips.data());
}

}  // namespace
