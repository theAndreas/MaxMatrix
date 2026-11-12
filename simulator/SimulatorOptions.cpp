// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The command line of the simulator.
 *
 * Parsed once at the start; an option that is unknown, lacks its value or has a
 * value of zero fails it.
 */

#include "SimulatorOptions.h"

#include <stdlib.h>

#include <iostream>
#include <string>
#include <vector>

SimulatorOptions::SimulatorOptions()
    : wiring_name_(kSimulatorPanelDefaultWiring) {}

bool SimulatorOptions::Parse(int argc, char** argv) {
  // T-20: main() is handed its arguments as a pointer to an array
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<char const*> const arguments(argv, argv + argc);

  for (size_t i{1}; i < arguments.size(); i++) {
    if (!ParseOption(arguments, &i)) {
      return false;
    }
  }
  return expect_file_ == nullptr || run_for_ != 0;
}

void SimulatorOptions::PrintUsage(char const* program) {
  std::cerr << "usage: " << program
            << " [--elf FILE] [--wiring NAME] [--upside-down] [--serial "
               "LINE]... [--web [--port PORT]]\n"
               "         [--tcp PORT] [--for SECONDS [--expect FILE]]\n"
               "  FILE of --elf: the example built for the Uno; SimulatorAvr "
               "only, which needs it\n"
               "  NAME: FC16 (default), GENERIC, PAROLA, ICSTATION or "
               "DR0CR0RR0 ... DR1CR1RR1\n";
}

/*
 * Reads the option at `*index`, and its value if it has one: `*index` is then
 * moved on to the value. Returns false if the option is unknown, lacks its
 * value or has a value it cannot take.
 */
bool SimulatorOptions::ParseOption(std::vector<char const*> const& arguments,
                                   size_t* index) {
  std::string const option = arguments[*index];

  if (option == "--upside-down") {
    upside_down_ = true;
    return true;
  }
  if (option == "--web") {
    web_ = true;
    return true;
  }
  if (*index + 1 >= arguments.size()) {
    return false;
  }
  ++*index;
  return ParseValue(option, arguments[*index]);
}

/*
 * Takes `value` for `option`, one that has a value. Returns false if the option
 * is unknown or cannot take the value.
 */
bool SimulatorOptions::ParseValue(std::string const& option,
                                  char const* value) {
  if (option == "--port") {
    return ConvertToPort(value, &port_);
  }
  if (option == "--tcp") {
    return ConvertToPort(value, &tcp_port_);
  }
  if (option == "--for") {
    return ConvertToRunFor(value, &run_for_);
  }
  if (option == "--wiring") {
    wiring_name_ = value;
  } else if (option == "--expect") {
    expect_file_ = value;
  } else if (option == "--elf") {
    elf_file_ = value;
  } else if (option == "--serial") {
    serial_input_ += std::string(value) + "\n";
  } else {
    return false;
  }
  return true;
}

/*
 * Converts the value of --port or --tcp into `*port`: a decimal number from 1
 * to 65535 and nothing else. Returns false if the value is no number, or out of
 * range; `*port` is then left as it was.
 *
 * atoi() and a cast, as before, turned 70000 into port 4464 and "80x" into port
 * 80, so the simulator listened somewhere else than asked without a word.
 */
bool SimulatorOptions::ConvertToPort(char const* text, uint16_t* port) {
  char* end{nullptr};
  // uint64_t holds what strtoul() returns on every host, until the range check
  // below.
  uint64_t const value{strtoul(text, &end, 10)};

  /* strtoul() takes a sign and wraps "-1" round to ULONG_MAX, which the range
   * check then refuses */
  if (end == text || *end != '\0' || value == 0 ||
      value > kSimulatorOptionsMaxPort) {
    return false;
  }
  *port = static_cast<uint16_t>(value);
  return true;
}

/*
 * Converts the value of --for, the seconds as a decimal number, fractions
 * included, and nothing else, into the milliseconds `*run_for`. Returns false
 * if the value is no number, or not a single millisecond, or more than the
 * milliseconds fit; `*run_for` is then left as it was.
 *
 * atof(), as before, took "2x" as two seconds, and a negative or a huge number
 * reached a cast to uint32_t that is undefined for it.
 */
bool SimulatorOptions::ConvertToRunFor(char const* text, uint32_t* run_for) {
  char* end{nullptr};
  double const milliseconds{strtod(text, &end) * 1000};

  /* asked as whether it lies in the range, so that NaN, which compares false
   * with everything, is refused as well */
  bool const in_range{milliseconds >= 1 &&
                      milliseconds <= kSimulatorOptionsMaxRunFor};
  if (end == text || *end != '\0' || !in_range) {
    return false;
  }
  *run_for = static_cast<uint32_t>(milliseconds);
  return true;
}
