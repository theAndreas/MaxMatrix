// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The command line of the simulator.
 *
 * [--elf FILE] [--wiring NAME] [--upside-down] [--serial LINE]... [--web
 * [--port PORT]]
 * [--for SECONDS [--expect FILE]].
 */

#ifndef MAXMATRIX_SIMULATOR_SIMULATOROPTIONS_H_
#define MAXMATRIX_SIMULATOR_SIMULATOROPTIONS_H_

#include <string>
#include <vector>

#include "MaxMatrixAttributes.h"
#include "SimulatorPanel.h"

/* SimulatorOptions Parameter */
constexpr uint16_t kSimulatorOptionsDefaultPort{8080U};
constexpr uint16_t kSimulatorOptionsMaxPort{65535U};
constexpr uint32_t kSimulatorOptionsMaxRunFor{UINT32_MAX}; /* milliseconds */

/*!
 * \brief The command line of the simulator.
 *
 * It only reads the command line. Whether the wiring exists is for
 * SimulatorPanel to say, which knows the names; whether a simulator takes an
 * `.elf` is for the simulator itself: SimulatorAvr needs one, the simulator a
 * sketch is built into refuses it.
 */
class SimulatorOptions {
 private:
  std::string wiring_name_;
  bool upside_down_{false};
  bool web_{false};
  uint16_t port_{kSimulatorOptionsDefaultPort};
  uint16_t tcp_port_{0};
  uint32_t run_for_{0};
  char const* expect_file_{nullptr};
  char const* elf_file_{nullptr};
  std::string serial_input_;

  bool ParseOption(std::vector<char const*> const& arguments, size_t* index);
  bool ParseValue(std::string const& option, char const* value);
  static bool ConvertToPort(char const* text, uint16_t* port);
  static bool ConvertToRunFor(char const* text, uint32_t* run_for);

 public:
  /*!
   * \brief Makes the options a simulator has without any on its command line.
   *
   * They are the default wiring, mounted upright, in the terminal, until
   * Ctrl+C.
   */
  SimulatorOptions();

  /*! \brief Returns the name of `--wiring`, or `kSimulatorPanelDefaultWiring`
   * without it. */
  MAXMATRIX_NODISCARD std::string const& GetWiringName() const {
    return wiring_name_;
  }

  /*! \brief Returns whether `--upside-down` was given. */
  MAXMATRIX_NODISCARD bool IsUpsideDown() const { return upside_down_; }

  /*! \brief Returns whether `--web` was given. */
  MAXMATRIX_NODISCARD bool IsWeb() const { return web_; }

  /*! \brief Returns the port of `--port`, or `kSimulatorOptionsDefaultPort`
   * without it. */
  MAXMATRIX_NODISCARD uint16_t GetPort() const { return port_; }

  /*! \brief Returns the port of `--tcp`, or 0 without it, for no serial port on
   * TCP. */
  MAXMATRIX_NODISCARD uint16_t GetTcpPort() const { return tcp_port_; }

  /*! \brief Returns the milliseconds of `--for`, or 0 without it, to run until
   * Ctrl+C. */
  MAXMATRIX_NODISCARD uint32_t GetRunFor() const { return run_for_; }

  /*! \brief Returns the file of `--expect`, or nullptr without it. */
  MAXMATRIX_NODISCARD char const* GetExpectFile() const { return expect_file_; }

  /*! \brief Returns the file of `--elf`, or nullptr without it. */
  MAXMATRIX_NODISCARD char const* GetElfFile() const { return elf_file_; }

  /*!
   * \brief Returns the lines of every `--serial`, each ended with a line feed,
   *        for the sketch's `Serial` to read.
   */
  MAXMATRIX_NODISCARD std::string const& GetSerialInput() const {
    return serial_input_;
  }

  /*!
   * \brief Reads the command line, one option after the other.
   *
   * `--expect` only makes sense with `--for`, which ends the run at a time the
   * expected panel was taken at; without it the command line is wrong.
   *
   * \param[in] argc The number of arguments, the program's name included.
   * \param[in] argv The arguments.
   * \return True once every option is read, or false if the command line is
   *         wrong; PrintUsage() says how it should be.
   */
  bool Parse(int argc, char** argv);

  /*!
   * \brief Says how the command line should be, on stderr, beside whatever went
   *        wrong.
   *
   * \param[in] program The program's name, `argv[0]`.
   */
  static void PrintUsage(char const* program);
};

#endif  // MAXMATRIX_SIMULATOR_SIMULATOROPTIONS_H_
