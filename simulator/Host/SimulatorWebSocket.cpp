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

/* handshake, RFC 6455 section 1.3: the server hashes the client's key with this
 * fixed string */
#define SIMULATOR_WEB_SOCKET_GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define SIMULATOR_WEB_SOCKET_KEY_HEADER "sec-websocket-key:"

/* frame, RFC 6455 section 5.2 */
#define SIMULATOR_WEB_SOCKET_FINAL_TEXT_FRAME 0x81u
#define SIMULATOR_WEB_SOCKET_OPCODE_MASK 0x0Fu
#define SIMULATOR_WEB_SOCKET_MASK_FLAG 0x80u
#define SIMULATOR_WEB_SOCKET_MASK_LENGTH 4u
#define SIMULATOR_WEB_SOCKET_LENGTH_MASK 0x7Fu
#define SIMULATOR_WEB_SOCKET_LENGTH_16_BIT 126u
#define SIMULATOR_WEB_SOCKET_LENGTH_64_BIT 127u
#define SIMULATOR_WEB_SOCKET_HEADER_LENGTH 2u
#define SIMULATOR_WEB_SOCKET_HEADER_LENGTH_16_BIT 4u

/* SHA-1, FIPS 180-4 */
#define SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH 64u
#define SIMULATOR_WEB_SOCKET_SHA1_LENGTH_POSITION 56u
#define SIMULATOR_WEB_SOCKET_SHA1_STATE_LENGTH 5u
#define SIMULATOR_WEB_SOCKET_SHA1_WORDS_PER_BLOCK 16u
#define SIMULATOR_WEB_SOCKET_SHA1_ROUNDS 80u

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
  } else
    return false;
}

std::string SimulatorWebSocket::EncodeFrame(std::string const& payload) {
  std::string frame;

  frame += static_cast<char>(SIMULATOR_WEB_SOCKET_FINAL_TEXT_FRAME);
  if (payload.size() < SIMULATOR_WEB_SOCKET_LENGTH_16_BIT) {
    frame += static_cast<char>(payload.size());
  } else {
    frame += static_cast<char>(SIMULATOR_WEB_SOCKET_LENGTH_16_BIT);
    frame += static_cast<char>((payload.size() >> 8) & 0xFF);
    frame += static_cast<char>(payload.size() & 0xFF);
  }
  return frame + payload;
}

bool SimulatorWebSocket::DecodeFrame(std::string* buffer, uint8_t* opcode,
                                     std::string* payload) {
  std::string const& frame = *buffer;

  if (frame.size() < SIMULATOR_WEB_SOCKET_HEADER_LENGTH) return false;

  size_t const mask_length =
      (static_cast<uint8_t>(frame[1]) & SIMULATOR_WEB_SOCKET_MASK_FLAG)
          ? SIMULATOR_WEB_SOCKET_MASK_LENGTH
          : 0;
  size_t length =
      static_cast<uint8_t>(frame[1]) & SIMULATOR_WEB_SOCKET_LENGTH_MASK;
  size_t offset = SIMULATOR_WEB_SOCKET_HEADER_LENGTH;

  if (length == SIMULATOR_WEB_SOCKET_LENGTH_64_BIT) {
    /* no command is this long; dropping what arrived is better than trying to
     * read it */
    buffer->clear();
    return false;
  }
  if (length == SIMULATOR_WEB_SOCKET_LENGTH_16_BIT) {
    if (frame.size() < SIMULATOR_WEB_SOCKET_HEADER_LENGTH_16_BIT) return false;
    length = (static_cast<size_t>(static_cast<uint8_t>(frame[2])) << 8) |
             static_cast<uint8_t>(frame[3]);
    offset = SIMULATOR_WEB_SOCKET_HEADER_LENGTH_16_BIT;
  }
  if (frame.size() < offset + mask_length + length) return false;

  payload->assign(frame, offset + mask_length, length);
  if (mask_length > 0) {
    for (size_t i = 0; i < length; i++)
      (*payload)[i] =
          static_cast<char>((*payload)[i] ^ frame[offset + i % mask_length]);
  }
  *opcode = static_cast<uint8_t>(frame[0]) & SIMULATOR_WEB_SOCKET_OPCODE_MASK;
  buffer->erase(0, offset + mask_length + length);
  return true;
}

// The accept key is the base64 of the SHA-1 of the key and the fixed GUID of
// RFC 6455.
std::string SimulatorWebSocket::ConvertKeyToAcceptKey(std::string const& key) {
  uint8_t digest[SIMULATOR_WEB_SOCKET_SHA1_LENGTH];

  CalculateSha1(key + SIMULATOR_WEB_SOCKET_GUID, digest);
  return ConvertToBase64(digest, sizeof(digest));
}

