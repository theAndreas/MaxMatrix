// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's serial port on TCP, which `--tcp PORT` opens on
 *        localhost.
 *
 * POSIX sockets and Winsock differ in a few names only, which the macros below
 * hide, so that both share one implementation.
 */

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <string>

#include "ControlTcpPort.h"

/* ControlTcpPort Parameter */
#define CONTROL_TCP_PORT_NONE (-1)
#define CONTROL_TCP_PORT_CHUNK_LENGTH 256u
#define CONTROL_TCP_PORT_WRITE_TIMEOUT_MS 1000

#ifdef _WIN32
#define CONTROL_TCP_PORT_SOCKET(socket) static_cast<SOCKET>(socket)
#define CONTROL_TCP_PORT_WOULD_BLOCK() (WSAGetLastError() == WSAEWOULDBLOCK)
#define CONTROL_TCP_PORT_SEND_FLAGS 0
/* send() takes the length as an int on Windows, a size_t on POSIX */
#define CONTROL_TCP_PORT_LENGTH(length) static_cast<int>(length)
#define CONTROL_TCP_PORT_CLOSE(socket) closesocket(socket)
#define CONTROL_TCP_PORT_POLL(descriptors, timeout) \
  WSAPoll(descriptors, 1, timeout)
#else
#define CONTROL_TCP_PORT_SOCKET(socket) static_cast<int>(socket)
#define CONTROL_TCP_PORT_WOULD_BLOCK() (errno == EAGAIN || errno == EWOULDBLOCK)
/* a write to a socket the simulator has closed fails rather than ending the
 * program with SIGPIPE */
#define CONTROL_TCP_PORT_SEND_FLAGS MSG_NOSIGNAL
#define CONTROL_TCP_PORT_LENGTH(length) (length)
#define CONTROL_TCP_PORT_CLOSE(socket) close(socket)
#define CONTROL_TCP_PORT_POLL(descriptors, timeout) \
  poll(descriptors, 1, timeout)
#endif

ControlTcpPort::ControlTcpPort() { socket_ = CONTROL_TCP_PORT_NONE; }

// The class is final, so Close() is its own here, as nothing can override it.
ControlTcpPort::~ControlTcpPort() { Close(); }

/*
 * The connection is made blocking, which on loopback is answered at once, and
 * only then switched to return at once, so that Read() never waits.
 */
bool ControlTcpPort::Open(uint16_t port) {
  sockaddr_in address = sockaddr_in();

  Close();
#ifdef _WIN32
  WSADATA winsock;
  if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return false;
#endif
  socket_ = static_cast<intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  if (socket_ == CONTROL_TCP_PORT_NONE) {
#ifdef _WIN32
    WSACleanup();
#endif
    return false;
  }
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bool opened =
      connect(CONTROL_TCP_PORT_SOCKET(socket_),
              reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0;
#ifdef _WIN32
  u_long non_blocking = 1;
  opened = opened && ioctlsocket(CONTROL_TCP_PORT_SOCKET(socket_), FIONBIO,
                                 &non_blocking) == 0;
#else
  if (opened) {
    int const flags = fcntl(CONTROL_TCP_PORT_SOCKET(socket_), F_GETFL, 0);
    // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, fcntl() takes flags as int
    int const non_blocking_flags = flags | O_NONBLOCK;
    opened = fcntl(CONTROL_TCP_PORT_SOCKET(socket_), F_SETFL,
                   non_blocking_flags) == 0;
  }
#endif
  if (!opened) Close();
  return opened;
}

bool ControlTcpPort::IsOpen() const { return socket_ != CONTROL_TCP_PORT_NONE; }

bool ControlTcpPort::Write(std::string const& data) {
  size_t sent = 0;

  if (!IsOpen()) return false;
  while (sent < data.size()) {
    auto const written =
        send(CONTROL_TCP_PORT_SOCKET(socket_), data.data() + sent,
             CONTROL_TCP_PORT_LENGTH(data.size() - sent),
             CONTROL_TCP_PORT_SEND_FLAGS);
    pollfd writable = {CONTROL_TCP_PORT_SOCKET(socket_), POLLOUT, 0};
    if (written > 0) {
      sent += static_cast<size_t>(written);
    } else if (written < 0 && CONTROL_TCP_PORT_WOULD_BLOCK() &&
               CONTROL_TCP_PORT_POLL(&writable,
                                     CONTROL_TCP_PORT_WRITE_TIMEOUT_MS) > 0) {
      continue;
    } else {
      Close();
      return false;
    }
  }
  return true;
}

bool ControlTcpPort::Read(std::string* data) {
  char chunk[CONTROL_TCP_PORT_CHUNK_LENGTH];

  if (!IsOpen()) return false;
  for (;;) {
    auto const length = recv(CONTROL_TCP_PORT_SOCKET(socket_), chunk,
                             static_cast<int>(sizeof(chunk)), 0);
    if (length > 0) {
      data->append(chunk, static_cast<size_t>(length));
    } else if (length < 0 && CONTROL_TCP_PORT_WOULD_BLOCK()) {
      return true;
    } else {
      Close();
      return false;
    }
  }
}

void ControlTcpPort::Close() {
  if (socket_ == CONTROL_TCP_PORT_NONE) return;
  CONTROL_TCP_PORT_CLOSE(CONTROL_TCP_PORT_SOCKET(socket_));
  socket_ = CONTROL_TCP_PORT_NONE;
#ifdef _WIN32
  WSACleanup();
#endif
}
