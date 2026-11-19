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

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>

#include "Utility/Bit.h"

namespace {

using ByteBits = max_matrix::Bit<uint8_t>;
using WordBits = max_matrix::Bit<uint32_t>;
using SizeBits = max_matrix::Bit<size_t>;
using LongBits = max_matrix::Bit<uint64_t>;

/* handshake, RFC 6455 section 1.3: the server hashes the client's key with this
 * fixed string */
constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
constexpr char kKeyHeader[] = "sec-websocket-key:";

/* frame, RFC 6455 section 5.2 */
constexpr uint8_t kFinalTextFrame{0x81u};
constexpr uint8_t kOpcodeMask{0x0Fu};
constexpr uint8_t kMaskFlag{0x80u};
constexpr size_t kMaskLength{4u};
constexpr uint8_t kLengthMask{0x7Fu};
constexpr uint8_t kLength16Bit{126u};
constexpr uint8_t kLength64Bit{127u};
constexpr size_t kHeaderLength{2u};
constexpr size_t kHeaderLength16Bit{4u};

/* a byte of a wider number, as a frame and a hash write them */
constexpr uint8_t kBitsPerByte{8u};
constexpr uint32_t kByteMask{0xFFu};

/* SHA-1, FIPS 180-4: blocks of 64 bytes, of 16 words; the 1 bit padding
 * starts with, and the length of the message in 8 bytes at the end */
constexpr size_t kSha1BlockLength{64u};
constexpr size_t kSha1LengthPosition{56u};
constexpr uint8_t kSha1WordsPerBlock{16u};
constexpr uint8_t kSha1BytesPerWord{4u};
constexpr uint8_t kSha1PadBit{0x80u};
constexpr uint8_t kSha1LengthBytes{8u};
/* 80 rounds in four stages of 20; the message schedule takes the words 3, 8,
 * 14 and 16 back; a round turns its words by 5 and 30 bits */
constexpr uint8_t kSha1Rounds{80u};
constexpr uint8_t kSha1RoundsPerStage{20u};
constexpr uint8_t kSha1TapB{8u};
constexpr uint8_t kSha1TapC{14u};
constexpr uint8_t kSha1TurnA{5u};
constexpr uint8_t kSha1TurnB{30u};

/* base64, RFC 4648: a char of six bits, four chars for three bytes */
constexpr uint8_t kBase64Bits{6u};
constexpr uint32_t kBase64Mask{0x3Fu};

// Returns the bytes of `payload` turned back with the four of the client's
// mask, which lie at `mask_at` of `frame`, RFC 6455 section 5.3.
std::string Unmask(std::string const& frame, size_t mask_at,
                   std::string payload) {
  for (size_t i{0}; i < payload.size(); i++) {
    payload[i] = static_cast<char>(ByteBits::ToggleBits(
        static_cast<uint8_t>(payload[i]),
        static_cast<uint8_t>(frame[mask_at + (i % kMaskLength)])));
  }
  return payload;
}

// What DecodeFrame() and GetKey() return when there is nothing to take.
SimulatorWebSocketFrame NoFrame() {
  return SimulatorWebSocketFrame{false, 0, std::string{}};
}

SimulatorFoundText NotFound() {
  return SimulatorFoundText{false, std::string{}};
}

// Returns the length of 16 bits that follows the first two bytes of `frame`,
// the high byte first.
size_t ReadLength16(std::string const& frame) {
  return SizeBits::Combine(
      SizeBits::ShiftUp(static_cast<uint8_t>(frame[2]), kBitsPerByte),
      static_cast<uint8_t>(frame[3]));
}

// Returns the byte at `index` of `bytes` as a number, as a char may be
// negative.
uint32_t GetByte(std::string const& bytes, size_t index) {
  return static_cast<uint8_t>(bytes[index]);
}

// The char in lower case where it is a capital of ASCII, else as it is:
// compared rather than asked of tolower(), which depends on the locale
// (MISRA 24.5.1). A header's name is ASCII.
char ToLower(char c) {
  bool const capital{(c >= 'A') && (c <= 'Z')};
  return capital ? static_cast<char>((c - 'A') + 'a') : c;
}

}  // namespace

