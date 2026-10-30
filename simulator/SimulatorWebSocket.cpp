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
/**     \file       SimulatorWebSocket.cpp
 *      \brief      The WebSocket protocol of the simulator's web page
 *
 *      \details    Ported from Wordclock's platform/simulator/src/WebHost.cpp. SHA-1 and base64 are written out rather than
 *                  pulled in as a dependency, as there: a checksum a browser demands, not a security measure.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_WEB_SOCKET_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <ctype.h>
#include "SimulatorWebSocket.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* handshake, RFC 6455 section 1.3: the server hashes the client's key with this fixed string */
#define SIMULATOR_WEB_SOCKET_GUID                                   "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define SIMULATOR_WEB_SOCKET_KEY_HEADER                             "sec-websocket-key:"

/* frame, RFC 6455 section 5.2 */
#define SIMULATOR_WEB_SOCKET_FINAL_TEXT_FRAME                       0x81u
#define SIMULATOR_WEB_SOCKET_OPCODE_MASK                            0x0Fu
#define SIMULATOR_WEB_SOCKET_MASK_FLAG                              0x80u
#define SIMULATOR_WEB_SOCKET_MASK_LENGTH                            4u
#define SIMULATOR_WEB_SOCKET_LENGTH_MASK                            0x7Fu
#define SIMULATOR_WEB_SOCKET_LENGTH_16_BIT                          126u
#define SIMULATOR_WEB_SOCKET_LENGTH_64_BIT                          127u
#define SIMULATOR_WEB_SOCKET_HEADER_LENGTH                          2u
#define SIMULATOR_WEB_SOCKET_HEADER_LENGTH_16_BIT                   4u

/* SHA-1, FIPS 180-4 */
#define SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH                      64u
#define SIMULATOR_WEB_SOCKET_SHA1_LENGTH_POSITION                   56u
#define SIMULATOR_WEB_SOCKET_SHA1_STATE_LENGTH                      5u
#define SIMULATOR_WEB_SOCKET_SHA1_WORDS_PER_BLOCK                   16u
#define SIMULATOR_WEB_SOCKET_SHA1_ROUNDS                            80u


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  getHandshakeAnswer()
******************************************************************************************************************************************************/
/*! \brief          answers a browser's request to open a WebSocket
 *  \details        this function builds the 101 response with the accept key the browser checks
 *
 *  \param[in]      Request         the whole HTTP request, headers included
 *  \param[out]     Answer          the response to send back
 *  \return         E_OK
 *                  E_NOT_OK        the request has no Sec-WebSocket-Key
 *****************************************************************************************************************************************************/
stdReturnType SimulatorWebSocket::getHandshakeAnswer(const std::string& Request, std::string* Answer)
{
    std::string Key;

    if(E_OK == getKey(Request, &Key)) {
        *Answer = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                  "Sec-WebSocket-Accept: " + convertKeyToAcceptKey(Key) + "\r\n\r\n";
        return E_OK;
    } else return E_NOT_OK;
} /* getHandshakeAnswer */


/******************************************************************************************************************************************************
  encodeFrame()
******************************************************************************************************************************************************/
/*! \brief          wraps a text into a frame
 *  \details        this function returns a final, unmasked text frame, as a server sends it. A panel update is a few
 *                  hundred bytes, so the 64 bit length is never needed.
 *
 *  \param[in]      Payload         the text to send
 *  \return         the frame, header and payload
 *****************************************************************************************************************************************************/
std::string SimulatorWebSocket::encodeFrame(const std::string& Payload)
{
    std::string Frame;

    Frame += static_cast<char>(SIMULATOR_WEB_SOCKET_FINAL_TEXT_FRAME);
    if(Payload.size() < SIMULATOR_WEB_SOCKET_LENGTH_16_BIT) {
        Frame += static_cast<char>(Payload.size());
    } else {
        Frame += static_cast<char>(SIMULATOR_WEB_SOCKET_LENGTH_16_BIT);
        Frame += static_cast<char>((Payload.size() >> 8) & 0xFF);
        Frame += static_cast<char>(Payload.size() & 0xFF);
    }
    return Frame + Payload;
} /* encodeFrame */


