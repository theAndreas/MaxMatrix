// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The sketch's `Serial` as a TCP port on loopback, for the control
 *        program.
 *
 * What the board's serial port is to the control program, a plain TCP port is
 * in the simulator: the bytes a client sends reach the sketch's `Serial`, and
 * what the sketch prints goes back. A program needs no WebSocket for it, which
 * wxWidgets does not have.
 */

#ifndef MAXMATRIX_SIMULATOR_HOST_SIMULATORSERIALHOST_H_
#define MAXMATRIX_SIMULATOR_HOST_SIMULATORSERIALHOST_H_

#include <stdint.h>

#include <string>

/*!
 * \brief The sketch's `Serial` as a TCP port on loopback, for the control
 *        program.
 *
 * One client at a time, as a serial port has one program at its other end: a
 * second connection is closed as soon as it is accepted. It is polled from the
 * simulator's tick, as the web host is.
 */
class SimulatorSerialHost {
 private:
  int listener_;
  int client_;

  void AcceptClient();
  void DropClient();

 public:
  /*! \brief Makes a host that listens nowhere until Begin(). */
  SimulatorSerialHost();

  /*! \brief Closes the client's connection and the port. */
  ~SimulatorSerialHost();

  // A copy would close the sockets of the original a second time.
  SimulatorSerialHost(SimulatorSerialHost const&) = delete;
  SimulatorSerialHost& operator=(SimulatorSerialHost const&) = delete;

  /*!
   * \brief Opens the port on localhost and starts listening.
   *
   * \param[in] port The port to listen on.
   * \return True once the host listens, or false if the port is taken, so that
   *         the caller can say so.
   */
  bool Begin(uint16_t port);

  /*!
   * \brief Accepts a client and reads what it has sent.
   *
   * Until Begin() has opened the port, it does nothing. A client that has
   * closed its end is dropped, and the next one may connect.
   *
   * \param[in,out] received Every byte the client has sent since the last call
   *                         is appended, as a board's `Serial` would receive
   *                         it.
   */
  void Task(std::string* received);

  /*!
   * \brief Sends the client what the sketch has printed.
   *
   * Without a client, the text is lost, as it would be on a board nobody
   * listens to. A client it cannot send to is dropped.
   *
   * \param[in] text The bytes, as the sketch printed them.
   */
  void Send(std::string const& text);
};

#endif  // MAXMATRIX_SIMULATOR_HOST_SIMULATORSERIALHOST_H_
