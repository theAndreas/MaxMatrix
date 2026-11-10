// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A board's serial port, over `termios` on Linux and the Win32 API on
 *        Windows.
 *
 * Rather than libserialport: one port, text only, and no further dependency.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLSERIALPORT_H_
#define MAXMATRIX_CONTROL_CONTROLSERIALPORT_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ControlPort.h"

/*!
 * \brief A board's serial port, over `termios` on Linux and the Win32 API on
 *        Windows.
 *
 * 8 data bits, no parity, one stop bit, no flow control, as the Arduino IDE's
 * serial monitor opens it. Opening the port resets an Uno, so what is sent
 * before its sketch has started is lost; ControlLink waits for SerialControl to
 * say `ready`.
 */
class ControlSerialPort final : public ControlPort {
 private:
#ifdef _WIN32
  void* handle_;
#else
  int descriptor_;
#endif

 public:
  /*! \brief Makes a port that is closed until Open(). */
  ControlSerialPort();

  /*! \brief Closes the port. */
  ~ControlSerialPort() override;

  // A copy would close the port of the original a second time.
  ControlSerialPort(ControlSerialPort const&) = delete;
  ControlSerialPort& operator=(ControlSerialPort const&) = delete;

  /*!
   * \brief Opens the port `name` at `baud_rate`.
   *
   * A port that is open already is closed first.
   *
   * \param[in] name The port: `/dev/ttyACM0` on Linux, `COM3` on Windows.
   * \param[in] baud_rate The baud rate, one of 9600, 19200, 38400, 57600 and
   *                      115200.
   * \return True once the port is open, or false if it does not exist, is
   *         taken, or does not take the baud rate.
   */
  bool Open(std::string const& name, uint32_t baud_rate);

  /*!
   * \brief Returns the serial ports a board may be on.
   *
   * On Linux the USB ones, `/dev/ttyACM*` and `/dev/ttyUSB*`, which is where an
   * Uno and the common USB adapters appear; on Windows every `COM` port. A port
   * of another name can still be opened by its name.
   *
   * \return The names, sorted.
   */
  static std::vector<std::string> List();

  bool IsOpen() const override;
  bool Write(std::string const& data) override;
  bool Read(std::string* data) override;
  void Close() override;
};

#endif
