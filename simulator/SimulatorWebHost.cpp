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
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include "SimulatorWebHost.h"
#include "SimulatorWebSocket.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* SimulatorWebHost Parameter */
#define SIMULATOR_WEB_HOST_NO_SOCKET                                (-1)
#define SIMULATOR_WEB_HOST_BACKLOG                                  4
#define SIMULATOR_WEB_HOST_REQUEST_CHUNK_LENGTH                     4096u
#define SIMULATOR_WEB_HOST_FRAME_CHUNK_LENGTH                       2048u
#define SIMULATOR_WEB_HOST_REQUEST_TIMEOUT_MS                       100u
#define SIMULATOR_WEB_HOST_WEB_SOCKET_PATH                          "/ws"


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF SimulatorWebHost
******************************************************************************************************************************************************/
/*! \brief          SimulatorWebHost constructor
 *  \details        Instantiation of a host that listens nowhere until Begin()
 *****************************************************************************************************************************************************/
SimulatorWebHost::SimulatorWebHost()
{
    listener_ = SIMULATOR_WEB_HOST_NO_SOCKET;
    page_path_ = nullptr;
    for(uint8_t slot = 0; slot < SIMULATOR_WEB_HOST_MAX_CLIENTS; slot++) clients_[slot] = SIMULATOR_WEB_HOST_NO_SOCKET;
} /* SimulatorWebHost */


/******************************************************************************************************************************************************
  Begin()
******************************************************************************************************************************************************/
/*! \brief          opens the port on localhost
 *  \details        this function starts listening; the page is read from its path on every request, so that a change to
 *                  it needs a reload in the browser, not a rebuild
 *
 *  \param[in]      port            the port to listen on
 *  \param[in]      page_path       the page to serve on /
 *  \return         true
 *                  false           the port is taken, so that the caller can say so
 *****************************************************************************************************************************************************/
bool SimulatorWebHost::Begin(uint16_t port, char const* page_path)
{
    page_path_ = page_path;
    listener_ = OpenListener(port);
    return listener_ != SIMULATOR_WEB_HOST_NO_SOCKET;
} /* Begin */


/******************************************************************************************************************************************************
  Task()
******************************************************************************************************************************************************/
/*! \brief          answers whatever has arrived
 *  \details        this function accepts and answers new connections and reads the frames of the open WebSockets
 *
 *  \param[out]     messages        every text message a page has sent since the last call is appended
 *****************************************************************************************************************************************************/
void SimulatorWebHost::Task(std::vector<std::string>* messages)
{
    if(listener_ == SIMULATOR_WEB_HOST_NO_SOCKET) return;

    AcceptClients();
    for(uint8_t slot = 0; slot < SIMULATOR_WEB_HOST_MAX_CLIENTS; slot++) {
        if(clients_[slot] != SIMULATOR_WEB_HOST_NO_SOCKET) ReadClient(slot, messages);
    }
} /* Task */


/******************************************************************************************************************************************************
  Broadcast()
******************************************************************************************************************************************************/
/*! \brief          sends a text to every open WebSocket
 *  \details        this function drops a client it cannot send to
 *
 *  \param[in]      text            the message
 *****************************************************************************************************************************************************/
void SimulatorWebHost::Broadcast(std::string const& text)
{
    std::string const frame = SimulatorWebSocket::EncodeFrame(text);

    for(uint8_t slot = 0; slot < SIMULATOR_WEB_HOST_MAX_CLIENTS; slot++) {
        if(clients_[slot] != SIMULATOR_WEB_HOST_NO_SOCKET && !SendAll(clients_[slot], frame)) DropClient(slot);
    }
} /* Broadcast */


/******************************************************************************************************************************************************
 * P R I V A T E   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  AcceptClients()
******************************************************************************************************************************************************/
/*! \brief          answers every new connection
 *  \details        this function keeps a connection that became a WebSocket, as long as there is a free slot for it,
 *                  and closes every other one
 *****************************************************************************************************************************************************/
void SimulatorWebHost::AcceptClients()
{
    int accepted;

    while((accepted = accept(listener_, nullptr, nullptr)) >= 0) {
        std::string const request = ReadRequest(accepted);
        uint8_t const slot = FindFreeSlot();

        if(slot < SIMULATOR_WEB_HOST_MAX_CLIENTS && AnswerRequest(accepted, request)) {
            SetNonBlocking(accepted);
            clients_[slot] = accepted;
        } else close(accepted);
    }
} /* AcceptClients */


/******************************************************************************************************************************************************
  ReadClient()
******************************************************************************************************************************************************/
/*! \brief          reads what a WebSocket has sent
 *  \details        this function collects the bytes until a frame is complete; a closed connection or a close frame
 *                  drops the client
 *
 *  \param[in]      slot            the client's slot
 *  \param[out]     messages        every complete text message is appended
 *****************************************************************************************************************************************************/
