// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The WebSocket protocol of the simulator's web page.
 *
 * The handshake and the frames of RFC 6455, as far as the page needs them. On strings only, with no
 * socket in here, so that the tests can check every byte without a network.
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATORWEBSOCKET_H_
#define MAXMATRIX_SIMULATOR_SIMULATORWEBSOCKET_H_

#include <stddef.h>
#include <stdint.h>
#include <string>

/* SimulatorWebSocket opcodes, RFC 6455 section 5.2 */
#define SIMULATOR_WEB_SOCKET_OPCODE_TEXT                            0x1u
#define SIMULATOR_WEB_SOCKET_OPCODE_CLOSE                           0x8u

/* SimulatorWebSocket Parameter */
#define SIMULATOR_WEB_SOCKET_SHA1_LENGTH                            20u

/* no state of its own, so every function is static, as in MaxMatrixFont */
class SimulatorWebSocket
{
  private:
    static bool GetKey(std::string const&, std::string*);
    static std::string PadSha1Message(std::string const&);
    static void ProcessSha1Block(uint8_t const*, uint32_t*);
    static uint32_t MixSha1(uint8_t, uint32_t, uint32_t, uint32_t);
    static uint32_t RotateLeft(uint32_t, uint8_t);

  public:
    // get methods
    static bool GetHandshakeAnswer(std::string const&, std::string*);

    // methods
    static std::string EncodeFrame(std::string const&);
    static bool DecodeFrame(std::string*, uint8_t*, std::string*);
    static std::string ConvertKeyToAcceptKey(std::string const&);
    static void CalculateSha1(std::string const&, uint8_t*);
    static std::string ConvertToBase64(uint8_t const*, size_t);
};

#endif
