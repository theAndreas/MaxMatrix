/* Tests of the MAX7219 model in mock/, against the datasheet rather than against the library.
 *
 * The library tests take the model's word for what is lit, so a mistake here would hide one there. */
#include <gtest/gtest.h>
#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>
#include "Max7219.h"

typedef std::pair<uint8_t, uint8_t> PairType;

/* one frame: every pair shifted in, address first, then the rising edge of chip select */
static void sendFrame(Max7219Chain& Chain, std::initializer_list<PairType> Pairs)
{
    for(const PairType& Pair : Pairs) {
        Chain.shift(Pair.first);
        Chain.shift(Pair.second);
    }
    Chain.latch();
}

static Max7219Chain chainInNormalOperation(uint8_t NumberOfChips)
{
    Max7219Chain Chain;
    Chain.reset(NumberOfChips);
    for(uint8_t i = 0; i < NumberOfChips; i++) {
        Chain.Chips[i].Shutdown = 1;
        Chain.Chips[i].ScanLimit = 7;
    }
    return Chain;
}

TEST(Max7219Test, FirstPairSentEndsUpAtTheFarEnd)
{
    Max7219Chain Chain = chainInNormalOperation(3);

    sendFrame(Chain, { {0x01, 0x11}, {0x01, 0x22}, {0x01, 0x33} });
    EXPECT_EQ(Chain.Chips[2].Digit[0], 0x11);
    EXPECT_EQ(Chain.Chips[1].Digit[0], 0x22);
    EXPECT_EQ(Chain.Chips[0].Digit[0], 0x33);
}

/* the chip at the far end still holds a pair of the frame before and latches it again */
TEST(Max7219Test, ShortFrameLatchesWhatIsLeftInTheChain)
{
    Max7219Chain Chain = chainInNormalOperation(2);

    sendFrame(Chain, { {0x01, 0xAA}, {0x02, 0xBB} });
    EXPECT_EQ(Chain.Chips[1].Digit[0], 0xAA);
    EXPECT_EQ(Chain.Chips[0].Digit[1], 0xBB);

    sendFrame(Chain, { {0x03, 0xCC} });
    EXPECT_EQ(Chain.Chips[0].Digit[2], 0xCC);
    EXPECT_EQ(Chain.Chips[1].Digit[1], 0xBB);
}

TEST(Max7219Test, NoOpChangesNothing)
{
    Max7219Chain Chain = chainInNormalOperation(2);

    sendFrame(Chain, { {0x01, 0x11}, {0x01, 0x22} });
    sendFrame(Chain, { {0x00, 0xFF}, {0x00, 0xFF} });
    EXPECT_EQ(Chain.Chips[1].Digit[0], 0x11);
    EXPECT_EQ(Chain.Chips[0].Digit[0], 0x22);
}

TEST(Max7219Test, OnlyTheLowNibbleOfTheAddressCounts)
{
    Max7219Chain Chain = chainInNormalOperation(1);

    sendFrame(Chain, { {0xF1, 0x5A} });
    EXPECT_EQ(Chain.Chips[0].Digit[0], 0x5A);
}

TEST(Max7219Test, PowerUpIsShutDownWithOneDigitAndMinimumIntensity)
{
    Max7219Chain Chain;
    Chain.reset(1);
    const Max7219& Chip = Chain.Chips[0];

    EXPECT_EQ(Chip.Shutdown, 0);
    EXPECT_EQ(Chip.ScanLimit, 0);
    EXPECT_EQ(Chip.DecodeMode, 0);
    EXPECT_EQ(Chip.Intensity, 0);
    EXPECT_EQ(Chip.DisplayTest, 0);

    sendFrame(Chain, { {0x01, 0xFF} });
    EXPECT_EQ(Chip.segments(0), 0x00);
}

TEST(Max7219Test, ShutdownBlanksAndKeepsTheDigits)
{
    Max7219Chain Chain = chainInNormalOperation(1);

    sendFrame(Chain, { {0x01, 0x81} });
    EXPECT_EQ(Chain.Chips[0].segments(0), 0x81);
    sendFrame(Chain, { {0x0C, 0x00} });
    EXPECT_EQ(Chain.Chips[0].segments(0), 0x00);
    sendFrame(Chain, { {0x0C, 0x01} });
    EXPECT_EQ(Chain.Chips[0].segments(0), 0x81);
}

TEST(Max7219Test, DigitsBeyondTheScanLimitAreDark)
{
    Max7219Chain Chain = chainInNormalOperation(1);

    sendFrame(Chain, { {0x03, 0xFF} });
    sendFrame(Chain, { {0x04, 0xFF} });
    sendFrame(Chain, { {0x0B, 0x02} });
    EXPECT_EQ(Chain.Chips[0].segments(2), 0xFF);
    EXPECT_EQ(Chain.Chips[0].segments(3), 0x00);
}

