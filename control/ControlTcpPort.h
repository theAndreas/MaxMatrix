// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's serial port on TCP, which `--tcp PORT` opens on
 *        localhost.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLTCPPORT_H_
#define MAXMATRIX_CONTROL_CONTROLTCPPORT_H_

#include <stdint.h>

#include <string>

#include "ControlPort.h"

/*!
 * \brief The simulator's serial port on TCP, which `--tcp PORT` opens on
 *        localhost.
 *
 * Over POSIX sockets on Linux and Winsock on Windows. The simulator takes one
 * client at a time and closes a second one at once, which Read() then reports
 * as an end that is gone.
 */
class ControlTcpPort final : public ControlPort {
 private:
  // A POSIX descriptor or a Winsock SOCKET, which is an unsigned integer as
  // wide as a pointer; -1 for none, which is what INVALID_SOCKET is as well.
  intptr_t socket_;

 public:
  /*! \brief Makes a port that is closed until Open(). */
  ControlTcpPort();

  /*! \brief Closes the port. */
  ~ControlTcpPort() override;

  // A copy would close the socket of the original a second time.
  ControlTcpPort(ControlTcpPort const&) = delete;
  ControlTcpPort& operator=(ControlTcpPort const&) = delete;

  /*!
   * \brief Connects to the simulator on localhost.
   *
   * A port that is open already is closed first.
   *
   * \param[in] port The port the simulator was given with `--tcp`.
   * \return True once connected, or false if no simulator listens on the port.
   */
  bool Open(uint16_t port);

  bool IsOpen() const override;
  bool Write(std::string const& data) override;
  bool Read(std::string* data) override;
  void Close() override;
};

#endif
