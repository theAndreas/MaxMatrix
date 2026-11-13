// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief One end the control program talks to: a board's serial port or the
 *        simulator's TCP port.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLPORT_H_
#define MAXMATRIX_CONTROL_CONTROLPORT_H_

#include <string>

#include "MaxMatrixAttributes.h"

/*!
 * \brief One end the control program talks to: a board's serial port or the
 *        simulator's TCP port.
 *
 * Both carry the same lines, so that to the program both ends look alike and
 * ControlLink needs to know neither. How a port is opened differs, so each kind
 * has an Open() of its own.
 *
 * Virtual functions, which the library avoids for the RAM of their tables on
 * the Uno: the program runs on a PC, and a test hands ControlLink a port of its
 * own.
 */
class ControlPort {
 public:
  /*! \brief Closes nothing: each kind of port closes itself when it is
   * destroyed. */
  virtual ~ControlPort() = default;

  /*! \brief Makes a port that is not open. */
  ControlPort() = default;

  /*! \brief Not copyable: the copy would close the same port again. */
  ControlPort(ControlPort const&) = delete;

  /*! \brief Not assignable, as it is not copyable. */
  ControlPort& operator=(ControlPort const&) = delete;

  /*! \brief Not movable: a test's port and the window's stay where they are. */
  ControlPort(ControlPort&&) = delete;

  /*! \brief Not movable, as it is not copyable. */
  ControlPort& operator=(ControlPort&&) = delete;

  /*! \brief Returns whether the port is open. */
  MAXMATRIX_NODISCARD virtual bool IsOpen() const = 0;

  /*!
   * \brief Sends all of `data`.
   *
   * \param[in] data The bytes to send.
   * \return True once all was sent, or false if the port is closed or broken; a
   *         broken port is closed.
   */
  virtual bool Write(std::string const& data) = 0;

  /*!
   * \brief Reads what has arrived, without waiting for more.
   *
   * \param[in,out] data Every byte that has arrived since the last call is
   *                     appended.
   * \return True while the port is open, or false once the other end is gone:
   *         the cable pulled, the simulator stopped. The port is closed then.
   */
  virtual bool Read(std::string* data) = 0;

  /*! \brief Closes the port, if it is open. */
  virtual void Close() = 0;
};

#endif  // MAXMATRIX_CONTROL_CONTROLPORT_H_
