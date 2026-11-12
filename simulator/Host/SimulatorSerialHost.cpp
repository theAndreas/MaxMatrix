// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The sketch's `Serial` as a TCP port on loopback, for the control
 *        program.
 */

#include "SimulatorSerialHost.h"

#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>

#include <string>

#include "SimulatorSocket.h"

/* SimulatorSerialHost Parameter */
#define SIMULATOR_SERIAL_HOST_CHUNK_LENGTH 256u

SimulatorSerialHost::SimulatorSerialHost() {
  listener_ = SIMULATOR_SOCKET_NONE;
  client_ = SIMULATOR_SOCKET_NONE;
}

SimulatorSerialHost::~SimulatorSerialHost() {
  if (client_ != SIMULATOR_SOCKET_NONE) close(client_);
  if (listener_ != SIMULATOR_SOCKET_NONE) close(listener_);
}

bool SimulatorSerialHost::Begin(uint16_t port) {
  listener_ = SimulatorSocket::OpenListener(port);
  return listener_ != SIMULATOR_SOCKET_NONE;
}

void SimulatorSerialHost::Task(std::string* received) {
  char chunk[SIMULATOR_SERIAL_HOST_CHUNK_LENGTH];

  if (listener_ == SIMULATOR_SOCKET_NONE) return;

  // The client first, so that one which has gone frees the port for the next in
  // the same call.
  while (client_ != SIMULATOR_SOCKET_NONE) {
    ssize_t const length = recv(client_, chunk, sizeof(chunk), 0);
    if (length > 0) {
      received->append(chunk, static_cast<size_t>(length));
    } else {
      if (length == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) {
        DropClient();
      }
      break;
    }
  }
  AcceptClient();
}

void SimulatorSerialHost::Send(std::string const& text) {
  if (client_ != SIMULATOR_SOCKET_NONE && !text.empty() &&
      !SimulatorSocket::SendAll(client_, text)) {
    DropClient();
  }
}

/*
 * Accepts every connection that waits: keeps the first while there is no
 * client, and closes every other one at once, so that it learns that the port
 * is taken rather than waiting for an answer that never comes.
 */
void SimulatorSerialHost::AcceptClient() {
  int accepted;

  while ((accepted = accept(listener_, nullptr, nullptr)) >= 0) {
    if (client_ == SIMULATOR_SOCKET_NONE) {
      SimulatorSocket::SetNonBlocking(accepted);
      client_ = accepted;
    } else {
      close(accepted);
    }
  }
}

// Closes the client's connection, so that the next one may connect.
void SimulatorSerialHost::DropClient() {
  close(client_);
  client_ = SIMULATOR_SOCKET_NONE;
}