/******************************************************************************************************************************************************
  decodeFrame()
******************************************************************************************************************************************************/
/*! \brief          takes the first frame out of what has arrived
 *  \details        this function unmasks the payload, as a browser always masks it, and removes the frame from the
 *                  buffer. A frame that has not fully arrived stays in the buffer for the next call.
 *
 *  \param[in,out]  Buffer          what has arrived on the connection and is not yet decoded
 *  \param[out]     Opcode          the opcode of the frame
 *  \param[out]     Payload         the payload of the frame, unmasked
 *  \return         E_OK
 *                  E_NOT_OK        no complete frame in the buffer
 *****************************************************************************************************************************************************/
stdReturnType SimulatorWebSocket::decodeFrame(std::string* Buffer, uint8_t* Opcode, std::string* Payload)
{
    const std::string& Frame = *Buffer;

    if(Frame.size() < SIMULATOR_WEB_SOCKET_HEADER_LENGTH) return E_NOT_OK;

    const size_t MaskLength = (static_cast<uint8_t>(Frame[1]) & SIMULATOR_WEB_SOCKET_MASK_FLAG) ? SIMULATOR_WEB_SOCKET_MASK_LENGTH : 0;
    size_t Length = static_cast<uint8_t>(Frame[1]) & SIMULATOR_WEB_SOCKET_LENGTH_MASK;
    size_t Offset = SIMULATOR_WEB_SOCKET_HEADER_LENGTH;

    if(Length == SIMULATOR_WEB_SOCKET_LENGTH_64_BIT) {
        /* no command is this long; dropping what arrived is better than trying to read it */
        Buffer->clear();
        return E_NOT_OK;
    }
    if(Length == SIMULATOR_WEB_SOCKET_LENGTH_16_BIT) {
        if(Frame.size() < SIMULATOR_WEB_SOCKET_HEADER_LENGTH_16_BIT) return E_NOT_OK;
        Length = (static_cast<size_t>(static_cast<uint8_t>(Frame[2])) << 8) | static_cast<uint8_t>(Frame[3]);
        Offset = SIMULATOR_WEB_SOCKET_HEADER_LENGTH_16_BIT;
    }
    if(Frame.size() < Offset + MaskLength + Length) return E_NOT_OK;

    Payload->assign(Frame, Offset + MaskLength, Length);
    if(MaskLength > 0) {
        for(size_t i = 0; i < Length; i++) (*Payload)[i] = static_cast<char>((*Payload)[i] ^ Frame[Offset + i % MaskLength]);
    }
    *Opcode = static_cast<uint8_t>(Frame[0]) & SIMULATOR_WEB_SOCKET_OPCODE_MASK;
    Buffer->erase(0, Offset + MaskLength + Length);
    return E_OK;
} /* decodeFrame */


/******************************************************************************************************************************************************
  convertKeyToAcceptKey()
******************************************************************************************************************************************************/
/*! \brief          converts the browser's key into the key it expects back
 *  \details        this function returns the base64 of the SHA-1 of the key and the fixed GUID of RFC 6455
 *
 *  \param[in]      Key             the value of Sec-WebSocket-Key
 *  \return         the value of Sec-WebSocket-Accept
 *****************************************************************************************************************************************************/
std::string SimulatorWebSocket::convertKeyToAcceptKey(const std::string& Key)
{
    uint8_t Digest[SIMULATOR_WEB_SOCKET_SHA1_LENGTH];

    calculateSha1(Key + SIMULATOR_WEB_SOCKET_GUID, Digest);
    return convertToBase64(Digest, sizeof(Digest));
} /* convertKeyToAcceptKey */


