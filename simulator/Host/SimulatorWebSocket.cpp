// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The WebSocket protocol of the simulator's web page.
 *
 * Ported from Wordclock's platform/simulator/src/WebHost.cpp. SHA-1 and base64
 * are written out rather than pulled in as a dependency, as there: a checksum a
 * browser demands, not a security measure.
 */

#include "SimulatorWebSocket.h"

#include <ctype.h>

#include <array>
#include <string>

namespace {

/* handshake, RFC 6455 section 1.3: the server hashes the client's key with this
 * fixed string */
constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr char kKeyHeader[] = "sec-websocket-key:";

/* frame, RFC 6455 section 5.2 */
constexpr uint8_t kFinalTextFrame{0x81U};
constexpr uint8_t kOpcodeMask{0x0FU};
constexpr uint8_t kMaskFlag{0x80U};
constexpr size_t kMaskLength{4U};
constexpr uint8_t kLengthMask{0x7FU};
constexpr uint8_t kLength16Bit{126U};
constexpr uint8_t kLength64Bit{127U};
constexpr size_t kHeaderLength{2U};
constexpr size_t kHeaderLength16Bit{4U};

/* a byte of a wider number, as a frame and a hash write them */
constexpr uint8_t kBitsPerByte{8U};
constexpr uint32_t kByteMask{0xFFU};

/* SHA-1, FIPS 180-4: blocks of 64 bytes, of 16 words; the 1 bit padding
 * starts with, and the length of the message in 8 bytes at the end */
constexpr size_t kSha1BlockLength{64U};
constexpr size_t kSha1LengthPosition{56U};
constexpr uint8_t kSha1WordsPerBlock{16U};
constexpr uint8_t kSha1BytesPerWord{4U};
constexpr uint8_t kSha1PadBit{0x80U};
constexpr uint8_t kSha1LengthBytes{8U};
constexpr uint8_t kWordBits{32U};
/* 80 rounds in four stages of 20; the message schedule takes the words 3, 8,
 * 14 and 16 back; a round turns its words by 5 and 30 bits */
constexpr uint8_t kSha1Rounds{80U};
constexpr uint8_t kSha1RoundsPerStage{20U};
constexpr uint8_t kSha1TapB{8U};
constexpr uint8_t kSha1TapC{14U};
constexpr uint8_t kSha1TurnA{5U};
constexpr uint8_t kSha1TurnB{30U};

/* base64, RFC 4648: a char of six bits, four chars for three bytes */
constexpr uint8_t kBase64Bits{6U};
constexpr uint32_t kBase64Mask{0x3FU};

// Turns the bytes of `payload` back with the four of the client's mask, which
// lie at `mask_at` of `frame`, RFC 6455 section 5.3.
void Unmask(std::string const& frame, size_t mask_at, std::string* payload) {
  for (size_t i{0}; i < payload->size(); i++) {
    (*payload)[i] = static_cast<char>(
        static_cast<uint8_t>((*payload)[i]) ^
        static_cast<uint8_t>(frame[mask_at + (i % kMaskLength)]));
  }
}

// Returns the byte at `index` of `bytes` as a number, as a char may be
// negative.
uint32_t GetByte(std::string const& bytes, size_t index) {
  return static_cast<uint8_t>(bytes[index]);
}

}  // namespace

bool SimulatorWebSocket::GetHandshakeAnswer(std::string const& request,
                                            std::string* answer) {
  std::string key;

  if (GetKey(request, &key)) {
    *answer =
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: "
        "Upgrade\r\n"
        "Sec-WebSocket-Accept: " +
        ConvertKeyToAcceptKey(key) + "\r\n\r\n";
    return true;
  }
  return false;
}

std::string SimulatorWebSocket::EncodeFrame(std::string const& payload) {
  std::string frame;

  frame += static_cast<char>(kFinalTextFrame);
  if (payload.size() < kLength16Bit) {
    frame += static_cast<char>(payload.size());
  } else {
    frame += static_cast<char>(kLength16Bit);
    frame += static_cast<char>((payload.size() >> kBitsPerByte) & kByteMask);
    frame += static_cast<char>(payload.size() & kByteMask);
  }
  return frame + payload;
}