SimulatorFoundText SimulatorWebSocket::GetHandshakeAnswer(
    std::string const& request) {
  SimulatorFoundText const key{GetKey(request)};

  if (!key.found) {
    return NotFound();
  }
  return SimulatorFoundText{
      true,
      "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: "
      "Upgrade\r\n"
      "Sec-WebSocket-Accept: " +
          ConvertKeyToAcceptKey(key.text) + "\r\n\r\n"};
}

std::string SimulatorWebSocket::EncodeFrame(std::string const& payload) {
  std::string frame;

  frame += static_cast<char>(kFinalTextFrame);
  if (payload.size() < kLength16Bit) {
    frame += static_cast<char>(payload.size());
  } else {
    frame += static_cast<char>(kLength16Bit);
    frame += static_cast<char>(
        SizeBits::ReadBitGroup(payload.size(), kByteMask, kBitsPerByte));
    frame +=
        static_cast<char>(SizeBits::ReadBitGroup(payload.size(), kByteMask));
  }
  return frame + payload;
}

SimulatorWebSocketFrame SimulatorWebSocket::DecodeFrame(std::string& buffer) {
  std::string const& frame{buffer};

  if (frame.size() < kHeaderLength) {
    return NoFrame();
  }

  uint8_t const second_byte{static_cast<uint8_t>(frame[1])};
  size_t const mask_length{
      (ByteBits::ReadBitGroup(second_byte, kMaskFlag) != 0u) ? kMaskLength : 0};
  size_t const length_field{ByteBits::ReadBitGroup(second_byte, kLengthMask)};

  if (length_field == kLength64Bit) {
    /* no command is this long; dropping what arrived is better than trying to
     * read it */
    buffer.clear();
    return NoFrame();
  }
  /* a length of 126 says that the length follows in two bytes */
  bool const length_follows{length_field == kLength16Bit};
  if (length_follows && (frame.size() < kHeaderLength16Bit)) {
    return NoFrame();
  }
  size_t const payload_length{length_follows ? ReadLength16(frame)
                                             : length_field};
  size_t const mask_at{length_follows ? kHeaderLength16Bit : kHeaderLength};
  size_t const payload_at{mask_at + mask_length};
  size_t const frame_end{payload_at + payload_length};
  if (frame.size() < frame_end) {
    return NoFrame();
  }

  std::string const sent = frame.substr(payload_at, payload_length);
  SimulatorWebSocketFrame const decoded{
      true, ByteBits::ReadBitGroup(static_cast<uint8_t>(frame[0]), kOpcodeMask),
      (mask_length > 0) ? Unmask(frame, mask_at, sent) : sent};
  buffer.erase(0, frame_end);
  return decoded;
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
      {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u}};
  Sha1State state{kInitialState};
  std::string const padded = PadSha1Message(message);
  SimulatorSha1Digest digest{};

  for (size_t offset{0}; offset < padded.size(); offset += kSha1BlockLength) {
    state = ProcessSha1Block(padded, offset, state);
  }
  for (size_t word{0}; word < state.size(); word++) {
    for (uint8_t part{0}; part < kSha1BytesPerWord; part++) {
      auto const bytes_after{
          static_cast<uint8_t>(kSha1BytesPerWord - 1u - part)};
      auto const shift{static_cast<uint8_t>(kBitsPerByte * bytes_after)};
      digest[(word * size_t{kSha1BytesPerWord}) + part] = static_cast<uint8_t>(
          WordBits::ReadBitGroup(state[word], kByteMask, shift));
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
    uint32_t group{
        WordBits::ShiftUp(GetByte(bytes, offset), 2u * kBitsPerByte)};

    if (remaining > 1) {
      group = WordBits::Combine(
          group, WordBits::ShiftUp(GetByte(bytes, offset + 1), kBitsPerByte));
    }
    if (remaining > 2) {
      group = WordBits::Combine(group, GetByte(bytes, offset + 2));
    }
    encoded +=
        kAlphabet[WordBits::ReadBitGroup(group, kBase64Mask, 3u * kBase64Bits)];
    encoded +=
        kAlphabet[WordBits::ReadBitGroup(group, kBase64Mask, 2u * kBase64Bits)];
    encoded +=
        (remaining > 1)
            ? kAlphabet[WordBits::ReadBitGroup(group, kBase64Mask, kBase64Bits)]
            : '=';
    encoded += (remaining > 2)
                   ? kAlphabet[WordBits::ReadBitGroup(group, kBase64Mask)]
                   : '=';
  }
  return encoded;
}