void SimulatorWebHost::ReadClient(uint8_t slot, std::vector<std::string>* messages)
{
    char chunk[SIMULATOR_WEB_HOST_FRAME_CHUNK_LENGTH];
    ssize_t const received = recv(clients_[slot], chunk, sizeof(chunk), 0);
    uint8_t opcode;
    std::string payload;

    if(received == 0 || (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
        DropClient(slot);
        return;
    }
    if(received > 0) pending_[slot].append(chunk, static_cast<size_t>(received));

    while(SimulatorWebSocket::DecodeFrame(&pending_[slot], &opcode, &payload)) {
        if(opcode == SIMULATOR_WEB_SOCKET_OPCODE_CLOSE) {
            DropClient(slot);
            return;
        }
        if(opcode == SIMULATOR_WEB_SOCKET_OPCODE_TEXT) messages->push_back(payload);
    }
} /* ReadClient */


/******************************************************************************************************************************************************
  AnswerRequest()
******************************************************************************************************************************************************/
/*! \brief          answers an HTTP request
 *  \details        this function serves the page on / and opens a WebSocket on /ws; every other path gets a 404
 *
 *  \param[in]      socket          the connection
 *  \param[in]      request         the whole request, headers included
 *  \return         true            the connection stays, which only a WebSocket does
 *                  false           the connection is done
 *****************************************************************************************************************************************************/
bool SimulatorWebHost::AnswerRequest(int socket, std::string const& request)
{
    std::string path;
    std::string answer;

    if(!GetPath(request, &path)) return false;

    if(path == SIMULATOR_WEB_HOST_WEB_SOCKET_PATH) {
        return SimulatorWebSocket::GetHandshakeAnswer(request, &answer) && SendAll(socket, answer);
    }
    if(path == "/" || path == "/index.html") SendPage(socket);
    else SendResponse(socket, "404 Not Found", "text/plain", "no such route on the simulator\n");
    return false;
} /* AnswerRequest */


/******************************************************************************************************************************************************
  SendPage()
******************************************************************************************************************************************************/
/*! \brief          sends the page
 *  \details        this function reads the page from its path, so that the browser always gets the current one
 *
 *  \param[in]      socket          the connection
 *****************************************************************************************************************************************************/
void SimulatorWebHost::SendPage(int socket)
{
    std::ifstream file(page_path_);
    std::stringstream body;

    body << file.rdbuf();
    if(file) SendResponse(socket, "200 OK", "text/html; charset=utf-8", body.str());
    else SendResponse(socket, "500 Internal Server Error", "text/plain", std::string("cannot read ") + page_path_ + "\n");
} /* SendPage */


/******************************************************************************************************************************************************
  FindFreeSlot()
******************************************************************************************************************************************************/
/*! \brief          finds a slot for a new WebSocket
 *  \details        this function returns the first slot without a client
 *
 *  \return         the first free slot, or SIMULATOR_WEB_HOST_MAX_CLIENTS when all are taken
 *****************************************************************************************************************************************************/
uint8_t SimulatorWebHost::FindFreeSlot() const
{
    uint8_t slot = 0;

    while(slot < SIMULATOR_WEB_HOST_MAX_CLIENTS && clients_[slot] != SIMULATOR_WEB_HOST_NO_SOCKET) slot++;
    return slot;
} /* FindFreeSlot */


/******************************************************************************************************************************************************
  DropClient()
******************************************************************************************************************************************************/
/*! \brief          closes a WebSocket
 *  \details        this function frees the slot and forgets what the client sent that was not yet a whole frame
 *
 *  \param[in]      slot            the client's slot
 *****************************************************************************************************************************************************/
void SimulatorWebHost::DropClient(uint8_t slot)
{
    close(clients_[slot]);
    clients_[slot] = SIMULATOR_WEB_HOST_NO_SOCKET;
    pending_[slot].clear();
} /* DropClient */


/******************************************************************************************************************************************************
  OpenListener()
******************************************************************************************************************************************************/
/*! \brief          opens a listening socket on loopback
 *  \details        this function binds to loopback only: VS Code forwards the port out of the dev container, and
 *                  nothing else should reach it
 *
 *  \param[in]      port            the port to listen on
 *  \return         the socket, or SIMULATOR_WEB_HOST_NO_SOCKET when the port is taken
 *****************************************************************************************************************************************************/
int SimulatorWebHost::OpenListener(uint16_t port)
{
    int const listening = socket(AF_INET, SOCK_STREAM, 0);
    int const reuse = 1;
    sockaddr_in address = sockaddr_in();

    if(listening < 0) return SIMULATOR_WEB_HOST_NO_SOCKET;

    /* so that a restart does not wait out the last connection's TIME_WAIT */
    setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if(bind(listening, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0 || listen(listening, SIMULATOR_WEB_HOST_BACKLOG) != 0) {
        close(listening);
        return SIMULATOR_WEB_HOST_NO_SOCKET;
    }
    SetNonBlocking(listening);
    return listening;
} /* OpenListener */


/******************************************************************************************************************************************************
  ReadRequest()
******************************************************************************************************************************************************/
/*! \brief          reads an HTTP request
 *  \details        this function reads up to the blank line after the headers. A request is one packet on loopback,
 *                  so it is read here and now rather than collected over several calls - but never waited for without
 *                  end. A connection that is opened and says nothing is what a browser does when it opens a socket
 *                  ahead of a request it may never make, and every recv() here blocks the sketch, so such a socket used
 *                  to stop the simulator until the browser closed it. A receive timeout rather than a select() before
 *                  every recv(): the loop already ends on a recv() that returns nothing.
 *
 *  \param[in]      socket          the new connection
 *  \return         the request, or what arrived of it before the connection closed or went quiet
 *****************************************************************************************************************************************************/
std::string SimulatorWebHost::ReadRequest(int socket)
{
    std::string request;
    char chunk[SIMULATOR_WEB_HOST_REQUEST_CHUNK_LENGTH];

    SetReceiveTimeout(socket, SIMULATOR_WEB_HOST_REQUEST_TIMEOUT_MS);

    while(request.find("\r\n\r\n") == std::string::npos) {
        ssize_t const received = recv(socket, chunk, sizeof(chunk), 0);
        if(received <= 0) break;
        request.append(chunk, static_cast<size_t>(received));
    }
    return request;
} /* ReadRequest */


/******************************************************************************************************************************************************
  GetPath()
******************************************************************************************************************************************************/
/*! \brief          finds the path in an HTTP request
 *  \details        this function returns what stands between the first two spaces of the request line
 *
 *  \param[in]      request         the whole request
 *  \param[out]     path            the path
 *  \return         true
 *                  false           the request line has no path
 *****************************************************************************************************************************************************/
bool SimulatorWebHost::GetPath(std::string const& request, std::string* path)
{
    size_t const start = request.find(' ');
    size_t const end = start == std::string::npos ? std::string::npos : request.find(' ', start + 1);

    if(end == std::string::npos) return false;
    *path = request.substr(start + 1, end - start - 1);
    return true;
} /* GetPath */


/******************************************************************************************************************************************************
  SetNonBlocking()
******************************************************************************************************************************************************/
/*! \brief          lets a socket return at once
 *  \details        this function sets O_NONBLOCK, so that polling a socket from the tick never stops the sketch
 *
 *  \param[in]      socket          the socket
 *****************************************************************************************************************************************************/
void SimulatorWebHost::SetNonBlocking(int socket)
{
    fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
} /* SetNonBlocking */


/******************************************************************************************************************************************************
  SetReceiveTimeout()
******************************************************************************************************************************************************/
/*! \brief          lets recv() on a blocking socket give up after a while
 *  \details        this function sets SO_RCVTIMEO; a recv() that has waited that long returns -1 with EAGAIN
 *
 *  \param[in]      socket          the socket
 *  \param[in]      milliseconds    how long one recv() may wait for the next bytes
 *****************************************************************************************************************************************************/
void SimulatorWebHost::SetReceiveTimeout(int socket, uint32_t milliseconds)
{
    timeval timeout = timeval();

    timeout.tv_sec = static_cast<time_t>(milliseconds / 1000);
    timeout.tv_usec = static_cast<suseconds_t>((milliseconds % 1000) * 1000);
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
} /* SetReceiveTimeout */


/******************************************************************************************************************************************************
  SendAll()
******************************************************************************************************************************************************/
/*! \brief          sends all of a text
 *  \details        this function sends everything or reports failure: half a frame is a broken connection rather than a
 *                  short one
 *
 *  \param[in]      socket          the connection
 *  \param[in]      data            the bytes to send
 *  \return         true            all was sent
 *                  false           the connection is broken
 *****************************************************************************************************************************************************/
bool SimulatorWebHost::SendAll(int socket, std::string const& data)
{
    size_t sent = 0;

    while(sent < data.size()) {
        ssize_t const written = send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
        if(written > 0) sent += static_cast<size_t>(written);
        else if(written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        else return false;
    }
    return true;
} /* SendAll */


/******************************************************************************************************************************************************
  SendResponse()
******************************************************************************************************************************************************/
/*! \brief          sends an HTTP response that closes the connection
 *  \details        this function says Connection: close, because the host closes every connection but a WebSocket's
 *                  after the answer
 *
 *  \param[in]      socket          the connection
 *  \param[in]      status          the status line's code and text, e.g. "200 OK"
 *  \param[in]      content_type    the type of the body
 *  \param[in]      body            the body
 *****************************************************************************************************************************************************/
void SimulatorWebHost::SendResponse(int socket, char const* status, char const* content_type, std::string const& body)
{
    std::ostringstream response;

    response << "HTTP/1.1 " << status << "\r\nContent-Type: " << content_type << "\r\nContent-Length: " << body.size()
             << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n" << body;
    SendAll(socket, response.str());
} /* SendResponse */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
