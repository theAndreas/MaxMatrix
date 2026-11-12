// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A small HTTP and WebSocket server for the simulator, on loopback only.
 *
 * One request per connection: every answer but a WebSocket's closes it, which
 * buys the whole of the keep-alive bookkeeping not existing, for a page load's
 * worth of connections on loopback.
 */

#include "SimulatorWebHost.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "SimulatorSocket.h"
#include "SimulatorWebSocket.h"

namespace {

/* SimulatorWebHost Parameter */
constexpr size_t kRequestChunkLength{4096u};
constexpr size_t kFrameChunkLength{2048u};
constexpr uint32_t kRequestTimeoutMs{100u};
constexpr char kWebSocketPath[] = "/ws";

}  // namespace

SimulatorWebHost::SimulatorWebHost() {
  listener_ = kSimulatorSocketNone;
  page_path_ = nullptr;
  for (uint8_t slot = 0; slot < kSimulatorWebHostMaxClients; slot++) {
    clients_[slot] = kSimulatorSocketNone;
  }
}

bool SimulatorWebHost::Begin(uint16_t port, char const* page_path) {
  page_path_ = page_path;
  listener_ = SimulatorSocket::OpenListener(port);
  return listener_ != kSimulatorSocketNone;
}

void SimulatorWebHost::Task(std::vector<std::string>* messages) {
  if (listener_ == kSimulatorSocketNone) return;

  AcceptClients();
  for (uint8_t slot = 0; slot < kSimulatorWebHostMaxClients; slot++) {
    if (clients_[slot] != kSimulatorSocketNone) ReadClient(slot, messages);
  }
}

void SimulatorWebHost::Broadcast(std::string const& text) {
  std::string const frame = SimulatorWebSocket::EncodeFrame(text);

  for (uint8_t slot = 0; slot < kSimulatorWebHostMaxClients; slot++) {
    if (clients_[slot] != kSimulatorSocketNone &&
        !SimulatorSocket::SendAll(clients_[slot], frame)) {
      DropClient(slot);
    }
  }
}

// Answers every new connection: keeps one that became a WebSocket, as long as
// there is a free slot for it, and closes every other one.
void SimulatorWebHost::AcceptClients() {
  int accepted;

  while ((accepted = accept(listener_, nullptr, nullptr)) >= 0) {
    std::string const request = ReadRequest(accepted);
    uint8_t const slot = FindFreeSlot();

    if (slot < kSimulatorWebHostMaxClients &&
        AnswerRequest(accepted, request)) {
      SimulatorSocket::SetNonBlocking(accepted);
      clients_[slot] = accepted;
    } else {
      close(accepted);
    }
  }
}

/*
 * Reads what the WebSocket in `slot` has sent and appends every complete text
 * message to `messages`. The bytes are collected until a frame is complete; a
 * closed connection or a close frame drops the client.
 */
