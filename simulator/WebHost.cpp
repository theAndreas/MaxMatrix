#include "WebHost.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <sstream>

namespace {

const int MaxClients = 4;
int Listener = -1;
int Clients[MaxClients] = { -1, -1, -1, -1 };
std::string Pending[MaxClients];
const char* Page = NULL;

/* RFC 6455 makes the handshake answer the base64 of the SHA-1 of the client's key and a fixed string. Written out
 * rather than pulled in as a dependency, as in Wordclock; a checksum a browser demands, not a security measure. */
void sha1(const uint8_t* Message, size_t Length, uint8_t Digest[20])
{
    uint32_t State[5] = { 0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u };
    std::string Padded(reinterpret_cast<const char*>(Message), Length);

    Padded.push_back(static_cast<char>(0x80));
    while(Padded.size() % 64 != 56) Padded.push_back('\0');
    uint64_t Bits = static_cast<uint64_t>(Length) * 8;
    for(int Shift = 56; Shift >= 0; Shift -= 8) Padded.push_back(static_cast<char>((Bits >> Shift) & 0xFF));

    for(size_t Offset = 0; Offset < Padded.size(); Offset += 64) {
        uint32_t Word[80];
        for(int i = 0; i < 16; i++) {
            const uint8_t* Chunk = reinterpret_cast<const uint8_t*>(&Padded[Offset + i * 4]);
            Word[i] = (uint32_t(Chunk[0]) << 24) | (uint32_t(Chunk[1]) << 16) | (uint32_t(Chunk[2]) << 8) | Chunk[3];
        }
        for(int i = 16; i < 80; i++) {
            uint32_t Mixed = Word[i - 3] ^ Word[i - 8] ^ Word[i - 14] ^ Word[i - 16];
            Word[i] = (Mixed << 1) | (Mixed >> 31);
        }

        uint32_t A = State[0], B = State[1], C = State[2], D = State[3], E = State[4];
        for(int i = 0; i < 80; i++) {
            uint32_t Mixed, Constant;
            if(i < 20)      { Mixed = (B & C) | (~B & D);           Constant = 0x5A827999u; }
            else if(i < 40) { Mixed = B ^ C ^ D;                    Constant = 0x6ED9EBA1u; }
            else if(i < 60) { Mixed = (B & C) | (B & D) | (C & D);  Constant = 0x8F1BBCDCu; }
            else            { Mixed = B ^ C ^ D;                    Constant = 0xCA62C1D6u; }
            uint32_t Next = ((A << 5) | (A >> 27)) + Mixed + E + Constant + Word[i];
            E = D;
            D = C;
            C = (B << 30) | (B >> 2);
            B = A;
            A = Next;
        }
        State[0] += A;
        State[1] += B;
        State[2] += C;
        State[3] += D;
        State[4] += E;
    }
    for(int i = 0; i < 5; i++) {
        for(int Byte = 0; Byte < 4; Byte++) Digest[i * 4 + Byte] = static_cast<uint8_t>(State[i] >> (24 - 8 * Byte));
    }
}

std::string toBase64(const uint8_t* Bytes, size_t Length)
{
    static const char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string Encoded;

    for(size_t Offset = 0; Offset < Length; Offset += 3) {
        size_t Remaining = Length - Offset;
        uint32_t Group = uint32_t(Bytes[Offset]) << 16;
        if(Remaining > 1) Group |= uint32_t(Bytes[Offset + 1]) << 8;
        if(Remaining > 2) Group |= Bytes[Offset + 2];
        Encoded += Alphabet[(Group >> 18) & 0x3F];
        Encoded += Alphabet[(Group >> 12) & 0x3F];
        Encoded += Remaining > 1 ? Alphabet[(Group >> 6) & 0x3F] : '=';
        Encoded += Remaining > 2 ? Alphabet[Group & 0x3F] : '=';
    }
    return Encoded;
}

void setNonBlocking(int Socket)
{
    fcntl(Socket, F_SETFL, fcntl(Socket, F_GETFL, 0) | O_NONBLOCK);
}

/* everything or nothing: half a frame is a broken connection rather than a short one */
bool sendAll(int Socket, const char* Data, size_t Length)
{
    size_t Sent = 0;

    while(Sent < Length) {
        ssize_t Written = send(Socket, Data + Sent, Length - Sent, MSG_NOSIGNAL);
        if(Written > 0) Sent += static_cast<size_t>(Written);
        else if(Written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        else return false;
    }
    return true;
}

void sendResponse(int Socket, const char* Status, const char* ContentType, const std::string& Body)
{
    char Header[256];
    int Written = snprintf(Header, sizeof(Header),
                           "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-store\r\n"
                           "Connection: close\r\n\r\n", Status, ContentType, Body.size());
    if(sendAll(Socket, Header, static_cast<size_t>(Written))) sendAll(Socket, Body.data(), Body.size());
}

/* one text frame out; a panel update is a few hundred bytes, so the 64-bit length is never needed */
bool sendFrame(int Socket, const std::string& Payload)
{
    std::string Header;

    Header += static_cast<char>(0x81);
    if(Payload.size() < 126) {
        Header += static_cast<char>(Payload.size());
    } else {
        Header += static_cast<char>(126);
        Header += static_cast<char>((Payload.size() >> 8) & 0xFF);
        Header += static_cast<char>(Payload.size() & 0xFF);
    }
    return sendAll(Socket, Header.data(), Header.size()) && sendAll(Socket, Payload.data(), Payload.size());
}

/* one frame in, or false while it is still arriving; what a browser sends is always masked */
bool takeFrame(std::string& Buffer, uint8_t& Opcode, std::string& Payload)
{
    if(Buffer.size() < 2) return false;

    bool IsMasked = (static_cast<uint8_t>(Buffer[1]) & 0x80) != 0;
    size_t Length = static_cast<uint8_t>(Buffer[1]) & 0x7F;
    size_t Offset = 2;

    if(Length == 126) {
        if(Buffer.size() < 4) return false;
        Length = (size_t(static_cast<uint8_t>(Buffer[2])) << 8) | static_cast<uint8_t>(Buffer[3]);
        Offset = 4;
    } else if(Length == 127) {
        /* no command is this long; refusing is better than trying to read it */
        Buffer.clear();
        return false;
    }

    size_t MaskLength = IsMasked ? 4 : 0;
    if(Buffer.size() < Offset + MaskLength + Length) return false;

    Payload.assign(Buffer, Offset + MaskLength, Length);
    if(IsMasked) {
        for(size_t i = 0; i < Length; i++) Payload[i] = static_cast<char>(Payload[i] ^ Buffer[Offset + i % 4]);
    }
    Opcode = static_cast<uint8_t>(Buffer[0]) & 0x0F;
    Buffer.erase(0, Offset + MaskLength + Length);
    return true;
}

bool completeHandshake(int Socket, const std::string& Request)
{
    static const char KeyHeader[] = "sec-websocket-key:";
    std::string Lowered(Request);

    for(size_t i = 0; i < Lowered.size(); i++) Lowered[i] = static_cast<char>(tolower(Lowered[i]));
    size_t Found = Lowered.find(KeyHeader);
    if(Found == std::string::npos) return false;

    size_t Start = Found + sizeof(KeyHeader) - 1;
    while(Start < Request.size() && Request[Start] == ' ') Start++;
    size_t End = Request.find("\r\n", Start);
    if(End == std::string::npos) return false;

    std::string Accept = Request.substr(Start, End - Start) + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    uint8_t Digest[20];
    sha1(reinterpret_cast<const uint8_t*>(Accept.data()), Accept.size(), Digest);

    std::string Answer = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                         "Sec-WebSocket-Accept: " + toBase64(Digest, sizeof(Digest)) + "\r\n\r\n";
    return sendAll(Socket, Answer.data(), Answer.size());
}

/* answers a request and says whether the connection stays, which only a WebSocket does */
bool answerRequest(int Socket, const std::string& Request)
{
    size_t PathStart = Request.find(' ');
    size_t PathEnd = PathStart == std::string::npos ? std::string::npos : Request.find(' ', PathStart + 1);
    if(PathEnd == std::string::npos) return false;
    std::string Path = Request.substr(PathStart + 1, PathEnd - PathStart - 1);

    if(Path == "/ws") return completeHandshake(Socket, Request);
    if(Path == "/" || Path == "/index.html") {
        /* read on every request, so that a change to the page needs a reload in the browser, not a rebuild */
        std::ifstream File(Page);
        std::stringstream Body;
        Body << File.rdbuf();
        if(File) sendResponse(Socket, "200 OK", "text/html; charset=utf-8", Body.str());
        else sendResponse(Socket, "500 Internal Server Error", "text/plain", std::string("cannot read ") + Page + "\n");
        return false;
    }
    sendResponse(Socket, "404 Not Found", "text/plain", "no such route on the simulator\n");
    return false;
}

void dropClient(int Slot)
{
    close(Clients[Slot]);
    Clients[Slot] = -1;
    Pending[Slot].clear();
}

}

bool WebHost::begin(unsigned short Port, const char* PagePath)
{
    Page = PagePath;
    Listener = socket(AF_INET, SOCK_STREAM, 0);
    if(Listener < 0) return false;

    /* so that a restart does not wait out the last connection's TIME_WAIT */
    int Reuse = 1;
    setsockopt(Listener, SOL_SOCKET, SO_REUSEADDR, &Reuse, sizeof(Reuse));

    sockaddr_in Address = sockaddr_in();
    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    /* loopback only: VS Code forwards it out of the dev container, and nothing else should reach it */
    Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(Listener, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) != 0 || listen(Listener, 4) != 0) {
        close(Listener);
        Listener = -1;
        return false;
    }
    setNonBlocking(Listener);
    return true;
}

