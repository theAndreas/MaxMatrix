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

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "SimulatorPanel.h"

namespace {

/* the milliseconds of a second, and the digits of a fraction that say them */
constexpr uint64_t kMillisecondsPerSecond{1000u};
constexpr size_t kMillisecondDigits{3u};
/* the digits of the most whole seconds whose milliseconds a uint32_t holds,
 * 4294967, so that a longer number is refused before it is read */
constexpr size_t kMaxSecondDigits{7u};

/* the digits a number is written in */
constexpr uint64_t kDecimalBase{10u};

// Whether `c` is a decimal digit, compared rather than asked of isdigit(),
// which depends on the locale (MISRA 24.5.1).
bool IsDigit(char c) { return (c >= '0') && (c <= '9'); }

// Whether `text` is decimal digits and nothing else; an empty one is.
bool IsDigits(std::string const& text) {
  return std::all_of(text.begin(), text.end(), IsDigit);
}

// The value of `digits`, decimal digits only, up to `maximum`: past it, the
// digits are no longer counted, so that a run too long for a uint64_t returns
// a value above `maximum` rather than wrapping round into range.
uint64_t ConvertDigits(std::string const& digits, uint64_t maximum) {
  uint64_t value{0};

  for (char const c : digits) {
    if (value <= maximum) {
      value = (value * kDecimalBase) + static_cast<uint64_t>(c - '0');
    }
  }
  return value;
}

}  // namespace

SimulatorOptions::SimulatorOptions()
    : wiring_name_(kSimulatorPanelDefaultWiring) {}

bool SimulatorOptions::Parse(int argc, char** argv) {
  // T-20: main() is handed its arguments as a pointer to an array
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<char const*> const arguments(argv, argv + argc);

  for (size_t i{1}; i < arguments.size(); i++) {
    if (!ParseOption(arguments, i)) {
      return false;
    }
  }
  return (expect_file_ == nullptr) || (run_for_ != 0);
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
 * Reads the option at `index`, and its value if it has one: `index` is then
 * moved on to the value. Returns false if the option is unknown, lacks its
 * value or has a value it cannot take.
 */
bool SimulatorOptions::ParseOption(std::vector<char const*> const& arguments,
                                   size_t& index) {
  std::string const option = arguments[index];

  if (option == "--upside-down") {
    upside_down_ = true;
    return true;
  }
  if (option == "--web") {
    web_ = true;
    return true;
  }
  if ((index + 1) >= arguments.size()) {
    return false;
  }
  ++index;
  return ParseValue(option, arguments[index]);
}

/*
 * Takes `value` for `option`, one that has a value. Returns false if the option
 * is unknown or cannot take the value.
 */
bool SimulatorOptions::ParseValue(std::string const& option,
                                  char const* value) {
  if (option == "--port") {
    return Keep(ConvertToPort(value), port_);
  }
  if (option == "--tcp") {
    return Keep(ConvertToPort(value), tcp_port_);
  }
  if (option == "--for") {
    return Keep(ConvertToRunFor(value), run_for_);
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
 * Converts the value of --port or --tcp into a port: a decimal number from 1 to
 * 65535 and nothing else. It is not valid if the value is no number, or out of
 * range.
 *
 * atoi() and a cast, as before, turned 70000 into port 4464 and "80x" into port
 * 80, so the simulator listened somewhere else than asked without a word;
 * strtoul() after it took a sign as well, "+80", and is one of the string
 * functions of C that MISRA 21.2.2 rules out.
 */
SimulatorOptions::Converted<uint16_t> SimulatorOptions::ConvertToPort(
    char const* text) {
  std::string const digits{text};
  bool const number{!digits.empty() && IsDigits(digits)};
  uint64_t const value{number ? ConvertDigits(digits, kSimulatorOptionsMaxPort)
                              : 0u};
  bool const in_range{(value != 0) && (value <= kSimulatorOptionsMaxPort)};

  return Converted<uint16_t>{number && in_range, static_cast<uint16_t>(value)};
}

/*
 * Converts the value of --for, the seconds as decimal digits with a fraction
 * after a point, `1.5` or `.5`, into milliseconds; digits of the fraction past
 * the third are dropped. They are not valid if the value is anything else, or
 * not a single millisecond, or more than the milliseconds fit.
 *
 * atof(), as before, took "2x" as two seconds, and a negative or a huge number
 * reached a cast to uint32_t that is undefined for it; strtod() after it took
 * those apart, but went through a double (H-7).
 */
SimulatorOptions::Converted<uint32_t> SimulatorOptions::ConvertToRunFor(
    char const* text) {
  std::string const value{text};
  size_t const point{value.find('.')};
  bool const has_point{point != std::string::npos};
  std::string const whole{value.substr(0, point)};
  std::string const fraction{has_point ? value.substr(point + 1)
                                       : std::string{}};
  bool const digits_only{IsDigits(whole) && IsDigits(fraction)};
  bool const has_digit{!whole.empty() || !fraction.empty()};

  if (!digits_only || !has_digit || (whole.size() > kMaxSecondDigits)) {
    return Converted<uint32_t>{false, 0};
  }
  uint64_t const seconds{ConvertDigits(whole, kSimulatorOptionsMaxRunFor)};
  std::string const thousandths{
      (fraction + "000").substr(0, kMillisecondDigits)};
  uint64_t const milliseconds{
      (seconds * kMillisecondsPerSecond) +
      ConvertDigits(thousandths, kMillisecondsPerSecond)};
  bool const in_range{(milliseconds >= 1) &&
                      (milliseconds <= kSimulatorOptionsMaxRunFor)};
  return Converted<uint32_t>{in_range, static_cast<uint32_t>(milliseconds)};
}
