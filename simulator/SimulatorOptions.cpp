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

#include <stdio.h>
#include <stdlib.h>

SimulatorOptions::SimulatorOptions() {
  wiring_name_ = SIMULATOR_PANEL_DEFAULT_WIRING;
  upside_down_ = false;
  web_ = false;
  port_ = SIMULATOR_OPTIONS_DEFAULT_PORT;
  tcp_port_ = 0;
  run_for_ = 0;
  expect_file_ = nullptr;
  elf_file_ = nullptr;
}

bool SimulatorOptions::Parse(int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if (!ParseOption(argc, argv, &i)) return false;
  }
  return expect_file_ == nullptr || run_for_ != 0;
}

void SimulatorOptions::PrintUsage(char const* program) {
  fprintf(stderr,
          "usage: %s [--elf FILE] [--wiring NAME] [--upside-down] [--serial "
          "LINE]... [--web [--port PORT]]\n",
          program);
  fprintf(stderr, "         [--tcp PORT] [--for SECONDS [--expect FILE]]\n");
  fprintf(stderr,
          "  FILE of --elf: the example built for the Uno; SimulatorAvr only, "
          "which needs it\n");
  fprintf(stderr,
          "  NAME: FC16 (default), GENERIC, PAROLA, ICSTATION or DR0CR0RR0 ... "
          "DR1CR1RR1\n");
}

/*
 * Reads the option at `*index`, and its value if it has one: `*index` is then
 * moved on to the value. Returns false if the option is unknown, lacks its
 * value or has a value it cannot take.
 */
bool SimulatorOptions::ParseOption(int argc, char** argv, int* index) {
  std::string const option = argv[*index];
  bool const has_value = *index + 1 < argc;

  if (option == "--upside-down") {
    upside_down_ = true;
  } else if (option == "--web") {
    web_ = true;
  } else if (option == "--wiring" && has_value) {
    wiring_name_ = argv[++*index];
  } else if (option == "--port" && has_value) {
    if (!ConvertToPort(argv[++*index], &port_)) return false;
  } else if (option == "--tcp" && has_value) {
    if (!ConvertToPort(argv[++*index], &tcp_port_)) return false;
  } else if (option == "--for" && has_value) {
    if (!ConvertToRunFor(argv[++*index], &run_for_)) return false;
  } else if (option == "--expect" && has_value) {
    expect_file_ = argv[++*index];
  } else if (option == "--elf" && has_value) {
    elf_file_ = argv[++*index];
  } else if (option == "--serial" && has_value) {
    serial_input_ += std::string(argv[++*index]) + "\n";
  } else
    return false;
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
  char* end;
  unsigned long const value = strtoul(text, &end, 10);

  /* strtoul() takes a sign and wraps "-1" round to ULONG_MAX, which the range
   * check then refuses */
  if (end == text || *end != '\0' || value == 0 ||
      value > SIMULATOR_OPTIONS_MAX_PORT)
    return false;
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
  char* end;
  double const milliseconds = strtod(text, &end) * 1000;

  /* written so that NaN, which compares false with everything, is refused as
   * well */
  if (end == text || *end != '\0' ||
      !(milliseconds >= 1 && milliseconds <= SIMULATOR_OPTIONS_MAX_RUN_FOR))
    return false;
  *run_for = static_cast<uint32_t>(milliseconds);
  return true;
}
