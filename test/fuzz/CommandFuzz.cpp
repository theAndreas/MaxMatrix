// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The fuzzer of Command, the one parser of the library that reads from outside:
 * what a control page or program sends a board over its serial port. Every
 * input is fed char by char to a matrix of its own, as Serial delivers it, and
 * a shift it started is stepped on, so that a text kept past its line is read.
 * ASan and UBSan find a read out of bounds and undefined behaviour without a
 * word of this file's; what it states on top is what a program at the other end
 * relies on. */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <string>

#include "Arduino.h"
#include "MaxMatrix.h"
#include "MaxMatrixAttributes.h"
#include "avr/io.h"

namespace {

using max_matrix::ChipSelectPin;
using max_matrix::ClockPin;
using max_matrix::Command;
using max_matrix::DataInPin;
using max_matrix::Interval;
using max_matrix::MatrixOf;
using max_matrix::State;

constexpr uint8_t kDataInPin{11};
constexpr uint8_t kChipSelectPin{10};
constexpr uint8_t kClockPin{13};
/* SerialControl's chain, whose picture is 32 columns */
constexpr uint8_t kNumberOfModules{4};
/* "picture=" and two hex digits a column */
constexpr size_t kPictureLength{8 + (2 * 32)};
/* more steps than a line of text has columns, at the least space */
constexpr uint16_t kShiftSteps{512};

/* what the matrix answers, as a program reads it */
class Answer final : public Print {
 public:
  std::string text;

  size_t write(uint8_t c) override {
    text += static_cast<char>(c);
    return 1;
  }
};

bool IsHexDigit(char c) {
  bool const digit{(c >= '0') && (c <= '9')};
  bool const letter{(c >= 'A') && (c <= 'F')};
  return digit || letter;
}

/* whether a line of the answer, without its "\r\n", is one a program knows:
 * ok, an error, or a picture of every column */
bool IsKnownAnswer(std::string const& line) {
  bool known{(line == "ok") || (line.compare(0, 7, "error: ") == 0)};
  if (!known && (line.size() == kPictureLength) &&
      (line.compare(0, 8, "picture=") == 0)) {
    known = true;
    for (size_t i{8}; i < line.size(); i++) {
      known = known && IsHexDigit(line[i]);
    }
  }
  return known;
}

/* Stops the run where what a program relies on does not hold, so that libFuzzer
 * keeps the input. */
void Require(bool holds) {
  if (!holds) {
    // MISRA 18.5.2: how a fuzzer reports what does not hold (Testing TS-29)
    abort();  // NOLINT(bugprone-unsafe-functions)
  }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(uint8_t const* data, size_t size) {
  /* every input from the same state, as a board that has just started */
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  MatrixOf<kNumberOfModules> matrix{DataInPin{kDataInPin},
                                    ChipSelectPin{kChipSelectPin},
                                    ClockPin{kClockPin}};
  Require(matrix.Init().IsOk());
  /* speed= sets it; the steps below come one per call regardless */
  Interval shift_interval{50};
  Command command;
  Answer answer;
  size_t line_ends{0};

  max_matrix::span<uint8_t const> const input{data, size};
  for (size_t i{0}; i < input.size(); i++) {
    char const c{static_cast<char>(input[i])};
    line_ends += (c == '\n') ? 1u : 0u;
    // what a line brought is checked in the answer, after the last
    max_matrix::Discard(command.Feed(matrix, shift_interval, c, answer));
  }
  uint16_t step{0};
  while ((step < kShiftSteps) && (matrix.GetState() != State::kReady)) {
    matrix.ShiftStep();
    step++;
  }

  /* every bit went out within a frame, with the interrupts off, which are on
   * again */
  Require(mock::errors == 0);
  Require(SREG == 0x80);
  /* one line back for every line at most, each one a program knows */
  size_t answered{0};
  size_t start{0};
  size_t end{answer.text.find("\r\n")};
  while (end != std::string::npos) {
    Require(IsKnownAnswer(answer.text.substr(start, end - start)));
    answered++;
    start = end + 2;
    end = answer.text.find("\r\n", start);
  }
  Require(start == answer.text.size());
  Require(answered <= line_ends);
  return 0;
}