/*
 * Finds the browser's key in `request`, the whole HTTP request with its
 * headers: the value of Sec-WebSocket-Key, or none if the request has no such
 * header. It searches a lowered copy of the request, as
 * header names are case-insensitive, and takes the value from the request
 * itself.
 */
SimulatorFoundText SimulatorWebSocket::GetKey(std::string const& request) {
  std::string lowered(request);

  for (char& c : lowered) {
    c = ToLower(c);
  }
  size_t const found{lowered.find(kKeyHeader)};
  if (found == std::string::npos) {
    return NotFound();
  }

  size_t start{found + sizeof(kKeyHeader) - 1};
  while ((start < request.size()) && (request[start] == ' ')) {
    start++;
  }
  size_t const end{request.find("\r\n", start)};
  if (end == std::string::npos) {
    return NotFound();
  }
  return SimulatorFoundText{true, request.substr(start, end - start)};
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
  while ((padded.size() % kSha1BlockLength) != kSha1LengthPosition) {
    padded.push_back('\0');
  }
  for (uint8_t i{1}; i <= kSha1LengthBytes; i++) {
    auto const shift{
        static_cast<uint8_t>(kBitsPerByte * (kSha1LengthBytes - i))};
    padded.push_back(
        static_cast<char>(LongBits::ReadBitGroup(bits, kByteMask, shift)));
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
      word[i] = WordBits::Combine(WordBits::ShiftUp(word[i], kBitsPerByte),
                                  GetByte(padded, at));
    }
  }
  for (uint8_t i{kSha1WordsPerBlock}; i < kSha1Rounds; i++) {
    uint32_t const near{WordBits::ToggleBits(word[i - 3], word[i - kSha1TapB])};
    uint32_t const far{WordBits::ToggleBits(word[i - kSha1TapC],
                                            word[i - kSha1WordsPerBlock])};
    word[i] = WordBits::RotateLeft(WordBits::ToggleBits(near, far), 1);
  }
  for (uint8_t round{0}; round < kSha1Rounds; round++) {
    uint32_t const next{WordBits::RotateLeft(a, kSha1TurnA) +
                        MixSha1(round, b, c, d) + e + word[round]};
    e = d;
    d = c;
    c = WordBits::RotateLeft(b, kSha1TurnB);
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
// FN-16: b, c and d of RFC 3174's f(t; B, C, D), in its order; the tests
// against its examples fail on a swap
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
uint32_t SimulatorWebSocket::MixSha1(uint8_t round, uint32_t b, uint32_t c,
                                     uint32_t d) {
  static constexpr std::array<uint32_t, 4> kRoundConstants{
      {0x5A827999u, 0x6ED9EBA1u, 0x8F1BBCDCu, 0xCA62C1D6u}};
  uint8_t const stage{static_cast<uint8_t>(round / kSha1RoundsPerStage)};
  uint32_t mixed{WordBits::ToggleBits(WordBits::ToggleBits(b, c), d)};

  if (stage == 0) {
    /* Choose: c where b is set, d where it is not */
    mixed = WordBits::Combine(WordBits::ReadBitGroup(c, b),
                              WordBits::ReadBitGroup(d, WordBits::Invert(b)));
  } else if (stage == 2) {
    /* Majority: what at least two of b, c and d have set */
    uint32_t const b_and_c{WordBits::ReadBitGroup(b, c)};
    uint32_t const either{WordBits::ReadBitGroup(d, WordBits::Combine(b, c))};
    mixed = WordBits::Combine(b_and_c, either);
  } else {
    /* Parity, stages 1 and 3: b, c and d toggled together, as mixed is */
  }
  return mixed + kRoundConstants[stage];
}
