// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Tests of the simulator's WebSocket protocol, against the RFCs rather than
 * against a browser.
 *
 * simulator/Test/WebTest.sh talks to a running simulator over a real socket,
 * but only with short commands and one key. What a browser could send beyond
 * that - another key, a long message, a frame split across reads - is checked
 * here. */
#include "Host/SimulatorWebSocket.h"

#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <array>
#include <iomanip>
#include <ios>
#include <sstream>
#include <string>

#include "CaseName.h"
#include "MaxMatrixAttributes.h"
#include "Utility/Bit.h"

namespace {

using ByteBits = max_matrix::Bit<uint8_t>;
using SizeBits = max_matrix::Bit<size_t>;

/* the bit of the second byte that says the frame is masked */
constexpr uint8_t kMasked{0x80u};
constexpr uint8_t kByte{0xFFu};
constexpr uint8_t kBitsPerByte{8u};

std::string Sha1Hex(std::string const& message) {
  std::ostringstream text;

  for (uint8_t const part : SimulatorWebSocket::CalculateSha1(message)) {
    text << std::hex << std::setw(2) << std::setfill('0')
         << static_cast<uint16_t>(part);
  }
  return text.str();
}

std::string Base64(std::string const& text) {
  return SimulatorWebSocket::ConvertToBase64(text);
}

/* a text frame as a browser sends it: final, masked, and with a mask that is
 * not zeros, which would leave a decoder that unmasks wrongly undetected */
std::string ClientFrame(std::string const& payload) {
  std::array<uint8_t, 4> const mask{{0x12, 0x34, 0x56, 0x78}};
  std::string frame;

  frame += static_cast<char>(0x81);
  if (payload.size() < 126) {
    frame += static_cast<char>(SizeBits::Combine(kMasked, payload.size()));
  } else {
    frame += static_cast<char>(ByteBits::Combine(kMasked, 126u));
    frame += static_cast<char>(
        SizeBits::ReadBitGroup(payload.size(), kByte, kBitsPerByte));
    frame += static_cast<char>(SizeBits::ReadBitGroup(payload.size(), kByte));
  }
  frame.append(mask.begin(), mask.end());
  for (size_t i{0}; i < payload.size(); i++) {
    frame += static_cast<char>(
        ByteBits::ToggleBits(static_cast<uint8_t>(payload[i]), mask[i % 4]));
  }
  return frame;
}

/* an example of a standard: what goes in, and what has to come out */
struct Example {
  char const* name;
  char const* input;
  char const* output;
};

/* Decodes what `buffer` holds as far as a test needs it taken before the call
 * it tests; what it found is no part of the test. */
void TakeFrame(std::string& buffer) {
  max_matrix::Discard(SimulatorWebSocket::DecodeFrame(buffer));
}

/* the examples of FIPS 180-2, appendix A, the last one two blocks long */
constexpr std::array<Example, 3> kSha1Examples{{
    {"Empty", "", "da39a3ee5e6b4b0d3255bfef95601890afd80709"},
    {"OneBlock", "abc", "a9993e364706816aba3e25717850c26c9cd0d89d"},
    {"TwoBlocks", "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
     "84983e441c3bd26ebaae4aa1f95129e5e54670f1"},
}};

class Sha1Test : public ::testing::TestWithParam<Example> {};

INSTANTIATE_TEST_SUITE_P(Fips180, Sha1Test, ::testing::ValuesIn(kSha1Examples),
                         CaseName<Example>);

TEST_P(Sha1Test, MatchesTheStandardsExample) {
  std::string const hash{Sha1Hex(GetParam().input)};

  EXPECT_EQ(hash, GetParam().output);
}

/* the examples of RFC 4648, section 10: every length of the last group */
constexpr std::array<Example, 7> kBase64Examples{{
    {"Empty", "", ""},
    {"OneByte", "f", "Zg=="},
    {"TwoBytes", "fo", "Zm8="},
    {"ThreeBytes", "foo", "Zm9v"},
    {"FourBytes", "foob", "Zm9vYg=="},
    {"FiveBytes", "fooba", "Zm9vYmE="},
    {"SixBytes", "foobar", "Zm9vYmFy"},
}};

class Base64Test : public ::testing::TestWithParam<Example> {};

INSTANTIATE_TEST_SUITE_P(Rfc4648, Base64Test,
                         ::testing::ValuesIn(kBase64Examples),
                         CaseName<Example>);

TEST_P(Base64Test, MatchesTheRfcExample) {
  std::string const text{Base64(GetParam().input)};

  EXPECT_EQ(text, GetParam().output);
}

/* the example of RFC 6455, section 1.3, which names its answer */
TEST(SimulatorWebSocketTest, AcceptKeyMatchesTheRfcExample) {
  std::string const accept_key{
      SimulatorWebSocket::ConvertKeyToAcceptKey("dGhlIHNhbXBsZSBub25jZQ==")};

  EXPECT_EQ(accept_key, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

/* header names are case-insensitive, and a browser may put more than one space
 * after the colon */
TEST(SimulatorWebSocketTest, HandshakeFindsTheKeyInAnyCase) {
  SimulatorFoundText const found{SimulatorWebSocket::GetHandshakeAnswer(
      "GET /ws HTTP/1.1\r\nSEC-WEBSOCKET-KEY:   "
      "dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n")};

  std::string const& answer{found.text};
  ASSERT_TRUE(found.found);
  EXPECT_EQ(answer.compare(0, 13, "HTTP/1.1 101 "), 0);
  EXPECT_NE(
      answer.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"),
      std::string::npos);
  EXPECT_EQ(answer.substr(answer.size() - 4), "\r\n\r\n");
}

TEST(SimulatorWebSocketTest, HandshakeWithoutKeyFails) {
  SimulatorFoundText const found{SimulatorWebSocket::GetHandshakeAnswer(
      "GET /ws HTTP/1.1\r\nHost: localhost\r\n\r\n")};

  EXPECT_FALSE(found.found);
}

/* 125 bytes is the longest payload whose length fits in the second byte; a
 * panel update is about 400 */
TEST(SimulatorWebSocketTest, EncodeUsesTheShortLengthUpTo125Bytes) {
  std::string const frame{
      SimulatorWebSocket::EncodeFrame(std::string(125, 'x'))};

  ASSERT_EQ(frame.size(), 2u + 125u);
  EXPECT_EQ(static_cast<uint8_t>(frame[0]), 0x81);
  EXPECT_EQ(static_cast<uint8_t>(frame[1]), 125);
}

TEST(SimulatorWebSocketTest, EncodeUsesThe16BitLengthFrom126Bytes) {
  std::string const frame{
      SimulatorWebSocket::EncodeFrame(std::string(300, 'x'))};

  ASSERT_EQ(frame.size(), 4u + 300u);
  EXPECT_EQ(static_cast<uint8_t>(frame[1]), 126);
  // 300 is 0x012C, the high byte first
  EXPECT_EQ(static_cast<uint8_t>(frame[2]), 0x01u);
  EXPECT_EQ(static_cast<uint8_t>(frame[3]), 0x2Cu);
  EXPECT_EQ(frame.substr(4), std::string(300, 'x'));
}

TEST(SimulatorWebSocketTest, EncodeUsesThe16BitLengthAt126Bytes) {
  std::string const frame{
      SimulatorWebSocket::EncodeFrame(std::string(126, 'x'))};

  ASSERT_EQ(frame.size(), 4u + 126u);
  EXPECT_EQ(static_cast<uint8_t>(frame[1]), 126);
  EXPECT_EQ(static_cast<uint8_t>(frame[2]), 0x00u);
  EXPECT_EQ(static_cast<uint8_t>(frame[3]), 0x7Eu);
}

TEST(SimulatorWebSocketTest, EncodeUsesThe16BitLengthUpTo65535Bytes) {
  std::string const frame{
      SimulatorWebSocket::EncodeFrame(std::string(65535, 'x'))};

  ASSERT_EQ(frame.size(), 4u + 65535u);
  EXPECT_EQ(static_cast<uint8_t>(frame[1]), 126);
  EXPECT_EQ(static_cast<uint8_t>(frame[2]), 0xFFu);
  EXPECT_EQ(static_cast<uint8_t>(frame[3]), 0xFFu);
}

TEST(SimulatorWebSocketTest, DecodeUnmasksWhatABrowserSends) {
  std::string buffer{ClientFrame("wiring GENERIC")};

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  ASSERT_TRUE(frame.complete);
  EXPECT_EQ(frame.opcode, kSimulatorWebSocketOpcodeText);
  EXPECT_EQ(frame.payload, "wiring GENERIC");
  EXPECT_TRUE(buffer.empty());
}

TEST(SimulatorWebSocketTest, DecodeReadsThe16BitLength) {
  std::string const long_text(200, 'x');
  std::string buffer{ClientFrame(long_text)};

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  ASSERT_TRUE(frame.complete);
  EXPECT_EQ(frame.payload, long_text);
}

/* A frame may arrive over several reads; until it is whole, nothing is taken
 * from the buffer. "hello" is a frame of 11 bytes: two of the header, four of
 * the mask and five of the text. */
TEST(SimulatorWebSocketTest, DecodeWaitsForTheMaskAfterTheHeader) {
  std::string buffer{ClientFrame("hello").substr(0, 2)};

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  EXPECT_FALSE(frame.complete);
  EXPECT_EQ(buffer.size(), 2u);
}

TEST(SimulatorWebSocketTest, DecodeWaitsForTheLastByte) {
  std::string buffer{ClientFrame("hello").substr(0, 10)};

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  EXPECT_FALSE(frame.complete);
  EXPECT_EQ(buffer.size(), 10u);
}

TEST(SimulatorWebSocketTest, DecodeTakesAFrameALaterReadCompletes) {
  std::string const sent{ClientFrame("hello")};
  std::string buffer{sent.substr(0, 10)};
  TakeFrame(buffer);
  buffer += sent.substr(10);

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  ASSERT_TRUE(frame.complete);
  EXPECT_EQ(frame.payload, "hello");
}

/* The page sends "hello" and a command at once when it opens; both have to come
 * out, in order, one frame per call. */
TEST(SimulatorWebSocketTest, DecodeTakesTheFirstFrameFirst) {
  std::string buffer{ClientFrame("hello") + ClientFrame("upside-down 1")};

  SimulatorWebSocketFrame const first{SimulatorWebSocket::DecodeFrame(buffer)};

  ASSERT_TRUE(first.complete);
  EXPECT_EQ(first.payload, "hello");
}

TEST(SimulatorWebSocketTest, DecodeTakesTheSecondFrameNext) {
  std::string buffer{ClientFrame("hello") + ClientFrame("upside-down 1")};
  TakeFrame(buffer);

  SimulatorWebSocketFrame const second{SimulatorWebSocket::DecodeFrame(buffer)};

  ASSERT_TRUE(second.complete);
  EXPECT_EQ(second.payload, "upside-down 1");
}

TEST(SimulatorWebSocketTest, DecodeOfTheTakenFramesLeavesNothing) {
  std::string buffer{ClientFrame("hello") + ClientFrame("upside-down 1")};
  TakeFrame(buffer);
  TakeFrame(buffer);

  SimulatorWebSocketFrame const none{SimulatorWebSocket::DecodeFrame(buffer)};

  EXPECT_FALSE(none.complete);
}

TEST(SimulatorWebSocketTest, DecodeDropsA64BitLength) {
  std::string buffer{std::string("\x81\xFF", 2) + std::string(12, '\0')};

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  EXPECT_FALSE(frame.complete);
  EXPECT_TRUE(buffer.empty());
}

TEST(SimulatorWebSocketTest, DecodeReadsWhatEncodeWrites) {
  std::string buffer{SimulatorWebSocket::EncodeFrame(std::string(300, 'y'))};

  SimulatorWebSocketFrame const frame{SimulatorWebSocket::DecodeFrame(buffer)};

  ASSERT_TRUE(frame.complete);
  EXPECT_EQ(frame.opcode, kSimulatorWebSocketOpcodeText);
  EXPECT_EQ(frame.payload, std::string(300, 'y'));
}

}  // namespace