/* One request per connection: every answer closes it, which buys the whole of the keep-alive bookkeeping not
 * existing, for a page load's worth of connections on loopback. */
void WebHost::task(void (*Command)(const std::string&))
{
    if(Listener < 0) return;

    for(;;) {
        int Accepted = accept(Listener, NULL, NULL);
        if(Accepted < 0) break;

        /* a request is one packet on loopback, read here and now */
        std::string Request;
        char Chunk[4096];
        while(Request.find("\r\n\r\n") == std::string::npos) {
            ssize_t Read = recv(Accepted, Chunk, sizeof(Chunk), 0);
            if(Read <= 0) break;
            Request.append(Chunk, static_cast<size_t>(Read));
        }

        int Slot = 0;
        while(Slot < MaxClients && Clients[Slot] >= 0) Slot++;
        if(Slot == MaxClients || !answerRequest(Accepted, Request)) {
            close(Accepted);
            continue;
        }
        setNonBlocking(Accepted);
        Clients[Slot] = Accepted;
    }

    for(int Slot = 0; Slot < MaxClients; Slot++) {
        if(Clients[Slot] < 0) continue;

        char Chunk[2048];
        ssize_t Read = recv(Clients[Slot], Chunk, sizeof(Chunk), 0);
        if(Read == 0 || (Read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            dropClient(Slot);
            continue;
        }
        if(Read > 0) Pending[Slot].append(Chunk, static_cast<size_t>(Read));

        uint8_t Opcode;
        std::string Payload;
        while(takeFrame(Pending[Slot], Opcode, Payload)) {
            if(Opcode == 0x8) {
                dropClient(Slot);
                break;
            }
            if(Opcode == 0x1 && Command != NULL) Command(Payload);
        }
    }
}

void WebHost::broadcast(const std::string& Text)
{
    for(int Slot = 0; Slot < MaxClients; Slot++) {
        if(Clients[Slot] >= 0 && !sendFrame(Clients[Slot], Text)) dropClient(Slot);
    }
}

bool WebHost::hasClients()
{
    for(int Slot = 0; Slot < MaxClients; Slot++) {
        if(Clients[Slot] >= 0) return true;
    }
    return false;
}
