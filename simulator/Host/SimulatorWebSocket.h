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

#include "MaxMatrixAttributes.h"

/* SimulatorWebSocket opcodes, RFC 6455 section 5.2 */
constexpr uint8_t kSimulatorWebSocketOpcodeText{0x1u};
constexpr uint8_t kSimulatorWebSocketOpcodeClose{0x8u};

/* SimulatorWebSocket Parameter */
constexpr size_t kSimulatorWebSocketSha1Length{20u};
/* the hash of SHA-1 */
using SimulatorSha1Digest = std::array<uint8_t, kSimulatorWebSocketSha1Length>;

/*! \brief A text looked for in a request: a header's value, a path or an
 *         answer. */
struct SimulatorFoundText {
  bool found;       /*!< Whether there is one. */
  std::string text; /*!< The text; empty if there is none. */
};

/*! \brief The first frame out of what has arrived, as
 *         SimulatorWebSocket::DecodeFrame() takes it. */
struct SimulatorWebSocketFrame {
  bool complete;       /*!< Whether a whole frame had arrived. */
  uint8_t opcode;      /*!< Its opcode. */
  std::string payload; /*!< Its payload, unmasked. */
};

/*!
 * \brief The WebSocket protocol of the simulator's web page.
 *
 * It has no state of its own, so every function is static, as in
 * max_matrix::Font.
 */
class SimulatorWebSocket {
 public:
  /*!
   * \brief Builds the answer to a browser's request to open a WebSocket.
   *
   * The answer is the 101 response with the accept key the browser checks.
   *
   * \param[in] request The whole HTTP request, headers included.
   * \return The response to send back, or none if the request has no
   *         `Sec-WebSocket-Key`.
   */
  MAXMATRIX_NODISCARD static SimulatorFoundText GetHandshakeAnswer(
      std::string const& request);

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
  MAXMATRIX_NODISCARD static std::string EncodeFrame(
      std::string const& payload);

  /*!
   * \brief Takes the first frame out of what has arrived.
   *
   * The payload is unmasked, as a browser always masks it, and the frame is
   * removed from `buffer`. A frame that has not fully arrived stays in `buffer`
   * for the next call. A frame with a 64 bit length is not read: `buffer` is
   * cleared, and no frame comes back.
   *
   * \param[in,out] buffer What has arrived on the connection and is not yet
   *                       decoded.
   * \return The frame, or none if `buffer` holds no complete frame.
   */
  MAXMATRIX_NODISCARD static SimulatorWebSocketFrame DecodeFrame(
      std::string& buffer);

  /*!
   * \brief Converts the browser's key into the key it expects back.
   *
   * \param[in] key The value of `Sec-WebSocket-Key`.
   * \return The value of `Sec-WebSocket-Accept`.
   */
  MAXMATRIX_NODISCARD static std::string ConvertKeyToAcceptKey(
      std::string const& key);

  /*!
   * \brief Calculates the SHA-1 of a message.
   *
   * \param[in] message The bytes to hash.
   * \return The hash.
   */
  MAXMATRIX_NODISCARD static SimulatorSha1Digest CalculateSha1(
      std::string const& message);

  /*!
   * \brief Converts bytes to base64.
   *
   * Every three bytes become four chars, and the last group is padded with `=`.
   *
   * \param[in] bytes The bytes to convert.
   * \return The base64 text.
   */
  MAXMATRIX_NODISCARD static std::string ConvertToBase64(
      std::string const& bytes);

 private:
  /* the five words of the hash while SHA-1 runs, FIPS 180-4 */
  static constexpr uint8_t kSha1StateWords{5};
  using Sha1State = std::array<uint32_t, kSha1StateWords>;

  MAXMATRIX_NODISCARD static SimulatorFoundText GetKey(
      std::string const& request);
  MAXMATRIX_NODISCARD static std::string PadSha1Message(
      std::string const& message);
  MAXMATRIX_NODISCARD static Sha1State ProcessSha1Block(
      std::string const& padded, size_t offset, Sha1State state);
  MAXMATRIX_NODISCARD static uint32_t MixSha1(uint8_t round, uint32_t b,
                                              uint32_t c, uint32_t d);
};

#endif  // MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBSOCKET_H_