void SimulatorWebHost::ReadClient(uint8_t slot,
                                  std::vector<std::string>* messages) {
  char chunk[kFrameChunkLength];
  ssize_t const received = recv(clients_[slot], chunk, sizeof(chunk), 0);
  uint8_t opcode;
  std::string payload;

  if (received == 0 ||
      (received < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
    DropClient(slot);
    return;
  }
  if (received > 0) pending_[slot].append(chunk, static_cast<size_t>(received));

  while (SimulatorWebSocket::DecodeFrame(&pending_[slot], &opcode, &payload)) {
    if (opcode == kSimulatorWebSocketOpcodeClose) {
      DropClient(slot);
      return;
    }
    if (opcode == kSimulatorWebSocketOpcodeText) {
      messages->push_back(payload);
    }
  }
}

/*
 * Answers `request`, the whole HTTP request with its headers, on the connection
 * `socket`: serves the page on `/` and opens a WebSocket on `/ws`; every other
 * path gets a 404. Returns true if the connection stays, which only a WebSocket
 * does, and false once the connection is done.
 */
bool SimulatorWebHost::AnswerRequest(int socket, std::string const& request) {
  std::string path;
  std::string answer;

  if (!GetPath(request, &path)) return false;

  if (path == kWebSocketPath) {
    return SimulatorWebSocket::GetHandshakeAnswer(request, &answer) &&
           SimulatorSocket::SendAll(socket, answer);
  }
  if (path == "/" || path == "/index.html") {
    SendPage(socket);
  } else {
    SendResponse(socket, "404 Not Found", "text/plain",
                 "no such route on the simulator\n");
  }
  return false;
}

// Sends the page on the connection `socket`, read from its path every time, so
// that the browser always gets the current one.
void SimulatorWebHost::SendPage(int socket) {
  std::ifstream const file{page_path_};
  std::stringstream body;

  body << file.rdbuf();
  if (file) {
    SendResponse(socket, "200 OK", "text/html; charset=utf-8", body.str());
  } else {
    SendResponse(socket, "500 Internal Server Error", "text/plain",
                 std::string("cannot read ") + page_path_ + "\n");
  }
}

// Finds a slot for a new WebSocket: returns the first slot without a client, or
// kSimulatorWebHostMaxClients when all are taken.
uint8_t SimulatorWebHost::FindFreeSlot() const {
  uint8_t slot = 0;

  while (slot < kSimulatorWebHostMaxClients &&
         clients_[slot] != kSimulatorSocketNone) {
    slot++;
  }
  return slot;
}

// Closes the WebSocket in `slot`, frees the slot and forgets what the client
// sent that was not yet a whole frame.
void SimulatorWebHost::DropClient(uint8_t slot) {
  close(clients_[slot]);
  clients_[slot] = kSimulatorSocketNone;
  pending_[slot].clear();
}

/*
 * Reads an HTTP request from the new connection `socket`, up to the blank line
 * after the headers. Returns the request, or what arrived of it before the
 * connection closed or went quiet.
 *
 * A request is one packet on loopback, so it is read here and now rather than
 * collected over several calls - but never waited for without end. A connection
 * that is opened and says nothing is what a browser does when it opens a socket
 * ahead of a request it may never make, and every recv() here blocks the
 * sketch, so such a socket used to stop the simulator until the browser closed
 * it. A receive timeout rather than a select() before every recv(): the loop
 * already ends on a recv() that returns nothing.
 */
std::string SimulatorWebHost::ReadRequest(int socket) {
  std::string request;
  char chunk[kRequestChunkLength];

  SimulatorSocket::SetReceiveTimeout(socket, kRequestTimeoutMs);

  while (request.find("\r\n\r\n") == std::string::npos) {
    ssize_t const received = recv(socket, chunk, sizeof(chunk), 0);
    if (received <= 0) break;
    request.append(chunk, static_cast<size_t>(received));
  }
  return request;
}

// Finds the path in `request`, what stands between the first two spaces of the
// request line, and gives it in `path`. Returns false if the request line has
// no path.
bool SimulatorWebHost::GetPath(std::string const& request, std::string* path) {
  size_t const start = request.find(' ');
  size_t const end = start == std::string::npos ? std::string::npos
                                                : request.find(' ', start + 1);

  if (end == std::string::npos) return false;
  *path = request.substr(start + 1, end - start - 1);
  return true;
}

/*
 * Sends an HTTP response that closes the connection `socket`: `status` is the
 * status line's code and text, such as "200 OK", `content_type` the type of
 * `body`. It says `Connection: close`, as the host closes every connection but
 * a WebSocket's after the answer.
 */
void SimulatorWebHost::SendResponse(int socket, char const* status,
                                    char const* content_type,
                                    std::string const& body) {
  std::ostringstream response;

  response << "HTTP/1.1 " << status << "\r\nContent-Type: " << content_type
           << "\r\nContent-Length: " << body.size()
           << "\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"
           << body;
  SimulatorSocket::SendAll(socket, response.str());
}
