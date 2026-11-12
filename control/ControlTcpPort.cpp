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
#include <array>
#include <string>

#include "ControlTcpPort.h"

namespace {

/* ControlTcpPort Parameter */
constexpr intptr_t kNone{(-1)};
constexpr size_t kChunkLength{256U};
constexpr int kWriteTimeoutMs{1000};

#ifdef _WIN32
using Socket = SOCKET;
/* send() takes the length as an int on Windows, a size_t on POSIX */
using Length = int;
constexpr int kSendFlags{0};

bool WouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
void CloseSocket(Socket socket) { closesocket(socket); }
int Poll(pollfd* descriptor, int timeout) {
  return WSAPoll(descriptor, 1, timeout);
}
#else
using Socket = int;
using Length = size_t;
/* a write to a socket the simulator has closed fails rather than ending the
 * program with SIGPIPE */
constexpr int kSendFlags{MSG_NOSIGNAL};

bool WouldBlock() { return errno == EAGAIN || errno == EWOULDBLOCK; }
void CloseSocket(Socket socket) { close(socket); }
int Poll(pollfd* descriptor, int timeout) {
  return poll(descriptor, 1, timeout);
}
#endif

// The socket is kept as an intptr_t, which holds both kinds.
Socket ToSocket(intptr_t socket) { return static_cast<Socket>(socket); }
Length ToLength(size_t length) { return static_cast<Length>(length); }

}  // namespace

ControlTcpPort::ControlTcpPort() : socket_{kNone} {}

// The class is final, so Close() is its own here, as nothing can override it.
ControlTcpPort::~ControlTcpPort() { Close(); }

/*
 * The connection is made blocking, which on loopback is answered at once, and
 * only then switched to return at once, so that Read() never waits.
 */
bool ControlTcpPort::Open(uint16_t port) {
  sockaddr_in address{};

  Close();
#ifdef _WIN32
  WSADATA winsock;
  if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) return false;
#endif
  socket_ = static_cast<intptr_t>(socket(AF_INET, SOCK_STREAM, 0));
  if (socket_ == kNone) {
#ifdef _WIN32
    WSACleanup();
#endif
    return false;
  }
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bool opened{
      connect(ToSocket(socket_),
              // the sockets API takes every address as a sockaddr
              // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0};
#ifdef _WIN32
  u_long non_blocking = 1;
  opened =
      opened && ioctlsocket(ToSocket(socket_), FIONBIO, &non_blocking) == 0;
#else
  if (opened) {
    int const flags{fcntl(ToSocket(socket_), F_GETFL, 0)};
    // NOLINTNEXTLINE(bugprone-signed-bitwise): H-8, fcntl() takes flags as int
    int const non_blocking_flags{flags | O_NONBLOCK};
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's fcntl()
    opened = fcntl(ToSocket(socket_), F_SETFL, non_blocking_flags) == 0;
  }
#endif
  if (!opened) {
    Close();
  }
  return opened;
}

bool ControlTcpPort::IsOpen() const { return socket_ != kNone; }

bool ControlTcpPort::Write(std::string const& data) {
  size_t sent{0};

  if (!IsOpen()) {
    return false;
  }
  while (sent < data.size()) {
    auto const written{send(ToSocket(socket_), &data[sent],
                            ToLength(data.size() - sent), kSendFlags)};
    pollfd writable{ToSocket(socket_), POLLOUT, 0};
    if (written > 0) {
      sent += static_cast<size_t>(written);
    } else if (written < 0 && WouldBlock() &&
               Poll(&writable, kWriteTimeoutMs) > 0) {
      continue;
    } else {
      Close();
      return false;
    }
  }
  return true;
}

bool ControlTcpPort::Read(std::string* data) {
  std::array<char, kChunkLength> chunk{};

  if (!IsOpen()) {
    return false;
  }
  for (;;) {
    auto const length{recv(ToSocket(socket_), chunk.data(),
                           static_cast<int>(chunk.size()), 0)};
    if (length > 0) {
      data->append(chunk.data(), static_cast<size_t>(length));
    } else if (length < 0 && WouldBlock()) {
      return true;
    } else {
      Close();
      return false;
    }
  }
}

void ControlTcpPort::Close() {
  if (socket_ == kNone) {
    return;
  }
  CloseSocket(ToSocket(socket_));
  socket_ = kNone;
#ifdef _WIN32
  WSACleanup();
#endif
}