/******************************************************************************************************************************************************
  calculateSha1()
******************************************************************************************************************************************************/
/*! \brief          calculates the SHA-1 of a message
 *  \details        this function pads the message and processes it block by block, as FIPS 180-4 describes
 *
 *  \param[in]      Message         the bytes to hash
 *  \param[out]     Digest          the hash, SIMULATOR_WEB_SOCKET_SHA1_LENGTH bytes
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebSocket::calculateSha1(const std::string& Message, uint8_t* Digest)
{
    uint32_t State[SIMULATOR_WEB_SOCKET_SHA1_STATE_LENGTH] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    const std::string Padded = padSha1Message(Message);

    for(size_t Offset = 0; Offset < Padded.size(); Offset += SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH) {
        processSha1Block(reinterpret_cast<const uint8_t*>(Padded.data() + Offset), State);
    }
    for(unsigned Word = 0; Word < SIMULATOR_WEB_SOCKET_SHA1_STATE_LENGTH; Word++) {
        for(unsigned Byte = 0; Byte < 4; Byte++) Digest[Word * 4 + Byte] = static_cast<uint8_t>(State[Word] >> (24 - 8 * Byte));
    }
} /* calculateSha1 */


/******************************************************************************************************************************************************
  convertToBase64()
******************************************************************************************************************************************************/
/*! \brief          converts bytes to base64
 *  \details        this function encodes every three bytes as four chars and pads the last group with '='
 *
 *  \param[in]      Bytes           the bytes to convert
 *  \param[in]      Length          the number of bytes
 *  \return         the base64 text
 *****************************************************************************************************************************************************/
std::string SimulatorWebSocket::convertToBase64(const uint8_t* Bytes, size_t Length)
{
    static const char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string Encoded;

    for(size_t Offset = 0; Offset < Length; Offset += 3) {
        const size_t Remaining = Length - Offset;
        uint32_t Group = static_cast<uint32_t>(Bytes[Offset]) << 16;

        if(Remaining > 1) Group |= static_cast<uint32_t>(Bytes[Offset + 1]) << 8;
        if(Remaining > 2) Group |= Bytes[Offset + 2];
        Encoded += Alphabet[(Group >> 18) & 0x3F];
        Encoded += Alphabet[(Group >> 12) & 0x3F];
        Encoded += Remaining > 1 ? Alphabet[(Group >> 6) & 0x3F] : '=';
        Encoded += Remaining > 2 ? Alphabet[Group & 0x3F] : '=';
    }
    return Encoded;
} /* convertToBase64 */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  getKey()
******************************************************************************************************************************************************/
/*! \brief          finds the browser's key in a request
 *  \details        this function searches a lowered copy of the request, because header names are case-insensitive,
 *                  and returns the value from the request itself
 *
 *  \param[in]      Request         the whole HTTP request, headers included
 *  \param[out]     Key             the value of Sec-WebSocket-Key
 *  \return         E_OK
 *                  E_NOT_OK        the request has no such header
 *****************************************************************************************************************************************************/
stdReturnType SimulatorWebSocket::getKey(const std::string& Request, std::string* Key)
{
    std::string Lowered(Request);

    for(size_t i = 0; i < Lowered.size(); i++) Lowered[i] = static_cast<char>(tolower(static_cast<unsigned char>(Lowered[i])));
    const size_t Found = Lowered.find(SIMULATOR_WEB_SOCKET_KEY_HEADER);
    if(Found == std::string::npos) return E_NOT_OK;

    size_t Start = Found + sizeof(SIMULATOR_WEB_SOCKET_KEY_HEADER) - 1;
    while(Start < Request.size() && Request[Start] == ' ') Start++;
    const size_t End = Request.find("\r\n", Start);
    if(End == std::string::npos) return E_NOT_OK;

    *Key = Request.substr(Start, End - Start);
    return E_OK;
} /* getKey */


/******************************************************************************************************************************************************
  padSha1Message()
******************************************************************************************************************************************************/
/*! \brief          pads a message to whole blocks
 *  \details        this function appends a 1 bit, zeros up to eight bytes short of a block, and the length of the
 *                  message in bits, big endian
 *
 *  \param[in]      Message         the bytes to hash
 *  \return         the padded message, a multiple of SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH long
 *****************************************************************************************************************************************************/
