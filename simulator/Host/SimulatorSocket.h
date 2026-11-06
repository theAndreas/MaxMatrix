// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief What the simulator's hosts do with a socket: listen on loopback, never wait, send all.
 *
 * Shared by the web host and the serial host, which both are polled from the simulator's tick.
 */

#ifndef MAXMATRIX_SIMULATOR_HOST_SIMULATORSOCKET_H_
#define MAXMATRIX_SIMULATOR_HOST_SIMULATORSOCKET_H_

#include <stdint.h>
#include <string>

/* SimulatorSocket Parameter */
#define SIMULATOR_SOCKET_NONE                                       (-1)

/*!
 * \brief What the simulator's hosts do with a socket: listen on loopback, never wait, send all.
 *
 * It has no state of its own, so every function is static, as in MaxMatrixFont.
 */
class SimulatorSocket
{
  public:
    /*!
     * \brief Opens a socket that listens on `port`, bound to loopback only.
     *
     * VS Code forwards the port out of the dev container, and nothing else should reach it. The
     * socket does not block, so that accept() returns at once when nobody is waiting.
     *
     * \param[in] port The port to listen on.
     * \return The socket, or `SIMULATOR_SOCKET_NONE` if the port is taken.
     */
    static int OpenListener(uint16_t port);

    /*!
     * \brief Lets `socket` return at once: sets O_NONBLOCK, so that polling it from the tick never
     *        stops the sketch.
     *
     * \param[in] socket The socket.
     */
    static void SetNonBlocking(int socket);

    /*!
     * \brief Lets recv() on the blocking `socket` give up after a while.
     *
     * A recv() that has waited `milliseconds` for the next bytes returns -1 with EAGAIN.
     *
     * \param[in] socket The socket.
     * \param[in] milliseconds The longest wait.
     */
    static void SetReceiveTimeout(int socket, uint32_t milliseconds);

    /*!
     * \brief Sends all of `data` on the connection `socket`.
     *
     * It sends everything or reports failure: half a frame or half a line is a broken connection
     * rather than a short one.
     *
     * \param[in] socket The connection.
     * \param[in] data The bytes to send.
     * \return True once all was sent, or false if the connection is broken.
     */
    static bool SendAll(int socket, std::string const& data);
};

#endif
