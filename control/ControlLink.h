// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The lines between the control program and one SerialControl, on a board or in the
 *        simulator.
 */

#ifndef MAXMATRIX_CONTROL_CONTROLLINK_H_
#define MAXMATRIX_CONTROL_CONTROLLINK_H_

#include <string>
#include <vector>
#include "ControlPort.h"

/* ControlLink Parameter */
/* what SerialControl says at the end of setup() */
#define CONTROL_LINK_READY                                          "ready"
/* the longest line kept whole; a sketch that never ends a line is shown in pieces of this length */
#define CONTROL_LINK_MAX_LINE_LENGTH                                256u

/*!
 * \brief The lines between the control program and one SerialControl, on a board or in the
 *        simulator.
 *
 * It sends a command as a line and cuts what comes back into lines, without the `\\r\\n` the
 * sketch ends them with. A board resets when its port is opened, and what is sent before its sketch
 * has started is lost, so on a board the link sends nothing until SerialControl has said `ready`.
 * The simulator's sketch said so long before the program connected, so there the link is ready at
 * once.
 */
class ControlLink
{
  private:
    ControlPort& port_;
    bool ready_;
    /* what has arrived after the last line feed */
    std::string received_;

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
     * \param[in] wait_for_ready True to send nothing until SerialControl says `ready`, as on a
     *                           board; false to be ready at once, as with the simulator.
     */
    void Begin(bool wait_for_ready);

    /*! \brief Returns whether the port is open, ready or not. */
    bool IsOpen() const { return port_.IsOpen(); }

    /*! \brief Returns whether the port is open and SerialControl takes commands. */
    bool IsReady() const;

    /*!
     * \brief Sends a command.
     *
     * \param[in] line The command, without its line feed.
     * \return True once it was sent, or false if the link is not ready or the port broke.
     */
    bool Send(std::string const& line);

    /*!
     * \brief Reads what has arrived and cuts it into lines.
     *
     * Call it often, from a timer: it never waits.
     *
     * \param[out] lines Every whole line that has arrived is appended, without its line end.
     * \return True while the port is open, or false once it is closed or its other end is gone.
     */
    bool Task(std::vector<std::string>* lines);
};

#endif