bool SimulatorWebSocket::DecodeFrame(std::string* buffer, uint8_t* opcode,
                                     std::string* payload) {
  std::string const& frame{*buffer};

  if (frame.size() < kHeaderLength) {
    return false;
  }

  size_t const mask_length{
      ((static_cast<uint8_t>(frame[1]) & kMaskFlag) != 0) ? kMaskLength : 0};
  size_t length{
      static_cast<size_t>(static_cast<uint8_t>(frame[1]) & kLengthMask)};
  size_t offset{kHeaderLength};

  if (length == kLength64Bit) {
    /* no command is this long; dropping what arrived is better than trying to
     * read it */
    buffer->clear();
    return false;
  }
  if (length == kLength16Bit) {
    if (frame.size() < kHeaderLength16Bit) {
      return false;
    }
    length =
        (static_cast<size_t>(static_cast<uint8_t>(frame[2])) << kBitsPerByte) |
        static_cast<uint8_t>(frame[3]);
    offset = kHeaderLength16Bit;
  }
  if (frame.size() < offset + mask_length + length) {
    return false;
  }

  payload->assign(frame, offset + mask_length, length);
  if (mask_length > 0) {
    Unmask(frame, offset, payload);
  }
  *opcode = static_cast<uint8_t>(frame[0]) & kOpcodeMask;
  buffer->erase(0, offset + mask_length + length);
  return true;
}

// The accept key is the base64 of the SHA-1 of the key and the fixed GUID of
// RFC 6455.
std::string SimulatorWebSocket::ConvertKeyToAcceptKey(std::string const& key) {
  SimulatorSha1Digest const digest{CalculateSha1(key + kGuid)};

  return ConvertToBase64(std::string(digest.begin(), digest.end()));
}

// Pads the message and processes it block by block, as FIPS 180-4 describes.
SimulatorSha1Digest SimulatorWebSocket::CalculateSha1(
    std::string const& message) {
  static constexpr Sha1State kInitialState{
      {0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U}};
  Sha1State state{kInitialState};
  std::string const padded = PadSha1Message(message);
  SimulatorSha1Digest digest{};

  for (size_t offset{0}; offset < padded.size(); offset += kSha1BlockLength) {
    state = ProcessSha1Block(padded, offset, state);
  }
  for (uint8_t word{0}; word < state.size(); word++) {
    for (uint8_t part{0}; part < kSha1BytesPerWord; part++) {
      auto const bytes_after{
          static_cast<uint8_t>(kSha1BytesPerWord - 1U - part)};
      digest[(word * size_t{kSha1BytesPerWord}) + part] = static_cast<uint8_t>(
          state[word] >> (uint32_t{kBitsPerByte} * bytes_after));
    }
  }
  return digest;
}

std::string SimulatorWebSocket::ConvertToBase64(std::string const& bytes) {
  static char const kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;

  for (size_t offset{0}; offset < bytes.size(); offset += 3) {
    size_t const remaining{bytes.size() - offset};
    uint32_t group{GetByte(bytes, offset) << (2U * kBitsPerByte)};

    if (remaining > 1) {
      group |= GetByte(bytes, offset + 1) << kBitsPerByte;
    }
    if (remaining > 2) {
      group |= GetByte(bytes, offset + 2);
    }
    encoded += kAlphabet[(group >> (3U * kBase64Bits)) & kBase64Mask];
    encoded += kAlphabet[(group >> (2U * kBase64Bits)) & kBase64Mask];
    encoded +=
        remaining > 1 ? kAlphabet[(group >> kBase64Bits) & kBase64Mask] : '=';
    encoded += remaining > 2 ? kAlphabet[group & kBase64Mask] : '=';
  }
  return encoded;
}

/*
 * Finds the browser's key in `request`, the whole HTTP request with its
 * headers, and gives the value of Sec-WebSocket-Key in `key`. Returns false if
 * the request has no such header. It searches a lowered copy of the request, as
 * header names are case-insensitive, and takes the value from the request
 * itself.
 */
