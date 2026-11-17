// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief What the simulator's hosts do with a socket: listen on loopback, never
 *        wait, send all.
 */

#include "SimulatorSocket.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>
#include <sys/time.h>  // IWYU pragma: keep: timeval, which glibc declares in bits/
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <chrono>
#include <string>

#include "MaxMatrixAttributes.h"
#include "Utility/Bit.h"

namespace {

/* SimulatorSocket Parameter */
constexpr int kBacklog{4};

// Whether the call that failed only would have had to wait.
bool WouldBlock() { return (errno == EAGAIN) || (errno == EWOULDBLOCK); }

}  // namespace

int SimulatorSocket::OpenListener(uint16_t port) {
  int const listening{socket(AF_INET, SOCK_STREAM, 0)};

  if (listening < 0) {
    return kSimulatorSocketNone;
  }

  // So that a restart does not wait out the last connection's TIME_WAIT;
  // without it the port is taken a minute longer, which bind() below then
  // reports.
  int const reuse{1};
  max_matrix::Discard(
      setsockopt(listening, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)));
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  // the sockets API takes every address as a sockaddr
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  bool const bound{bind(listening, reinterpret_cast<sockaddr*>(&address),
                        sizeof(address)) == 0};
  bool const listens{bound && (listen(listening, kBacklog) == 0)};
  bool const ready{listens && SetNonBlocking(listening)};
  if (!ready) {
    Close(listening);
    return kSimulatorSocketNone;
  }
  return listening;
}

bool SimulatorSocket::SetNonBlocking(int socket) {
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's fcntl()
  int const flags{fcntl(socket, F_GETFL, 0)};
  if (flags < 0) {
    return false;
  }
  // H-8: fcntl() takes its flags as an int, or'ed here as the bits they are.
  int const non_blocking_flags{
      static_cast<int>(max_matrix::Bit<uint32_t>::Combine(
          static_cast<uint32_t>(flags), static_cast<uint32_t>(O_NONBLOCK)))};
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg): POSIX's fcntl()
  return fcntl(socket, F_SETFL, non_blocking_flags) == 0;
}

void SimulatorSocket::Close(int socket) {
  // close() frees the descriptor even when it reports an error, and there is
  // nothing to try again.
  max_matrix::Discard(close(socket));
}

bool SimulatorSocket::IsGone(ssize_t received) {
  return (received == 0) || ((received < 0) && !WouldBlock());
}

// setsockopt() takes the timeout in whole seconds and the microseconds beyond.
bool SimulatorSocket::SetReceiveTimeout(int socket,
                                        std::chrono::milliseconds timeout) {
  auto const seconds{std::chrono::duration_cast<std::chrono::seconds>(timeout)};
  auto const beyond{
      std::chrono::duration_cast<std::chrono::microseconds>(timeout - seconds)};
  timeval wait{};

  wait.tv_sec = static_cast<time_t>(seconds.count());
  wait.tv_usec = static_cast<suseconds_t>(beyond.count());
  return setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &wait, sizeof(wait)) == 0;
}

bool SimulatorSocket::SendAll(int socket, std::string const& data) {
  size_t sent{0};

  while (sent < data.size()) {
    ssize_t const written{
        send(socket, &data[sent], data.size() - sent, MSG_NOSIGNAL)};
    if (written > 0) {
      sent += static_cast<size_t>(written);
    } else if ((written < 0) && WouldBlock()) {
      continue;
    } else {
      return false;
    }
  }
  return true;
}
