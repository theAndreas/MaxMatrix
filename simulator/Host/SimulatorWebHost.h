// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A small HTTP and WebSocket server for the simulator, on loopback only.
 *
 * Serves the page and keeps the WebSockets open to it. Ported from Wordclock's
 * platform/simulator/src/WebHost.cpp, without what only the clock needs (the
 * firmware update route, the JSON documents, Windows).
 */

#ifndef MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBHOST_H_
#define MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBHOST_H_

#include <stdint.h>

#include <array>
#include <string>
#include <vector>

/* SimulatorWebHost Parameter */
constexpr uint8_t kSimulatorWebHostMaxClients{4u};

/*!
 * \brief A small HTTP and WebSocket server for the simulator, on loopback only.
 *
 * It is polled from the simulator's tick rather than run in a thread, so
 * nothing in it needs a lock: every call happens between two steps of the
 * sketch.
 */
class SimulatorWebHost {
 private:
  int listener_;
  std::array<int, kSimulatorWebHostMaxClients> clients_{};
  std::array<std::string, kSimulatorWebHostMaxClients> pending_;
  char const* page_path_{nullptr};

  void AcceptClients();
  void ReadClient(uint8_t slot, std::vector<std::string>* messages);
  bool AnswerRequest(int socket, std::string const& request);
  void SendPage(int socket);
  uint8_t FindFreeSlot() const;
  void DropClient(uint8_t slot);
  static std::string ReadRequest(int socket);
  static bool GetPath(std::string const& request, std::string* path);
  static void SendResponse(int socket, char const* status,
                           char const* content_type, std::string const& body);

 public:
  /*! \brief Makes a host that listens nowhere until Begin(). */
  SimulatorWebHost();

  /*!
   * \brief Opens the port on localhost and starts listening.
   *
   * The page is read from its path on every request, so that a change to it
   * needs a reload in the browser, not a rebuild.
   *
   * \param[in] port The port to listen on.
   * \param[in] page_path The page to serve on `/`. The host keeps the pointer,
   *                      so it has to live as long as the host.
   * \return True once the host listens, or false if the port is taken, so that
   *         the caller can say so.
   */
  bool Begin(uint16_t port, char const* page_path);

  /*!
   * \brief Answers whatever has arrived.
   *
   * It accepts and answers new connections and reads the frames of the open
   * WebSockets. Until Begin() has opened the port, it does nothing.
   *
   * \param[in,out] messages Every text message a page has sent since the last
   *                         call is appended.
   */
  void Task(std::vector<std::string>* messages);

  /*!
   * \brief Sends a text to every open WebSocket.
   *
   * A client it cannot send to is dropped.
   *
   * \param[in] text The message.
   */
  void Broadcast(std::string const& text);
};

#endif  // MAXMATRIX_SIMULATOR_HOST_SIMULATORWEBHOST_H_
