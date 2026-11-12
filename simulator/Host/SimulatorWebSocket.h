// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The WebSocket protocol of the simulator's web page.
 *
 * The handshake and the frames of RFC 6455, as far as the page needs them. On
 * strings only, with no socket in here, so that the tests can check every byte
 * without a network.
 */

#ifndef MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBSOCKET_H_
#define MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBSOCKET_H_

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>

/* SimulatorWebSocket opcodes, RFC 6455 section 5.2 */
constexpr uint8_t kSimulatorWebSocketOpcodeText{0x1U};
constexpr uint8_t kSimulatorWebSocketOpcodeClose{0x8U};

/* SimulatorWebSocket Parameter */
constexpr size_t kSimulatorWebSocketSha1Length{20U};
/* the hash of SHA-1 */
using SimulatorSha1Digest = std::array<uint8_t, kSimulatorWebSocketSha1Length>;

/*!
 * \brief The WebSocket protocol of the simulator's web page.
 *
 * It has no state of its own, so every function is static, as in
 * max_matrix::Font.
 */
class SimulatorWebSocket {
 private:
  /* the five words of the hash while SHA-1 runs, FIPS 180-4 */
  using Sha1State = std::array<uint32_t, 5>;

  static bool GetKey(std::string const& request, std::string* key);
  static std::string PadSha1Message(std::string const& message);
  static Sha1State ProcessSha1Block(std::string const& padded, size_t offset,
                                    Sha1State state);
  static uint32_t MixSha1(uint8_t round, uint32_t b, uint32_t c, uint32_t d);
  static uint32_t RotateLeft(uint32_t value, uint8_t bits);

 public:
  /*!
   * \brief Builds the answer to a browser's request to open a WebSocket.
   *
   * The answer is the 101 response with the accept key the browser checks.
   *
   * \param[in] request The whole HTTP request, headers included.
   * \param[out] answer The response to send back; left as it was on false.
   * \return True, or false if the request has no `Sec-WebSocket-Key`.
   */
  static bool GetHandshakeAnswer(std::string const& request,
                                 std::string* answer);

  /*!
   * \brief Wraps a text into a frame.
   *
   * The frame is final and unmasked, as a server sends it.
   *
   * \param[in] payload The text to send.
   * \return The frame, header and payload.
   * \note The 64 bit length is never written, as a panel update is a few
   *       hundred bytes: a payload of 64 KiB or more does not fit.
   */
  static std::string EncodeFrame(std::string const& payload);

  /*!
   * \brief Takes the first frame out of what has arrived.
   *
   * The payload is unmasked, as a browser always masks it, and the frame is
   * removed from `buffer`. A frame that has not fully arrived stays in `buffer`
   * for the next call. A frame with a 64 bit length is not read: `buffer` is
   * cleared, and false comes back.
   *
   * \param[in,out] buffer What has arrived on the connection and is not yet
   *                       decoded.
   * \param[out] opcode The opcode of the frame.
   * \param[out] payload The payload of the frame, unmasked.
   * \return True, or false if `buffer` holds no complete frame.
   */
  static bool DecodeFrame(std::string* buffer, uint8_t* opcode,
                          std::string* payload);

  /*!
   * \brief Converts the browser's key into the key it expects back.
   *
   * \param[in] key The value of `Sec-WebSocket-Key`.
   * \return The value of `Sec-WebSocket-Accept`.
   */
  static std::string ConvertKeyToAcceptKey(std::string const& key);

  /*!
   * \brief Calculates the SHA-1 of a message.
   *
   * \param[in] message The bytes to hash.
   * \return The hash.
   */
  static SimulatorSha1Digest CalculateSha1(std::string const& message);

  /*!
   * \brief Converts bytes to base64.
   *
   * Every three bytes become four chars, and the last group is padded with `=`.
   *
   * \param[in] bytes The bytes to convert.
   * \return The base64 text.
   */
  static std::string ConvertToBase64(std::string const& bytes);
};

#endif  // MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBSOCKET_H_
