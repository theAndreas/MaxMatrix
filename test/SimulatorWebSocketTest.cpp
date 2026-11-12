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
#include <stdio.h>

#include <string>

static std::string Sha1Hex(std::string const& message) {
  uint8_t digest[SIMULATOR_WEB_SOCKET_SHA1_LENGTH];
  char hex[3];
  std::string text;

  SimulatorWebSocket::CalculateSha1(message, digest);
  for (uint8_t part : digest) {
    snprintf(hex, sizeof(hex), "%02x", part);
    text += hex;
  }
  return text;
}

static std::string Base64(std::string const& text) {
  return SimulatorWebSocket::ConvertToBase64(
      reinterpret_cast<uint8_t const*>(text.data()), text.size());
}

/* a text frame as a browser sends it: final, masked, and with a mask that is
 * not zeros, which would leave a decoder that unmasks wrongly undetected */
static std::string ClientFrame(std::string const& payload) {
  uint8_t const mask[4] = {0x12, 0x34, 0x56, 0x78};
  std::string frame;

  frame += static_cast<char>(0x81);
  if (payload.size() < 126) {
    frame += static_cast<char>(0x80U | payload.size());
  } else {
    frame += static_cast<char>(0x80U | 126U);
    frame += static_cast<char>(payload.size() >> 8U);
    frame += static_cast<char>(payload.size() & 0xFFU);
  }
  frame.append(reinterpret_cast<char const*>(mask), sizeof(mask));
  for (size_t i = 0; i < payload.size(); i++) {
    frame += static_cast<char>(static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
  }
  return frame;
}

/* the examples of FIPS 180-2, appendix A, the last one two blocks long */
TEST(SimulatorWebSocketTest, Sha1MatchesTheStandardsExamples) {
  EXPECT_EQ(Sha1Hex(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
  EXPECT_EQ(Sha1Hex("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
  EXPECT_EQ(Sha1Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
            "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

/* the examples of RFC 4648, section 10: every length of the last group */
TEST(SimulatorWebSocketTest, Base64MatchesTheRfcExamples) {
  EXPECT_EQ(Base64(""), "");
  EXPECT_EQ(Base64("f"), "Zg==");
  EXPECT_EQ(Base64("fo"), "Zm8=");
  EXPECT_EQ(Base64("foo"), "Zm9v");
  EXPECT_EQ(Base64("foob"), "Zm9vYg==");
  EXPECT_EQ(Base64("fooba"), "Zm9vYmE=");
  EXPECT_EQ(Base64("foobar"), "Zm9vYmFy");
}

/* the example of RFC 6455, section 1.3, which names its answer */
TEST(SimulatorWebSocketTest, AcceptKeyMatchesTheRfcExample) {
  EXPECT_EQ(
      SimulatorWebSocket::ConvertKeyToAcceptKey("dGhlIHNhbXBsZSBub25jZQ=="),
      "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

/* header names are case-insensitive, and a browser may put more than one space
 * after the colon */
TEST(SimulatorWebSocketTest, HandshakeFindsTheKeyInAnyCase) {
  std::string answer;

  ASSERT_TRUE(SimulatorWebSocket::GetHandshakeAnswer(
      "GET /ws HTTP/1.1\r\nSEC-WEBSOCKET-KEY:   "
      "dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n",
      &answer));
  EXPECT_EQ(answer.compare(0, 13, "HTTP/1.1 101 "), 0);
  EXPECT_NE(
      answer.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"),
      std::string::npos);
  EXPECT_EQ(answer.substr(answer.size() - 4), "\r\n\r\n");
}

TEST(SimulatorWebSocketTest, HandshakeWithoutKeyFails) {
  std::string answer;

  EXPECT_FALSE(SimulatorWebSocket::GetHandshakeAnswer(
      "GET /ws HTTP/1.1\r\nHost: localhost\r\n\r\n", &answer));
}

/* 125 bytes is the longest payload whose length fits in the second byte; a
 * panel update is about 400 */
TEST(SimulatorWebSocketTest, EncodeUsesTheShortLengthUpTo125Bytes) {
  std::string const frame =
      SimulatorWebSocket::EncodeFrame(std::string(125, 'x'));

  ASSERT_EQ(frame.size(), 2u + 125u);
  EXPECT_EQ(static_cast<uint8_t>(frame[0]), 0x81);
  EXPECT_EQ(static_cast<uint8_t>(frame[1]), 125);
}

TEST(SimulatorWebSocketTest, EncodeUsesThe16BitLengthFrom126Bytes) {
  std::string const frame =
      SimulatorWebSocket::EncodeFrame(std::string(300, 'x'));

  ASSERT_EQ(frame.size(), 4u + 300u);
  EXPECT_EQ(static_cast<uint8_t>(frame[1]), 126);
  EXPECT_EQ(static_cast<uint8_t>(frame[2]), 300U >> 8U);
  EXPECT_EQ(static_cast<uint8_t>(frame[3]), 300U & 0xFFU);
  EXPECT_EQ(frame.substr(4), std::string(300, 'x'));
}

TEST(SimulatorWebSocketTest, DecodeUnmasksWhatABrowserSends) {
  std::string buffer = ClientFrame("wiring GENERIC");
  std::string payload;
  uint8_t opcode = 0;

  ASSERT_TRUE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_EQ(opcode, SIMULATOR_WEB_SOCKET_OPCODE_TEXT);
  EXPECT_EQ(payload, "wiring GENERIC");
  EXPECT_TRUE(buffer.empty());
}

TEST(SimulatorWebSocketTest, DecodeReadsThe16BitLength) {
  std::string const long_text(200, 'x');
  std::string buffer = ClientFrame(long_text);
  std::string payload;
  uint8_t opcode = 0;

  ASSERT_TRUE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_EQ(payload, long_text);
}

/* a frame may arrive over several reads; until it is whole, nothing is taken
 * from the buffer */
TEST(SimulatorWebSocketTest, DecodeWaitsForTheWholeFrame) {
  std::string const frame = ClientFrame("hello");
  std::string buffer;
  std::string payload;
  uint8_t opcode = 0;

  for (size_t i = 0; i + 1 < frame.size(); i++) {
    buffer += frame[i];
    ASSERT_FALSE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload))
        << "after " << i + 1 << " bytes";
    ASSERT_EQ(buffer.size(), i + 1);
  }
  buffer += frame.back();
  ASSERT_TRUE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_EQ(payload, "hello");
}

/* the page sends "hello" and a command at once when it opens; both have to come
 * out, in order */
TEST(SimulatorWebSocketTest, DecodeTakesOneFrameAtATime) {
  std::string buffer = ClientFrame("hello") + ClientFrame("upside-down 1");
  std::string payload;
  uint8_t opcode = 0;

  ASSERT_TRUE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_EQ(payload, "hello");
  ASSERT_TRUE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_EQ(payload, "upside-down 1");
  EXPECT_FALSE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
}

TEST(SimulatorWebSocketTest, DecodeDropsA64BitLength) {
  std::string buffer = std::string("\x81\xFF", 2) + std::string(12, '\0');
  std::string payload;
  uint8_t opcode = 0;

  EXPECT_FALSE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_TRUE(buffer.empty());
}

TEST(SimulatorWebSocketTest, DecodeReadsWhatEncodeWrites) {
  std::string buffer = SimulatorWebSocket::EncodeFrame(std::string(300, 'y'));
  std::string payload;
  uint8_t opcode = 0;

  ASSERT_TRUE(SimulatorWebSocket::DecodeFrame(&buffer, &opcode, &payload));
  EXPECT_EQ(opcode, SIMULATOR_WEB_SOCKET_OPCODE_TEXT);
  EXPECT_EQ(payload, std::string(300, 'y'));
}
