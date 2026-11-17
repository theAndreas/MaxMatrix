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

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>

#include "MaxMatrixAttributes.h"
#include "SimulatorFile.h"
#include "SimulatorSocket.h"
#include "SimulatorWebSocket.h"

namespace {

/* SimulatorWebHost Parameter */
constexpr size_t kRequestChunkLength{4096u};
constexpr size_t kFrameChunkLength{2048u};
constexpr std::chrono::milliseconds kRequestTimeout{100};
constexpr char kWebSocketPath[] = "/ws";

}  // namespace

SimulatorWebHost::SimulatorWebHost() : listener_{kSimulatorSocketNone} {
  for (int& client : clients_) {
    client = kSimulatorSocketNone;
  }
}

SimulatorWebHost::~SimulatorWebHost() {
  for (int const client : clients_) {
    if (client != kSimulatorSocketNone) {
      SimulatorSocket::Close(client);
    }
  }
  if (listener_ != kSimulatorSocketNone) {
    SimulatorSocket::Close(listener_);
  }
}

bool SimulatorWebHost::Begin(uint16_t port, char const* page_path) {
  page_path_ = page_path;
  listener_ = SimulatorSocket::OpenListener(port);
  return listener_ != kSimulatorSocketNone;
}

std::vector<std::string> SimulatorWebHost::Task() {
  std::vector<std::string> messages;

  if (listener_ == kSimulatorSocketNone) {
    return messages;
  }

  AcceptClients();
  for (uint8_t slot{0}; slot < kSimulatorWebHostMaxClients; slot++) {
    if (clients_[slot] != kSimulatorSocketNone) {
      std::vector<std::string> const read = ReadClient(slot);
      messages.insert(messages.end(), read.begin(), read.end());
    }
  }
  return messages;
}

void SimulatorWebHost::Broadcast(std::string const& text) {
  std::string const frame = SimulatorWebSocket::EncodeFrame(text);

  for (uint8_t slot{0}; slot < kSimulatorWebHostMaxClients; slot++) {
    if ((clients_[slot] != kSimulatorSocketNone) &&
        !SimulatorSocket::SendAll(clients_[slot], frame)) {
      DropClient(slot);
    }
  }
}

// Answers every new connection: keeps one that became a WebSocket, as long as
// there is a free slot for it, and closes every other one.
void SimulatorWebHost::AcceptClients() {
  for (;;) {
    int const accepted{accept(listener_, nullptr, nullptr)};
    if (accepted < 0) {
      break;
    }
    std::string const request = ReadRequest(accepted);
    uint8_t const slot{FindFreeSlot()};
    bool const answered{(slot < kSimulatorWebHostMaxClients) &&
                        AnswerRequest(accepted, request)};
    bool const kept{answered && SimulatorSocket::SetNonBlocking(accepted)};

    if (kept) {
      clients_[slot] = accepted;
    } else {
      SimulatorSocket::Close(accepted);
    }
  }
}

/*
 * Reads what the WebSocket in `slot` has sent and returns every complete text
 * message. The bytes are collected until a frame is complete; a closed
 * connection or a close frame drops the client.
 */
std::vector<std::string> SimulatorWebHost::ReadClient(uint8_t slot) {
  std::array<char, kFrameChunkLength> chunk{};
  ssize_t const received{recv(clients_[slot], chunk.data(), chunk.size(), 0)};
  std::vector<std::string> messages;

  if (SimulatorSocket::IsGone(received)) {
    DropClient(slot);
    return messages;
  }
  if (received > 0) {
    pending_[slot].append(chunk.data(), static_cast<size_t>(received));
  }

  SimulatorWebSocketFrame frame{
      SimulatorWebSocket::DecodeFrame(pending_[slot])};
  while (frame.complete) {
    if (frame.opcode == kSimulatorWebSocketOpcodeClose) {
      DropClient(slot);
      return messages;
    }
    if (frame.opcode == kSimulatorWebSocketOpcodeText) {
      messages.push_back(frame.payload);
    }
    frame = SimulatorWebSocket::DecodeFrame(pending_[slot]);
  }
  return messages;
}

/*
 * Answers `request`, the whole HTTP request with its headers, on the connection
 * `socket`: serves the page on `/` and opens a WebSocket on `/ws`; every other
 * path gets a 404. Returns true if the connection stays, which only a WebSocket
 * does, and false once the connection is done.
 */
bool SimulatorWebHost::AnswerRequest(int socket, std::string const& request) {
  SimulatorFoundText const path{GetPath(request)};

  if (!path.found) {
    return false;
  }

  if (path.text == kWebSocketPath) {
    SimulatorFoundText const answer{
        SimulatorWebSocket::GetHandshakeAnswer(request)};
    return answer.found && SimulatorSocket::SendAll(socket, answer.text);
  }
  if ((path.text == "/") || (path.text == "/index.html")) {
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
  SimulatorFileText const page{SimulatorFile::Read(page_path_)};

  if (page.complete) {
    SendResponse(socket, "200 OK", "text/html; charset=utf-8", page.text);
  } else {
    SendResponse(socket, "500 Internal Server Error", "text/plain",
                 std::string("cannot read ") + page_path_ + "\n");
  }
}

// Finds a slot for a new WebSocket: returns the first slot without a client, or
// kSimulatorWebHostMaxClients when all are taken.
uint8_t SimulatorWebHost::FindFreeSlot() const {
  uint8_t slot{0};

  while ((slot < kSimulatorWebHostMaxClients) &&
         (clients_[slot] != kSimulatorSocketNone)) {
    slot++;
  }
  return slot;
}

// Closes the WebSocket in `slot`, frees the slot and forgets what the client
// sent that was not yet a whole frame.
void SimulatorWebHost::DropClient(uint8_t slot) {
  SimulatorSocket::Close(clients_[slot]);
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

  // Without the timeout a silent client would stop the sketch, so its request
  // is not read; it gets no answer, and its connection is closed.
  if (!SimulatorSocket::SetReceiveTimeout(socket, kRequestTimeout)) {
    return request;
  }
  std::array<char, kRequestChunkLength> chunk{};
  while (request.find("\r\n\r\n") == std::string::npos) {
    ssize_t const received{recv(socket, chunk.data(), chunk.size(), 0)};
    if (received <= 0) {
      break;
    }
    request.append(chunk.data(), static_cast<size_t>(received));
  }
  return request;
}

// Finds the path in `request`, what stands between the first two spaces of the
// request line, or none if the request line has no path.
SimulatorFoundText SimulatorWebHost::GetPath(std::string const& request) {
  size_t const start{request.find(' ')};
  size_t const end{(start == std::string::npos) ? std::string::npos
                                                : request.find(' ', start + 1)};

  if (end == std::string::npos) {
    return SimulatorFoundText{false, std::string{}};
  }
  return SimulatorFoundText{true, request.substr(start + 1, end - start - 1)};
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
  // The connection is closed after the answer either way, and a browser that
  // has gone needs none.
  max_matrix::Discard(SimulatorSocket::SendAll(socket, response.str()));
}
