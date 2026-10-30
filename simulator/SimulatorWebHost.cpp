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
/**     \file       SimulatorWebHost.cpp
 *      \brief      A small HTTP and WebSocket server for the simulator, on loopback only
 *
 *      \details    One request per connection: every answer but a WebSocket's closes it, which buys the whole of the
 *                  keep-alive bookkeeping not existing, for a page load's worth of connections on loopback.
 *
 *****************************************************************************************************************************************************/
#define _SIMULATOR_WEB_HOST_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include "SimulatorWebHost.h"
#include "SimulatorWebSocket.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorWebHost Parameter */
#define SIMULATOR_WEB_HOST_NO_SOCKET                                -1
#define SIMULATOR_WEB_HOST_BACKLOG                                  4
#define SIMULATOR_WEB_HOST_REQUEST_CHUNK_LENGTH                     4096u
#define SIMULATOR_WEB_HOST_FRAME_CHUNK_LENGTH                       2048u
#define SIMULATOR_WEB_HOST_WEB_SOCKET_PATH                          "/ws"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorWebHost
******************************************************************************************************************************************************/
/*! \brief          SimulatorWebHost constructor
 *  \details        Instantiation of a host that listens nowhere until begin()
 *
 *  \return         -
 *****************************************************************************************************************************************************/
SimulatorWebHost::SimulatorWebHost()
{
    Listener = SIMULATOR_WEB_HOST_NO_SOCKET;
    PagePath = nullptr;
    for(unsigned Slot = 0; Slot < SIMULATOR_WEB_HOST_MAX_CLIENTS; Slot++) Clients[Slot] = SIMULATOR_WEB_HOST_NO_SOCKET;
} /* SimulatorWebHost */


/******************************************************************************************************************************************************
  begin()
******************************************************************************************************************************************************/
/*! \brief          opens the port on localhost
 *  \details        this function starts listening; the page is read from its path on every request, so that a change to
 *                  it needs a reload in the browser, not a rebuild
 *
 *  \param[in]      Port            the port to listen on
 *  \param[in]      sPagePath       the page to serve on /
 *  \return         E_OK
 *                  E_NOT_OK        the port is taken, so that the caller can say so
 *****************************************************************************************************************************************************/
stdReturnType SimulatorWebHost::begin(unsigned short Port, const char* sPagePath)
{
    PagePath = sPagePath;
    Listener = openListener(Port);
    return Listener == SIMULATOR_WEB_HOST_NO_SOCKET ? E_NOT_OK : E_OK;
} /* begin */


/******************************************************************************************************************************************************
  task()
******************************************************************************************************************************************************/
/*! \brief          answers whatever has arrived
 *  \details        this function accepts and answers new connections and reads the frames of the open WebSockets
 *
 *  \param[out]     Messages        every text message a page has sent since the last call is appended
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::task(std::vector<std::string>* Messages)
{
    if(Listener == SIMULATOR_WEB_HOST_NO_SOCKET) return;

    acceptClients();
    for(unsigned Slot = 0; Slot < SIMULATOR_WEB_HOST_MAX_CLIENTS; Slot++) {
        if(Clients[Slot] != SIMULATOR_WEB_HOST_NO_SOCKET) readClient(Slot, Messages);
    }
} /* task */


/******************************************************************************************************************************************************
  broadcast()
******************************************************************************************************************************************************/
/*! \brief          sends a text to every open WebSocket
 *  \details        this function drops a client it cannot send to
 *
 *  \param[in]      Text            the message
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::broadcast(const std::string& Text)
{
    const std::string Frame = SimulatorWebSocket::encodeFrame(Text);

    for(unsigned Slot = 0; Slot < SIMULATOR_WEB_HOST_MAX_CLIENTS; Slot++) {
        if(Clients[Slot] != SIMULATOR_WEB_HOST_NO_SOCKET && !sendAll(Clients[Slot], Frame)) dropClient(Slot);
    }
} /* broadcast */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  acceptClients()
******************************************************************************************************************************************************/
/*! \brief          answers every new connection
 *  \details        this function keeps a connection that became a WebSocket, as long as there is a free slot for it,
 *                  and closes every other one
 *
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::acceptClients()
{
    int Accepted;

    while((Accepted = accept(Listener, nullptr, nullptr)) >= 0) {
        const std::string Request = readRequest(Accepted);
        const unsigned Slot = findFreeSlot();

        if(Slot < SIMULATOR_WEB_HOST_MAX_CLIENTS && answerRequest(Accepted, Request)) {
            setNonBlocking(Accepted);
            Clients[Slot] = Accepted;
        } else close(Accepted);
    }
} /* acceptClients */