bool SimulatorWebSocket::GetKey(std::string const& request, std::string* key) {
  std::string lowered(request);

  for (char& c : lowered) {
    c = static_cast<char>(tolower(static_cast<uint8_t>(c)));
  }
  size_t const found{lowered.find(kKeyHeader)};
  if (found == std::string::npos) {
    return false;
  }

  size_t start{found + sizeof(kKeyHeader) - 1};
  while (start < request.size() && request[start] == ' ') {
    start++;
  }
  size_t const end{request.find("\r\n", start)};
  if (end == std::string::npos) {
    return false;
  }

  *key = request.substr(start, end - start);
  return true;
}

/*
 * Pads `message`, the bytes to hash, to whole blocks: appends a 1 bit, zeros up
 * to eight bytes short of a block, and the length of the message in bits, big
 * endian. Returns the padded message, a multiple of
 * kSha1BlockLength long.
 */
std::string SimulatorWebSocket::PadSha1Message(std::string const& message) {
  uint64_t const bits{static_cast<uint64_t>(message.size()) * kBitsPerByte};
  std::string padded(message);

  padded.push_back(static_cast<char>(kSha1PadBit));
  while (padded.size() % kSha1BlockLength != kSha1LengthPosition) {
    padded.push_back('\0');
  }
  for (uint8_t i{1}; i <= kSha1LengthBytes; i++) {
    auto const shift{
        static_cast<uint8_t>(kBitsPerByte * (kSha1LengthBytes - i))};
    padded.push_back(static_cast<char>((bits >> shift) & kByteMask));
  }
  return padded;
}

/*
 * Returns `state`, the five words of the hash so far, with the block of
 * kSha1BlockLength bytes at `offset` of the padded message mixed in: expands
 * the block into the message schedule of 80 words and runs the 80 rounds.
 */
SimulatorWebSocket::Sha1State SimulatorWebSocket::ProcessSha1Block(
    std::string const& padded, size_t offset, Sha1State state) {
  std::array<uint32_t, kSha1Rounds> word{};
  uint32_t a{state[0]};
  uint32_t b{state[1]};
  uint32_t c{state[2]};
  uint32_t d{state[3]};
  uint32_t e{state[4]};

  for (uint8_t i{0}; i < kSha1WordsPerBlock; i++) {
    for (uint8_t part{0}; part < kSha1BytesPerWord; part++) {
      size_t const at{offset + (size_t{kSha1BytesPerWord} * i) + part};
      word[i] = (word[i] << kBitsPerByte) | GetByte(padded, at);
    }
  }
  for (uint8_t i{kSha1WordsPerBlock}; i < kSha1Rounds; i++) {
    uint32_t const mixed{word[i - 3] ^ word[i - kSha1TapB] ^
                         word[i - kSha1TapC] ^ word[i - kSha1WordsPerBlock]};
    word[i] = RotateLeft(mixed, 1);
  }
  for (uint8_t round{0}; round < kSha1Rounds; round++) {
    uint32_t const next{RotateLeft(a, kSha1TurnA) + MixSha1(round, b, c, d) +
                        e + word[round]};
    e = d;
    d = c;
    c = RotateLeft(b, kSha1TurnB);
    b = a;
    a = next;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
  return state;
}

/*
 * Returns the function and the constant of `round`, 0 to 79, added: the round's
 * function of `b`, `c` and `d`, the second, third and fourth word of the
 * working state, plus its constant. Both change every 20 rounds.
 */
uint32_t SimulatorWebSocket::MixSha1(uint8_t round, uint32_t b, uint32_t c,
                                     uint32_t d) {
  static constexpr std::array<uint32_t, 4> kRoundConstants{
      {0x5A827999U, 0x6ED9EBA1U, 0x8F1BBCDCU, 0xCA62C1D6U}};
  uint8_t const stage{static_cast<uint8_t>(round / kSha1RoundsPerStage)};
  uint32_t mixed{b ^ c ^ d};

  if (stage == 0) {
    mixed = (b & c) | (~b & d);
  } else if (stage == 2) {
    mixed = (b & c) | (b & d) | (c & d);
  }
  return mixed + kRoundConstants[stage];
}

// Returns `value` rotated to the left by `bits`, 1 to 31: the bits shifted out
// on the left come back in on the right.
uint32_t SimulatorWebSocket::RotateLeft(uint32_t value, uint8_t bits) {
  return (value << bits) | (value >> (uint32_t{kWordBits} - bits));
}
