// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The lines between the control program and one SerialControl, on a
 *        board or in the simulator.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLLINK_H_
#define MAXMATRIX_CONTROL_CONTROLLINK_H_

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "ControlPort.h"
#include "MaxMatrixAttributes.h"

/* ControlLink Parameter */
/* what SerialControl says at the end of setup() */
constexpr char kControlLinkReady[] = "ready";
/* the longest line kept whole; a sketch that never ends a line is shown in
 * pieces of this length */
constexpr size_t kControlLinkMaxLineLength{256U};
/* how often the picture is asked for at most: 20 times a second, whose answers
 * take an eighth of 115200 baud */
constexpr uint32_t kControlLinkPictureIntervalMs{50U};
/* how long an answer to picture is waited for before it is taken as lost and
 * asked for again */
constexpr uint32_t kControlLinkPictureTimeoutMs{1000U};

/*!
 * \brief The lines between the control program and one SerialControl, on a
 *        board or in the simulator.
 *
 * It sends a command as a line and cuts what comes back into lines, without the
 * `\\r\\n` the sketch ends them with. A board resets when its port is opened,
 * and what is sent before its sketch has started is lost, so on a board the
 * link sends nothing until SerialControl has said `ready`. The simulator's
 * sketch said so long before the program connected, so there the link is ready
 * at once.
 *
 * It also keeps what the matrix shows, as SerialControl answers `picture`:
 * AskForPicture() asks for it over and over, and the answer is kept rather than
 * passed on as a line.
 */
class ControlLink {
 private:
  std::reference_wrapper<ControlPort> port_;
  bool ready_{false};
  /* what has arrived after the last line feed */
  std::string received_;
  std::vector<uint8_t> picture_;
  /*
   * Whether an answer to picture is due, and since when; picture_asked_at_
   * holds a time only once the picture has been asked for since Begin(), so
   * that the first time it is at once. SerialControl answers in order, so an
   * answer to a picture asked for before a command shows the matrix before it,
   * and is dropped as stale.
   */
  bool picture_asked_{false};
  bool picture_stale_{false};
  bool picture_timed_{false};
  uint32_t picture_asked_at_{0};

  void Take(std::string line, std::vector<std::string>* lines);

 public:
  /*!
   * \brief Makes a link over `port`, which is not ready until Begin().
   *
   * \param[in] port The port, which has to live as long as the link.
   */
  explicit ControlLink(ControlPort& port);

  /*!
   * \brief Starts over on a port that has just been opened.
   *
   * \param[in] wait_for_ready True to send nothing until SerialControl says
   *                           `ready`, as on a board; false to be ready at
   *                           once, as with the simulator.
   */
  void Begin(bool wait_for_ready);

  /*! \brief Returns whether the port is open, ready or not. */
  MAXMATRIX_NODISCARD bool IsOpen() const { return port_.get().IsOpen(); }

  /*! \brief Returns whether the port is open and SerialControl takes commands.
   */
  MAXMATRIX_NODISCARD bool IsReady() const;

  /*!
   * \brief Sends a command.
   *
   * \param[in] line The command, without its line feed.
   * \return True once it was sent, or false if the link is not ready or the
   *         port broke.
   */
  bool Send(std::string const& line);

  /*!
   * \brief Asks SerialControl what the matrix shows, if it is time to.
   *
   * Call it often, from a timer: it asks at most every
   * `kControlLinkPictureIntervalMs`, and not again while an answer is due,
   * unless it has not come for `kControlLinkPictureTimeoutMs`.
   *
   * \param[in] now The time in milliseconds, from any clock that does not jump.
   */
  void AskForPicture(uint32_t now);

  /*!
   * \brief Returns what the matrix showed at the last answer to `picture`.
   *
   * \return One byte per column as the viewer sees it, from the left, bit 0 the
   *         top row; empty until the first answer after Begin().
   */
  MAXMATRIX_NODISCARD std::vector<uint8_t> const& GetPicture() const {
    return picture_;
  }

  /*!
   * \brief Reads what has arrived and cuts it into lines.
   *
   * Call it often, from a timer: it never waits.
   *
   * \param[out] lines Every whole line that has arrived is appended, without
   *                   its line end, but an answer to `picture`, which
   *                   GetPicture() returns.
   * \return True while the port is open, or false once it is closed or its
   *         other end is gone.
   */
  bool Task(std::vector<std::string>* lines);
};

#endif  // MAXMATRIX_CONTROL_CONTROLLINK_H_