/******************************************************************************************************************************************************
  readClient()
******************************************************************************************************************************************************/
/*! \brief          reads what a WebSocket has sent
 *  \details        this function collects the bytes until a frame is complete; a closed connection or a close frame
 *                  drops the client
 *
 *  \param[in]      Slot            the client's slot
 *  \param[out]     Messages        every complete text message is appended
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::readClient(unsigned Slot, std::vector<std::string>* Messages)
{
    char Chunk[SIMULATOR_WEB_HOST_FRAME_CHUNK_LENGTH];
    const ssize_t Read = recv(Clients[Slot], Chunk, sizeof(Chunk), 0);
    uint8_t Opcode;
    std::string Payload;

    if(Read == 0 || (Read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        dropClient(Slot);
        return;
    }
    if(Read > 0) Pending[Slot].append(Chunk, static_cast<size_t>(Read));

    while(E_OK == SimulatorWebSocket::decodeFrame(&Pending[Slot], &Opcode, &Payload)) {
        if(Opcode == SIMULATOR_WEB_SOCKET_OPCODE_CLOSE) {
            dropClient(Slot);
            return;
        }
        if(Opcode == SIMULATOR_WEB_SOCKET_OPCODE_TEXT) Messages->push_back(Payload);
    }
} /* readClient */


/******************************************************************************************************************************************************
  answerRequest()
******************************************************************************************************************************************************/
/*! \brief          answers an HTTP request
 *  \details        this function serves the page on / and opens a WebSocket on /ws; every other path gets a 404
 *
 *  \param[in]      Socket          the connection
 *  \param[in]      Request         the whole request, headers included
 *  \return         true            the connection stays, which only a WebSocket does
 *                  false           the connection is done
 *****************************************************************************************************************************************************/
bool SimulatorWebHost::answerRequest(int Socket, const std::string& Request)
{
    std::string Path;
    std::string Answer;

    if(E_OK != getPath(Request, &Path)) return false;

    if(Path == SIMULATOR_WEB_HOST_WEB_SOCKET_PATH) {
        return E_OK == SimulatorWebSocket::getHandshakeAnswer(Request, &Answer) && sendAll(Socket, Answer);
    }
    if(Path == "/" || Path == "/index.html") sendPage(Socket);
    else sendResponse(Socket, "404 Not Found", "text/plain", "no such route on the simulator\n");
    return false;
} /* answerRequest */


/******************************************************************************************************************************************************
  sendPage()
******************************************************************************************************************************************************/
/*! \brief          sends the page
 *  \details        this function reads the page from its path, so that the browser always gets the current one
 *
 *  \param[in]      Socket          the connection
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::sendPage(int Socket)
{
    std::ifstream File(PagePath);
    std::stringstream Body;

    Body << File.rdbuf();
    if(File) sendResponse(Socket, "200 OK", "text/html; charset=utf-8", Body.str());
    else sendResponse(Socket, "500 Internal Server Error", "text/plain", std::string("cannot read ") + PagePath + "\n");
} /* sendPage */


/******************************************************************************************************************************************************
  findFreeSlot()
******************************************************************************************************************************************************/
/*! \brief          finds a slot for a new WebSocket
 *  \details        this function returns the first slot without a client
 *
 *  \return         the first free slot, or SIMULATOR_WEB_HOST_MAX_CLIENTS when all are taken
 *****************************************************************************************************************************************************/
unsigned SimulatorWebHost::findFreeSlot() const
{
    unsigned Slot = 0;

    while(Slot < SIMULATOR_WEB_HOST_MAX_CLIENTS && Clients[Slot] != SIMULATOR_WEB_HOST_NO_SOCKET) Slot++;
    return Slot;
} /* findFreeSlot */


/******************************************************************************************************************************************************
  dropClient()
******************************************************************************************************************************************************/
/*! \brief          closes a WebSocket
 *  \details        this function frees the slot and forgets what the client sent that was not yet a whole frame
 *
 *  \param[in]      Slot            the client's slot
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::dropClient(unsigned Slot)
{
    close(Clients[Slot]);
    Clients[Slot] = SIMULATOR_WEB_HOST_NO_SOCKET;
    Pending[Slot].clear();
} /* dropClient */


/******************************************************************************************************************************************************
  openListener()
******************************************************************************************************************************************************/
/*! \brief          opens a listening socket on loopback
 *  \details        this function binds to loopback only: VS Code forwards the port out of the dev container, and
 *                  nothing else should reach it
 *
 *  \param[in]      Port            the port to listen on
 *  \return         the socket, or SIMULATOR_WEB_HOST_NO_SOCKET when the port is taken
 *****************************************************************************************************************************************************/