std::string SimulatorWebSocket::padSha1Message(const std::string& Message)
{
    const uint64_t Bits = static_cast<uint64_t>(Message.size()) * 8;
    std::string Padded(Message);

    Padded.push_back(static_cast<char>(0x80));
    while(Padded.size() % SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH != SIMULATOR_WEB_SOCKET_SHA1_LENGTH_POSITION) Padded.push_back('\0');
    for(int Shift = 56; Shift >= 0; Shift -= 8) Padded.push_back(static_cast<char>((Bits >> Shift) & 0xFF));
    return Padded;
} /* padSha1Message */


/******************************************************************************************************************************************************
  processSha1Block()
******************************************************************************************************************************************************/
/*! \brief          mixes one block into the state
 *  \details        this function expands the block into the message schedule of 80 words and runs the 80 rounds
 *
 *  \param[in]      Block           SIMULATOR_WEB_SOCKET_SHA1_BLOCK_LENGTH bytes of the padded message
 *  \param[in,out]  State           the five words of the hash so far
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebSocket::processSha1Block(const uint8_t* Block, uint32_t* State)
{
    uint32_t Word[SIMULATOR_WEB_SOCKET_SHA1_ROUNDS];
    uint32_t A = State[0], B = State[1], C = State[2], D = State[3], E = State[4];

    for(unsigned i = 0; i < SIMULATOR_WEB_SOCKET_SHA1_WORDS_PER_BLOCK; i++) {
        Word[i] = (static_cast<uint32_t>(Block[4 * i]) << 24) | (static_cast<uint32_t>(Block[4 * i + 1]) << 16) |
                  (static_cast<uint32_t>(Block[4 * i + 2]) << 8) | Block[4 * i + 3];
    }
    for(unsigned i = SIMULATOR_WEB_SOCKET_SHA1_WORDS_PER_BLOCK; i < SIMULATOR_WEB_SOCKET_SHA1_ROUNDS; i++) {
        Word[i] = rotateLeft(Word[i - 3] ^ Word[i - 8] ^ Word[i - 14] ^ Word[i - 16], 1);
    }
    for(unsigned Round = 0; Round < SIMULATOR_WEB_SOCKET_SHA1_ROUNDS; Round++) {
        const uint32_t Next = rotateLeft(A, 5) + mixSha1(Round, B, C, D) + E + Word[Round];
        E = D;
        D = C;
        C = rotateLeft(B, 30);
        B = A;
        A = Next;
    }
    State[0] += A;
    State[1] += B;
    State[2] += C;
    State[3] += D;
    State[4] += E;
} /* processSha1Block */


/******************************************************************************************************************************************************
  mixSha1()
******************************************************************************************************************************************************/
/*! \brief          the function and the constant of a round
 *  \details        this function returns the round's function of B, C and D plus its constant, which change every
 *                  20 rounds
 *
 *  \param[in]      Round           the round, 0 to 79
 *  \param[in]      B               the second word of the working state
 *  \param[in]      C               the third word of the working state
 *  \param[in]      D               the fourth word of the working state
 *  \return         function and constant, added
 *****************************************************************************************************************************************************/
uint32_t SimulatorWebSocket::mixSha1(unsigned Round, uint32_t B, uint32_t C, uint32_t D)
{
    if(Round < 20) return ((B & C) | (~B & D)) + 0x5A827999u;
    else if(Round < 40) return (B ^ C ^ D) + 0x6ED9EBA1u;
    else if(Round < 60) return ((B & C) | (B & D) | (C & D)) + 0x8F1BBCDCu;
    else return (B ^ C ^ D) + 0xCA62C1D6u;
} /* mixSha1 */


/******************************************************************************************************************************************************
  rotateLeft()
******************************************************************************************************************************************************/
/*! \brief          rotates a word to the left
 *  \details        this function shifts the bits out on the left back in on the right
 *
 *  \param[in]      Value           the word to rotate
 *  \param[in]      Bits            how far, 1 to 31
 *  \return         the rotated word
 *****************************************************************************************************************************************************/
uint32_t SimulatorWebSocket::rotateLeft(uint32_t Value, unsigned Bits)
{
    return (Value << Bits) | (Value >> (32 - Bits));
} /* rotateLeft */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
