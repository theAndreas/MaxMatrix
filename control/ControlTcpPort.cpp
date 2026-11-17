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
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#endif
#include <stddef.h>
#include <stdint.h>

#include <string>

#include "ControlStream.h"
#include "ControlTcpPort.h"
#include "MaxMatrixAttributes.h"
#include "Utility/Bit.h"

namespace {

/* ControlTcpPort Parameter */
constexpr intptr_t kNone{(-1)};
constexpr int kWriteTimeoutMs{1000};

#ifdef _WIN32
using Socket = SOCKET;
/* send() takes the length as an int on Windows, a size_t on POSIX */
using Length = int;
constexpr int kSendFlags{0};

bool WouldBlock() { return WSAGetLastError() == WSAEWOULDBLOCK; }
// The socket is gone even when closing reports an error, and there is nothing
// to try again.
void CloseSocket(Socket socket) { max_matrix::Discard(closesocket(socket)); }
int Poll(pollfd* descriptor, int timeout) {
  return WSAPoll(descriptor, 1, timeout);
}
bool SetNonBlocking(Socket socket) {
  u_long non_blocking = 1;
  return ioctlsocket(socket, FIONBIO, &non_blocking) == 0;
}
#else
using Socket = int;
using Length = size_t;
/* a write to a socket the simulator has closed fails rather than ending the
 * program with SIGPIPE */
constexpr int kSendFlags{MSG_NOSIGNAL};

bool WouldBlock() { return (errno == EAGAIN) || (errno == EWOULDBLOCK); }
// As on Windows.
void CloseSocket(Socket socket) { max_matrix::Discard(close(socket)); }
int Poll(pollfd* descriptor, int timeout) {
  return poll(descriptor, 1, timeout);
}
bool SetNonBlocking(Socket socket) {
  int const flags{fcntl(socket, F_GETFL, 0)};
  // H-8: fcntl() takes its flags as an int, or'ed here as the bits they are.
  int const non_blocking_flags{
      static_cast<int>(max_matrix::Bit<uint32_t>::Combine(
          static_cast<uint32_t>(flags), static_cast<uint32_t>(O_NONBLOCK)))};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's fcntl()
  return fcntl(socket, F_SETFL, non_blocking_flags) == 0;
}
#endif

// The socket is kept as an intptr_t, which holds both kinds.
constexpr Socket ToSocket(intptr_t socket) {
  return static_cast<Socket>(socket);
}
constexpr Length ToLength(size_t length) { return static_cast<Length>(length); }

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
    // It fails only without the WSAStartup() above.
    max_matrix::Discard(WSACleanup());
#endif
    return false;
  }
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  bool const connected{
      connect(ToSocket(socket_),
              // the sockets API takes every address as a sockaddr
              // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
              reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0};
  bool const opened{connected && SetNonBlocking(ToSocket(socket_))};
  if (!opened) {
    Close();
  }
  return opened;
}

bool ControlTcpPort::IsOpen() const { return socket_ != kNone; }

bool ControlTcpPort::Write(std::string const& data) {
  if (!IsOpen()) {
    return false;
  }
  Socket const connection{ToSocket(socket_)};
  bool const sent{ControlStream::SendAll(
      data,
      [connection](char const* bytes, size_t length) {
        return send(connection, bytes, ToLength(length), kSendFlags);
      },
      WouldBlock,
      [connection] {
        pollfd writable{connection, POLLOUT, 0};
        return Poll(&writable, kWriteTimeoutMs) > 0;
      })};
  if (!sent) {
    Close();
  }
  return sent;
}

bool ControlTcpPort::Read(std::string& data) {
  if (!IsOpen()) {
    return false;
  }
  Socket const connection{ToSocket(socket_)};
  bool const open{ControlStream::ReceiveAvailable(
      data,
      [connection](char* bytes, size_t length) {
        return recv(connection, bytes, ToLength(length), 0);
      },
      WouldBlock)};
  if (!open) {
    Close();
  }
  return open;
}

void ControlTcpPort::Close() {
  if (socket_ == kNone) {
    return;
  }
  CloseSocket(ToSocket(socket_));
  socket_ = kNone;
#ifdef _WIN32
  // It fails only without the WSAStartup() of Open().
  max_matrix::Discard(WSACleanup());
#endif
}
