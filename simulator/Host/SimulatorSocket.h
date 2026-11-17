// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief What the simulator's hosts do with a socket: listen on loopback, never
 *        wait, send all.
 *
 * Shared by the web host and the serial host, which both are polled from the
 * simulator's tick.
 */

#ifndef MAXMATRIX_SIMULATOR_HOST_SIMULATORSOCKET_H_
#define MAXMATRIX_SIMULATOR_HOST_SIMULATORSOCKET_H_

#include <stdint.h>
#include <sys/types.h>

#include <chrono>
#include <string>

#include "MaxMatrixAttributes.h"

/* SimulatorSocket Parameter */
constexpr int kSimulatorSocketNone{(-1)};

/*!
 * \brief What the simulator's hosts do with a socket: listen on loopback, never
 *        wait, send all.
 *
 * It has no state of its own, so every function is static, as in
 * max_matrix::Font.
 */
class SimulatorSocket {
 public:
  /*!
   * \brief Opens a socket that listens on `port`, bound to loopback only.
   *
   * VS Code forwards the port out of the dev container, and nothing else should
   * reach it. The socket does not block, so that accept() returns at once when
   * nobody is waiting.
   *
   * \param[in] port The port to listen on.
   * \return The socket, or `kSimulatorSocketNone` if the port is taken or the
   *         socket would block.
   */
  MAXMATRIX_NODISCARD static int OpenListener(uint16_t port);

  /*!
   * \brief Lets `socket` return at once: sets O_NONBLOCK, so that polling it
   *        from the tick never stops the sketch.
   *
   * \param[in] socket The socket.
   * \return True, or false if it still blocks, which the caller must not poll
   *         then.
   */
  MAXMATRIX_NODISCARD static bool SetNonBlocking(int socket);

  /*!
   * \brief Closes `socket`.
   *
   * \param[in] socket The socket.
   */
  static void Close(int socket);

  /*!
   * \brief Returns whether the other end of a connection is gone, as a recv()
   *        on it says: closed, or broken.
   *
   * \param[in] received What recv() returned.
   * \return True if it returned 0 or failed for another reason than that
   *         nothing has arrived yet.
   */
  MAXMATRIX_NODISCARD static bool IsGone(ssize_t received);

  /*!
   * \brief Lets recv() on the blocking `socket` give up after a while.
   *
   * A recv() that has waited `timeout` for the next bytes returns -1 with
   * EAGAIN.
   *
   * \param[in] socket The socket.
   * \param[in] timeout The longest wait; a duration rather than a number, so
   *                    that it cannot take the socket's place (FN-16).
   * \return True, or false if recv() would wait without end, which the caller
   *         must not call then.
   */
  MAXMATRIX_NODISCARD static bool SetReceiveTimeout(
      int socket, std::chrono::milliseconds timeout);

  /*!
   * \brief Sends all of `data` on the connection `socket`.
   *
   * It sends everything or reports failure: half a frame or half a line is a
   * broken connection rather than a short one.
   *
   * \param[in] socket The connection.
   * \param[in] data The bytes to send.
   * \return True once all was sent, or false if the connection is broken.
   */
  static bool SendAll(int socket, std::string const& data);
};

#endif  // MAXMATRIX_SIMULATOR_HOST_SIMULATORSOCKET_H_
