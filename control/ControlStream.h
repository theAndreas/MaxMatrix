// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief How a port that never waits sends all and reads what has arrived: the
 *        serial port on Linux and the TCP port.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLSTREAM_H_
#define MAXMATRIX_CONTROL_CONTROLSTREAM_H_

#include <stddef.h>

#include <array>
#include <string>

/* the bytes ControlStream::ReceiveAvailable() reads at a time */
constexpr size_t kControlStreamChunkLength{256u};

/*!
 * \brief The loops of a port that never waits, whatever system calls it makes.
 *
 * The port hands in its calls: one that sends or reads some bytes and returns
 * how many, or less than 0 on an error; one that says whether that error is
 * only that the call would have had to wait; and to send, one that waits until
 * the port takes bytes again.
 */
class ControlStream {
 public:
  /*!
   * \brief Sends all of `data`, waiting while the port takes nothing.
   *
   * \param[in] data The bytes to send.
   * \param[in] send_some Sends up to a length of bytes from a pointer and
   *                      returns how many it took, or less than 0 on an error.
   * \param[in] would_block Returns whether that error was only that the port
   *                        took nothing for now.
   * \param[in] wait_writable Waits until the port takes bytes again, and
   *                          returns whether it does before it gives up.
   * \return True once all was sent, or false if the port is broken.
   */
  template <typename SendSome, typename WouldBlock, typename WaitWritable>
  static bool SendAll(std::string const& data, SendSome send_some,
                      WouldBlock would_block, WaitWritable wait_writable) {
    size_t sent{0};

    while (sent < data.size()) {
      auto const written{send_some(&data[sent], data.size() - sent)};
      if (written > 0) {
        sent += static_cast<size_t>(written);
      } else if ((written >= 0) || !would_block() || !wait_writable()) {
        return false;
      }
    }
    return true;
  }

  /*!
   * \brief Reads what has arrived, without waiting for more.
   *
   * \param[in,out] data Every byte that has arrived is appended.
   * \param[in] receive_some Reads up to a length of bytes to a pointer and
   *                         returns how many, 0 if the other end is gone, or
   *                         less than 0 on an error.
   * \param[in] would_block Returns whether that error was only that nothing has
   *                        arrived.
   * \return True while the port is open, or false once the other end is gone or
   *         the port is broken.
   */
  template <typename ReceiveSome, typename WouldBlock>
  static bool ReceiveAvailable(std::string& data, ReceiveSome receive_some,
                               WouldBlock would_block) {
    std::array<char, kControlStreamChunkLength> chunk{};

    for (;;) {
      auto const length{receive_some(chunk.data(), chunk.size())};
      if (length <= 0) {
        return (length < 0) && would_block();
      }
      data.append(chunk.data(), static_cast<size_t>(length));
    }
  }
};

#endif  // MAXMATRIX_CONTROL_CONTROLSTREAM_H_