TEST(Max7219Test, DisplayTestLightsEverythingEvenInShutdown)
{
    Max7219Chain Chain;
    Chain.reset(1);

    sendFrame(Chain, { {0x0F, 0x01} });
    for(uint8_t Digit = 0; Digit < Max7219::NumberOfDigits; Digit++) EXPECT_EQ(Chain.Chips[0].segments(Digit), 0xFF);
    sendFrame(Chain, { {0x0F, 0x00} });
    EXPECT_EQ(Chain.Chips[0].segments(0), 0x00);
}

TEST(Max7219Test, CodeBDecodesOnlyTheSelectedDigits)
{
    Max7219Chain Chain = chainInNormalOperation(1);

    sendFrame(Chain, { {0x09, 0x01} });
    sendFrame(Chain, { {0x01, 0x85} });
    sendFrame(Chain, { {0x02, 0x85} });
    EXPECT_EQ(Chain.Chips[0].segments(0), 0x80 | 0x5B);   // DP and '5'
    EXPECT_EQ(Chain.Chips[0].segments(1), 0x85);
}

TEST(Max7219Test, IntensityKeepsFourBits)
{
    Max7219Chain Chain = chainInNormalOperation(1);

    sendFrame(Chain, { {0x0A, 0xF7} });
    EXPECT_EQ(Chain.Chips[0].Intensity, 0x07);
}

static Max7219Chain chainWithOneSegment(uint8_t NumberOfChips, uint8_t Chip, uint8_t Digit, uint8_t Segment)
{
    Max7219Chain Chain = chainInNormalOperation(NumberOfChips);
    Chain.Chips[Chip].Digit[Digit] = 1 << Segment;
    return Chain;
}

/* an LED as the viewer sees it, column X and row Y */
typedef std::pair<uint16_t, uint16_t> LedType;

static std::vector<LedType> litLeds(const Max7219Chain& Chain, const Max7219WiringType& Wiring,
                                   bool UpsideDown = false)
{
    std::vector<LedType> Leds;
    for(uint16_t X = 0; X < Chain.Chips.size() * Max7219::NumberOfDigits; X++) {
        for(uint8_t Y = 0; Y < Max7219::NumberOfDigits; Y++) {
            if(Chain.isLit(Wiring, X, Y, UpsideDown)) Leds.push_back(LedType(X, Y));
        }
    }
    return Leds;
}

/* every digit and segment drives one LED of its own, in every wiring */
TEST(Max7219Test, EveryWiringLightsEveryLedOnce)
{
    for(const Max7219WiringType& Wiring : Max7219Wirings) {
        SCOPED_TRACE(Wiring.Name);
        std::vector<LedType> Seen;
        for(uint8_t Digit = 0; Digit < Max7219::NumberOfDigits; Digit++) {
            for(uint8_t Segment = 0; Segment < 8; Segment++) {
                std::vector<LedType> Leds = litLeds(chainWithOneSegment(1, 0, Digit, Segment), Wiring);
                ASSERT_EQ(Leds.size(), 1u);
                Seen.push_back(Leds[0]);
            }
        }
        std::sort(Seen.begin(), Seen.end());
        EXPECT_EQ(std::unique(Seen.begin(), Seen.end()), Seen.end());
    }
}

/* digit 0, segment bit 0 - where MD_MAX72XX's setPoint() puts it, with its column 0 on the right */
TEST(Max7219Test, NamedWiringsMatchMdMax72xx)
{
    const std::pair<const Max7219WiringType*, LedType> Expected[] = {
        { &MAX7219_WIRING_FC16, LedType(7, 0) },
        { &MAX7219_WIRING_GENERIC, LedType(7, 7) },
        { &MAX7219_WIRING_PAROLA, LedType(0, 0) },
        { &MAX7219_WIRING_ICSTATION, LedType(0, 7) },
    };

    for(const auto& Case : Expected) {
        SCOPED_TRACE(Case.first->Name);
        std::vector<LedType> Leds = litLeds(chainWithOneSegment(1, 0, 0, 0), *Case.first);
        ASSERT_EQ(Leds.size(), 1u);
        EXPECT_EQ(Leds[0], Case.second);
    }
}

TEST(Max7219Test, ChainRunsFromRightToLeft)
{
    Max7219Chain Chain = chainWithOneSegment(2, 0, 0, 0);

    std::vector<LedType> Leds = litLeds(Chain, MAX7219_WIRING_FC16);
    ASSERT_EQ(Leds.size(), 1u);
    EXPECT_EQ(Leds[0], LedType(15, 0));

    /* upside down, the chain starts on the left and the LED is in the opposite corner of its module */
    Leds = litLeds(Chain, MAX7219_WIRING_FC16, true);
    ASSERT_EQ(Leds.size(), 1u);
    EXPECT_EQ(Leds[0], LedType(0, 7));

    EXPECT_EQ(&Chain.chipAt(0), &Chain.Chips[1]);
    EXPECT_EQ(&Chain.chipAt(15), &Chain.Chips[0]);
    EXPECT_EQ(&Chain.chipAt(0, true), &Chain.Chips[0]);
}
