// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The simulator's panel in a browser.
 *
 * What the page in simulator/Web/index.html is sent - the panel, the brightness
 * of every module, the frames on the wire - and what it sends back: the wiring
 * and the mounting to choose while the sketch runs.
 */

#ifndef MAXMATRIX_SIMULATOR_WEB_SIMULATORWEB_H_
#define MAXMATRIX_SIMULATOR_WEB_SIMULATORWEB_H_

#include <string>
#include <vector>

#include "Host/SimulatorWebHost.h"
#include "SimulatorPanel.h"

/* SimulatorWeb Parameter */
/* the most frames the page is sent at once; more would scroll past before
 * anyone read them */
constexpr size_t kSimulatorWebFrameLogLength{16u};

/*!
 * \brief The simulator's panel in a browser.
 *
 * Every function that needs the panel is handed it.
 */
class SimulatorWeb {
 private:
  // Every function that needs the panel is handed it, as
  // max_matrix::internal::Text is, rather than the class keeping a reference:
  // the panel belongs to the simulator, and the terminal is handed it too.

  SimulatorWebHost host_;
  /* the frames since the page was last sent the panel, as hex pairs, one string
   * per frame */
  std::vector<std::string> frame_log_;
  int32_t logged_frames_;
  /* what the sketch printed since the page was last sent the panel */
  std::string serial_output_;

  void LogFrame();
  static void Command(SimulatorPanel& panel, std::string const& message);
  std::string GetMessage(SimulatorPanel const& panel) const;
  std::string GetLog() const;
  static std::string GetIntensities(SimulatorPanel const& panel);

 public:
  /*! \brief Makes a web view that has logged no frame yet. */
  SimulatorWeb();

  /*! \copydoc SimulatorWebHost::Begin(uint16_t, char const*) */
  bool Begin(uint16_t port, char const* page_path);

  /*!
   * \brief Follows the sketch and the page.
   *
   * It logs a new frame and carries out what the page has sent. It is called
   * after every frame, so that each one gets logged and the page shows them as
   * they pass. What the sketch has printed asks for the panel too: an answer
   * such as "ok" changes no LED, and would otherwise wait for the next frame.
   *
   * \param[in,out] panel The panel the page's commands change.
   * \param[in] printed What the sketch has printed since the last call; it goes
   *                    to the page with the next panel.
   */
  void Task(SimulatorPanel& panel, std::string const& printed);

  /*!
   * \brief Sends the panel to the page.
   *
   * The frames logged and what the sketch printed since the last time go along,
   * and both start over.
   *
   * \param[in] panel The panel to send.
   */
  void Send(SimulatorPanel const& panel);

  /*!
   * \brief Converts text into the inside of a JSON string.
   *
   * Quotes and backslashes are escaped, and every byte outside printable ASCII
   * is written as
   * \\u00XX, the byte read as latin-1.
   *
   * \param[in] text The bytes, as the sketch printed them.
   * \return The text for between the quotes of a JSON string.
   */
  static std::string ConvertToJsonString(std::string const& text);
};

#endif  // MAXMATRIX_SIMULATOR_WEB_SIMULATORWEB_H_
