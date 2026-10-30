/* Tests of the simulator's WebSocket protocol, against the RFCs rather than against a browser.
 *
 * simulator/WebTest.sh talks to a running simulator over a real socket, but only with short commands and one key. What
 * a browser could send beyond that - another key, a long message, a frame split across reads - is checked here. */
#include <gtest/gtest.h>
#include <stdio.h>
#include <string>
#include "SimulatorWebSocket.h"

static std::string sha1Hex(const std::string& Message)
{
    uint8_t Digest[SIMULATOR_WEB_SOCKET_SHA1_LENGTH];
    char Hex[3];
    std::string Text;

    SimulatorWebSocket::calculateSha1(Message, Digest);
    for(uint8_t Byte : Digest) {
        snprintf(Hex, sizeof(Hex), "%02x", Byte);
        Text += Hex;
    }
    return Text;
}

static std::string base64(const std::string& Text)
{
    return SimulatorWebSocket::convertToBase64(reinterpret_cast<const uint8_t*>(Text.data()), Text.size());
}

/* a text frame as a browser sends it: final, masked, and with a mask that is not zeros, which would leave a decoder
 * that unmasks wrongly undetected */
static std::string clientFrame(const std::string& Payload)
{
    const uint8_t Mask[4] = { 0x12, 0x34, 0x56, 0x78 };
    std::string Frame;

    Frame += static_cast<char>(0x81);
    if(Payload.size() < 126) {
        Frame += static_cast<char>(0x80 | Payload.size());
    } else {
        Frame += static_cast<char>(0x80 | 126);
        Frame += static_cast<char>(Payload.size() >> 8);
        Frame += static_cast<char>(Payload.size() & 0xFF);
    }
    Frame.append(reinterpret_cast<const char*>(Mask), sizeof(Mask));
    for(size_t i = 0; i < Payload.size(); i++) Frame += static_cast<char>(Payload[i] ^ Mask[i % 4]);
    return Frame;
}