int SimulatorWebHost::openListener(unsigned short Port)
{
    const int Socket = socket(AF_INET, SOCK_STREAM, 0);
    const int Reuse = 1;
    sockaddr_in Address = sockaddr_in();

    if(Socket < 0) return SIMULATOR_WEB_HOST_NO_SOCKET;

    /* so that a restart does not wait out the last connection's TIME_WAIT */
    setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, &Reuse, sizeof(Reuse));
    Address.sin_family = AF_INET;
    Address.sin_port = htons(Port);
    Address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(Socket, reinterpret_cast<sockaddr*>(&Address), sizeof(Address)) != 0 || listen(Socket, SIMULATOR_WEB_HOST_BACKLOG) != 0) {
        close(Socket);
        return SIMULATOR_WEB_HOST_NO_SOCKET;
    }
    setNonBlocking(Socket);
    return Socket;
} /* openListener */


/******************************************************************************************************************************************************
  readRequest()
******************************************************************************************************************************************************/
/*! \brief          reads an HTTP request
 *  \details        this function reads up to the blank line after the headers. A request is one packet on loopback,
 *                  so it is read here and now rather than collected over several calls.
 *
 *  \param[in]      Socket          the new connection
 *  \return         the request, or what arrived of it before the connection closed
 *****************************************************************************************************************************************************/
std::string SimulatorWebHost::readRequest(int Socket)
{
    std::string Request;
    char Chunk[SIMULATOR_WEB_HOST_REQUEST_CHUNK_LENGTH];

    while(Request.find("\r\n\r\n") == std::string::npos) {
        const ssize_t Read = recv(Socket, Chunk, sizeof(Chunk), 0);
        if(Read <= 0) break;
        Request.append(Chunk, static_cast<size_t>(Read));
    }
    return Request;
} /* readRequest */


/******************************************************************************************************************************************************
  getPath()
******************************************************************************************************************************************************/
/*! \brief          finds the path in an HTTP request
 *  \details        this function returns what stands between the first two spaces of the request line
 *
 *  \param[in]      Request         the whole request
 *  \param[out]     Path            the path
 *  \return         E_OK
 *                  E_NOT_OK        the request line has no path
 *****************************************************************************************************************************************************/
stdReturnType SimulatorWebHost::getPath(const std::string& Request, std::string* Path)
{
    const size_t Start = Request.find(' ');
    const size_t End = Start == std::string::npos ? std::string::npos : Request.find(' ', Start + 1);

    if(End == std::string::npos) return E_NOT_OK;
    *Path = Request.substr(Start + 1, End - Start - 1);
    return E_OK;
} /* getPath */


/******************************************************************************************************************************************************
  setNonBlocking()
******************************************************************************************************************************************************/
/*! \brief          lets a socket return at once
 *  \details        this function sets O_NONBLOCK, so that polling a socket from the tick never stops the sketch
 *
 *  \param[in]      Socket          the socket
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::setNonBlocking(int Socket)
{
    fcntl(Socket, F_SETFL, fcntl(Socket, F_GETFL, 0) | O_NONBLOCK);
} /* setNonBlocking */


/******************************************************************************************************************************************************
  sendAll()
******************************************************************************************************************************************************/
/*! \brief          sends all of a text
 *  \details        this function sends everything or reports failure: half a frame is a broken connection rather than a
 *                  short one
 *
 *  \param[in]      Socket          the connection
 *  \param[in]      Data            the bytes to send
 *  \return         true            all was sent
 *                  false           the connection is broken
 *****************************************************************************************************************************************************/
bool SimulatorWebHost::sendAll(int Socket, const std::string& Data)
{
    size_t Sent = 0;

    while(Sent < Data.size()) {
        const ssize_t Written = send(Socket, Data.data() + Sent, Data.size() - Sent, MSG_NOSIGNAL);
        if(Written > 0) Sent += static_cast<size_t>(Written);
        else if(Written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        else return false;
    }
    return true;
} /* sendAll */


/******************************************************************************************************************************************************
  sendResponse()
******************************************************************************************************************************************************/
/*! \brief          sends an HTTP response that closes the connection
 *  \details        this function says Connection: close, because the host closes every connection but a WebSocket's
 *                  after the answer
 *
 *  \param[in]      Socket          the connection
 *  \param[in]      Status          the status line's code and text, e.g. "200 OK"
 *  \param[in]      ContentType     the type of the body
 *  \param[in]      Body            the body
 *  \return         -
 *****************************************************************************************************************************************************/
void SimulatorWebHost::sendResponse(int Socket, const char* Status, const char* ContentType, const std::string& Body)
{
    std::ostringstream Response;

    Response << "HTTP/1.1 " << Status << "\r\nContent-Type: " << ContentType << "\r\nContent-Length: " << Body.size()
             << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" << Body;
    sendAll(Socket, Response.str());
} /* sendResponse */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
