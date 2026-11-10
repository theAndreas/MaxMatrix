// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief What the simulator's hosts do with a socket: listen on loopback, never
 *        wait, send all.
 */

#include "SimulatorSocket.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <string>

/* SimulatorSocket Parameter */
#define SIMULATOR_SOCKET_BACKLOG 4

int SimulatorSocket::OpenListener(uint16_t port) {
  int const listening = socket(AF_INET, SOCK_STREAM, 0);
  int const reuse = 1;
  sockaddr_in address = sockaddr_in();

  if (listening < 0) return SIMULATOR_SOCKET_NONE;

  // so that a restart does not wait out the last connection's TIME_WAIT
  setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (bind(listening, reinterpret_cast<sockaddr*>(&address), sizeof(address)) !=
          0 ||
      listen(listening, SIMULATOR_SOCKET_BACKLOG) != 0) {
    close(listening);
    return SIMULATOR_SOCKET_NONE;
  }
  SetNonBlocking(listening);
  return listening;
}

void SimulatorSocket::SetNonBlocking(int socket) {
  fcntl(socket, F_SETFL, fcntl(socket, F_GETFL, 0) | O_NONBLOCK);
}

void SimulatorSocket::SetReceiveTimeout(int socket, uint32_t milliseconds) {
  timeval timeout = timeval();

  timeout.tv_sec = static_cast<time_t>(milliseconds / 1000);
  timeout.tv_usec = static_cast<suseconds_t>((milliseconds % 1000) * 1000);
  setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

bool SimulatorSocket::SendAll(int socket, std::string const& data) {
  size_t sent = 0;

  while (sent < data.size()) {
    ssize_t const written =
        send(socket, data.data() + sent, data.size() - sent, MSG_NOSIGNAL);
    if (written > 0)
      sent += static_cast<size_t>(written);
    else if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
      continue;
    else
      return false;
  }
  return true;
}