/* the examples of FIPS 180-2, appendix A, the last one two blocks long */
TEST(SimulatorWebSocketTest, Sha1MatchesTheStandardsExamples)
{
    EXPECT_EQ(sha1Hex(""), "da39a3ee5e6b4b0d3255bfef95601890afd80709");
    EXPECT_EQ(sha1Hex("abc"), "a9993e364706816aba3e25717850c26c9cd0d89d");
    EXPECT_EQ(sha1Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
}

/* the examples of RFC 4648, section 10: every length of the last group */
TEST(SimulatorWebSocketTest, Base64MatchesTheRfcExamples)
{
    EXPECT_EQ(base64(""), "");
    EXPECT_EQ(base64("f"), "Zg==");
    EXPECT_EQ(base64("fo"), "Zm8=");
    EXPECT_EQ(base64("foo"), "Zm9v");
    EXPECT_EQ(base64("foob"), "Zm9vYg==");
    EXPECT_EQ(base64("fooba"), "Zm9vYmE=");
    EXPECT_EQ(base64("foobar"), "Zm9vYmFy");
}

/* the example of RFC 6455, section 1.3, which names its answer */
TEST(SimulatorWebSocketTest, AcceptKeyMatchesTheRfcExample)
{
    EXPECT_EQ(SimulatorWebSocket::convertKeyToAcceptKey("dGhlIHNhbXBsZSBub25jZQ=="), "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
}

/* header names are case-insensitive, and a browser may put more than one space after the colon */
TEST(SimulatorWebSocketTest, HandshakeFindsTheKeyInAnyCase)
{
    std::string Answer;

    ASSERT_EQ(SimulatorWebSocket::getHandshakeAnswer("GET /ws HTTP/1.1\r\nSEC-WEBSOCKET-KEY:   dGhlIHNhbXBsZSBub25jZQ==\r\n\r\n", &Answer), E_OK);
    EXPECT_EQ(Answer.compare(0, 13, "HTTP/1.1 101 "), 0);
    EXPECT_NE(Answer.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"), std::string::npos);
    EXPECT_EQ(Answer.substr(Answer.size() - 4), "\r\n\r\n");
}

TEST(SimulatorWebSocketTest, HandshakeWithoutKeyFails)
{
    std::string Answer;

    EXPECT_EQ(SimulatorWebSocket::getHandshakeAnswer("GET /ws HTTP/1.1\r\nHost: localhost\r\n\r\n", &Answer), E_NOT_OK);
}

/* 125 bytes is the longest payload whose length fits in the second byte; a panel update is about 400 */
TEST(SimulatorWebSocketTest, EncodeUsesTheShortLengthUpTo125Bytes)
{
    const std::string Frame = SimulatorWebSocket::encodeFrame(std::string(125, 'x'));

    ASSERT_EQ(Frame.size(), 2u + 125u);
    EXPECT_EQ(static_cast<uint8_t>(Frame[0]), 0x81);
    EXPECT_EQ(static_cast<uint8_t>(Frame[1]), 125);
}

TEST(SimulatorWebSocketTest, EncodeUsesThe16BitLengthFrom126Bytes)
{
    const std::string Frame = SimulatorWebSocket::encodeFrame(std::string(300, 'x'));

    ASSERT_EQ(Frame.size(), 4u + 300u);
    EXPECT_EQ(static_cast<uint8_t>(Frame[1]), 126);
    EXPECT_EQ(static_cast<uint8_t>(Frame[2]), 300 >> 8);
    EXPECT_EQ(static_cast<uint8_t>(Frame[3]), 300 & 0xFF);
    EXPECT_EQ(Frame.substr(4), std::string(300, 'x'));
}

TEST(SimulatorWebSocketTest, DecodeUnmasksWhatABrowserSends)
{
    std::string Buffer = clientFrame("wiring GENERIC");
    std::string Payload;
    uint8_t Opcode = 0;

    ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_OK);
    EXPECT_EQ(Opcode, SIMULATOR_WEB_SOCKET_OPCODE_TEXT);
    EXPECT_EQ(Payload, "wiring GENERIC");
    EXPECT_TRUE(Buffer.empty());
}

TEST(SimulatorWebSocketTest, DecodeReadsThe16BitLength)
{
    const std::string Long(200, 'x');
    std::string Buffer = clientFrame(Long);
    std::string Payload;
    uint8_t Opcode = 0;

    ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_OK);
    EXPECT_EQ(Payload, Long);
}

/* a frame may arrive over several reads; until it is whole, nothing is taken from the buffer */
TEST(SimulatorWebSocketTest, DecodeWaitsForTheWholeFrame)
{
    const std::string Frame = clientFrame("hello");
    std::string Buffer;
    std::string Payload;
    uint8_t Opcode = 0;

    for(size_t i = 0; i + 1 < Frame.size(); i++) {
        Buffer += Frame[i];
        ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_NOT_OK) << "after " << i + 1 << " bytes";
        ASSERT_EQ(Buffer.size(), i + 1);
    }
    Buffer += Frame.back();
    ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_OK);
    EXPECT_EQ(Payload, "hello");
}

/* the page sends "hello" and a command at once when it opens; both have to come out, in order */
TEST(SimulatorWebSocketTest, DecodeTakesOneFrameAtATime)
{
    std::string Buffer = clientFrame("hello") + clientFrame("upside-down 1");
    std::string Payload;
    uint8_t Opcode = 0;

    ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_OK);
    EXPECT_EQ(Payload, "hello");
    ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_OK);
    EXPECT_EQ(Payload, "upside-down 1");
    EXPECT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_NOT_OK);
}

TEST(SimulatorWebSocketTest, DecodeDropsA64BitLength)
{
    std::string Buffer = std::string("\x81\xFF", 2) + std::string(12, '\0');
    std::string Payload;
    uint8_t Opcode = 0;

    EXPECT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_NOT_OK);
    EXPECT_TRUE(Buffer.empty());
}

TEST(SimulatorWebSocketTest, DecodeReadsWhatEncodeWrites)
{
    std::string Buffer = SimulatorWebSocket::encodeFrame(std::string(300, 'y'));
    std::string Payload;
    uint8_t Opcode = 0;

    ASSERT_EQ(SimulatorWebSocket::decodeFrame(&Buffer, &Opcode, &Payload), E_OK);
    EXPECT_EQ(Opcode, SIMULATOR_WEB_SOCKET_OPCODE_TEXT);
    EXPECT_EQ(Payload, std::string(300, 'y'));
}
