/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       SimulatorWebSocket.h
 *      \brief      The WebSocket protocol of the simulator's web page
 *
 *      \details    The handshake and the frames of RFC 6455, as far as the page needs them. On strings only, with no socket
 *                  in here, so that the tests can check every byte without a network.
 *
 *****************************************************************************************************************************************************/
#ifndef _SIMULATOR_WEB_SOCKET_H_
#define _SIMULATOR_WEB_SOCKET_H_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stddef.h>
#include <stdint.h>
#include <string>


/******************************************************************************************************************************************************
 *  GLOBAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorWebSocket opcodes, RFC 6455 section 5.2 */
#define SIMULATOR_WEB_SOCKET_OPCODE_TEXT                            0x1u
#define SIMULATOR_WEB_SOCKET_OPCODE_CLOSE                           0x8u

/* SimulatorWebSocket Parameter */
#define SIMULATOR_WEB_SOCKET_SHA1_LENGTH                            20u


/******************************************************************************************************************************************************
 *  CLASS  SimulatorWebSocket
 *****************************************************************************************************************************************************/
/* no state of its own, so every function is static, as in MaxMatrixFont */
class SimulatorWebSocket
{
  private:
    static bool getKey(std::string const&, std::string*);
    static std::string padSha1Message(std::string const&);
    static void processSha1Block(uint8_t const*, uint32_t*);
    static uint32_t mixSha1(uint8_t, uint32_t, uint32_t, uint32_t);
    static uint32_t rotateLeft(uint32_t, uint8_t);

  public:
    // get methods
    static bool getHandshakeAnswer(std::string const&, std::string*);

    // methods
    static std::string encodeFrame(std::string const&);
    static bool decodeFrame(std::string*, uint8_t*, std::string*);
    static std::string convertKeyToAcceptKey(std::string const&);
    static void calculateSha1(std::string const&, uint8_t*);
    static std::string convertToBase64(uint8_t const*, size_t);
};

#endif

/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