// Pads the message and processes it block by block, as FIPS 180-4 describes.
void SimulatorWebSocket::CalculateSha1(std::string const& message,
                                       uint8_t* digest) {
  uint32_t state[SIMULATOR_WEB_SOCKET_SHA1_STATE_LENGTH] = {
      0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
  std::string const padded = PadSha1Message(message);

  for (size_t offset = 0; offset < padded.size();
       offset += SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH) {
    ProcessSha1Block(reinterpret_cast<uint8_t const*>(padded.data() + offset),
                     state);
  }
  for (uint8_t word = 0; word < SIMULATOR_WEB_SOCKET_SHA1_STATE_LENGTH;
       word++) {
    for (uint8_t part = 0; part < 4; part++)
      digest[word * 4 + part] =
          static_cast<uint8_t>(state[word] >> (24 - 8 * part));
  }
}

std::string SimulatorWebSocket::ConvertToBase64(uint8_t const* bytes,
                                                size_t length) {
  static char const kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string encoded;

  for (size_t offset = 0; offset < length; offset += 3) {
    size_t const remaining = length - offset;
    uint32_t group = static_cast<uint32_t>(bytes[offset]) << 16;

    if (remaining > 1) group |= static_cast<uint32_t>(bytes[offset + 1]) << 8;
    if (remaining > 2) group |= bytes[offset + 2];
    encoded += kAlphabet[(group >> 18) & 0x3F];
    encoded += kAlphabet[(group >> 12) & 0x3F];
    encoded += remaining > 1 ? kAlphabet[(group >> 6) & 0x3F] : '=';
    encoded += remaining > 2 ? kAlphabet[group & 0x3F] : '=';
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

  for (size_t i = 0; i < lowered.size(); i++)
    lowered[i] = static_cast<char>(tolower(static_cast<uint8_t>(lowered[i])));
  size_t const found = lowered.find(SIMULATOR_WEB_SOCKET_KEY_HEADER);
  if (found == std::string::npos) return false;

  size_t start = found + sizeof(SIMULATOR_WEB_SOCKET_KEY_HEADER) - 1;
  while (start < request.size() && request[start] == ' ') start++;
  size_t const end = request.find("\r\n", start);
  if (end == std::string::npos) return false;

  *key = request.substr(start, end - start);
  return true;
}

/*
 * Pads `message`, the bytes to hash, to whole blocks: appends a 1 bit, zeros up
 * to eight bytes short of a block, and the length of the message in bits, big
 * endian. Returns the padded message, a multiple of
 * SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH long.
 */
std::string SimulatorWebSocket::PadSha1Message(std::string const& message) {
  uint64_t const bits = static_cast<uint64_t>(message.size()) * 8;
  std::string padded(message);

  padded.push_back(static_cast<char>(0x80));
  while (padded.size() % SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH !=
         SIMULATOR_WEB_SOCKET_SHA1_LENGTH_POSITION)
    padded.push_back('\0');
  for (int8_t shift = 56; shift >= 0; shift -= 8)
    padded.push_back(static_cast<char>((bits >> shift) & 0xFF));
  return padded;
}

/*
 * Mixes `block`, SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH bytes of the padded
 * message, into `state`, the five words of the hash so far: expands the block
 * into the message schedule of 80 words and runs the 80 rounds.
 */
void SimulatorWebSocket::ProcessSha1Block(uint8_t const* block,
                                          uint32_t* state) {
  uint32_t word[SIMULATOR_WEB_SOCKET_SHA1_ROUNDS];
  uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4];

  for (uint8_t i = 0; i < SIMULATOR_WEB_SOCKET_SHA1_WORDS_PER_BLOCK;
       i++, block += 4) {
    word[i] = (static_cast<uint32_t>(block[0]) << 24) |
              (static_cast<uint32_t>(block[1]) << 16) |
              (static_cast<uint32_t>(block[2]) << 8) | block[3];
  }
  for (uint8_t i = SIMULATOR_WEB_SOCKET_SHA1_WORDS_PER_BLOCK;
       i < SIMULATOR_WEB_SOCKET_SHA1_ROUNDS; i++) {
    word[i] =
        RotateLeft(word[i - 3] ^ word[i - 8] ^ word[i - 14] ^ word[i - 16], 1);
  }
  for (uint8_t round = 0; round < SIMULATOR_WEB_SOCKET_SHA1_ROUNDS; round++) {
    uint32_t const next =
        RotateLeft(a, 5) + MixSha1(round, b, c, d) + e + word[round];
    e = d;
    d = c;
    c = RotateLeft(b, 30);
    b = a;
    a = next;
  }
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
  state[4] += e;
}

/*
 * Returns the function and the constant of `round`, 0 to 79, added: the round's
 * function of `b`, `c` and `d`, the second, third and fourth word of the
 * working state, plus its constant. Both change every 20 rounds.
 */
uint32_t SimulatorWebSocket::MixSha1(uint8_t round, uint32_t b, uint32_t c,
                                     uint32_t d) {
  if (round < 20)
    return ((b & c) | (~b & d)) + 0x5A827999u;
  else if (round < 40)
    return (b ^ c ^ d) + 0x6ED9EBA1u;
  else if (round < 60)
    return ((b & c) | (b & d) | (c & d)) + 0x8F1BBCDCu;
  else
    return (b ^ c ^ d) + 0xCA62C1D6u;
}

// Returns `value` rotated to the left by `bits`, 1 to 31: the bits shifted out
// on the left come back in on the right.
uint32_t SimulatorWebSocket::RotateLeft(uint32_t value, uint8_t bits) {
  return (value << bits) | (value >> (32 - bits));
}
