// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* What the host-side tests of MaxMatrix share, against the Arduino stand-in in
 * mock/: the fixtures, the chain as the tests see it, and how GoogleTest prints
 * a type of the library.
 *
 * Built three times by CMakeLists.txt, with bit banging, with
 * MAXMATRIX_USE_SPI=STD_ON and with MAXMATRIX_DEV_ERROR_DETECT, because the
 * transfer paths are separate code. What this cannot show is anything about
 * timing or the electrical side - whether a real MAX7219 accepts the frames is
 * only known on hardware. */
#ifndef MAXMATRIX_TEST_MAXMATRIXTESTSUPPORT_H_
#define MAXMATRIX_TEST_MAXMATRIXTESTSUPPORT_H_

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Arduino.h"

/* The parts of the library and the matrix buffer are private members of
 * Matrix, and some of what is tested here has no public path:
 * internal::Wired::setColumnOnAllModules is only reached through Clear(), which
 * zeroes the buffer afterwards and so hides a wrong index. Included after
 * GoogleTest and the standard headers, which must not see it - defined above
 * them, it rewrites their classes too. */
// What it redefines is a keyword, not a name of ours.
// NOLINTNEXTLINE(readability-identifier-naming)
#define private public
#include "MaxMatrix.h"
#undef private

static byte const kDataInPin = 11;
static byte const kChipSelectPin = 10;
static byte const kClockPin = 13;
/* the chain of the examples; the tests of other lengths make their own */
static byte const kNumberOfModules = 4;
static byte const kNumberOfColumns =
    kNumberOfModules * max_matrix::kColumnNumberOfModule;
/* a row of the whole panel, one byte per module */
// NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a row as a sketch has one
using PanelRow = byte[kNumberOfModules];

/* an array that is no font, for SetFont() */
// NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a font as a sketch has one
static uint8_t const kNoFont[] PROGMEM = {'G', 1, 32, 126, 8};

/* every error by name, in the order of the enum */
static std::array<char const*, 8> const kErrorNames{
    {"Error::kOk", "Error::kOutOfRange", "Error::kUnknownChar",
     "Error::kNoText", "Error::kNotANumber", "Error::kUnknownCommand",
     "Error::kLineTooLong", "Error::kNotAFont"}};
static uint8_t const kNumberOfErrors = kErrorNames.size();

/* GoogleTest finds what prints and compares a type of the library by ADL, so
 * these lie in its namespace; every test file sees them, as GoogleTest would
 * print a type differently in a file that did not */
namespace max_matrix {

/* GoogleTest would print an error as its number */
void PrintTo(Error error, std::ostream* out);

/* what Feed() returns, compared with a state as with an error: a line is done
 * only if it did not fail as well */
bool operator==(Result<CommandState> const& result, CommandState state);

void PrintTo(Result<CommandState> const& result, std::ostream* out);

}  // namespace max_matrix

/* where Command answers in a test: a Print that keeps what it is given
 */
class TextPrint final : public Print {
 public:
  std::string text;

  size_t write(uint8_t c) override {
    text += static_cast<char>(c);
    return 1;
  }
};

/* the text of F() and GetErrorText(), which lies in RAM on the host */
std::string Text(__FlashStringHelper const* text);

/* What the library reported, with MAXMATRIX_DEV_ERROR_DETECT on: the definition
 * of ReportError() in MaxMatrixTestSupport.cpp wins over the weak one of the
 * library, which prints. With it off the list stays empty, so that a test can
 * expect a report in every variant. */
using Report = std::pair<std::string, max_matrix::Error>;
static bool const kDevErrorDetect = (MAXMATRIX_DEV_ERROR_DETECT == STD_ON);
extern std::vector<Report> reports;

/* GoogleTest prints a vector element by element, where a failed memcmp only
 * says that something differs */
std::vector<byte> Bytes(byte const* data, size_t size);

/* the library sends the pair of module 0 first, so it ends up in the chip at
 * the far end of the chain */
Max7219 const& ChipOfModule(byte module);

/* what the chain shows, laid out like matrix_buffer_: a column of a module is a
 * digit, a row a segment */
std::vector<byte> Display();

class MaxMatrixTest : public ::testing::Test {
 protected:
  max_matrix::MatrixOf<kNumberOfModules> matrix_{kDataInPin, kChipSelectPin,
                                                 kClockPin};

  void SetUp() override {
    mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
    reports.clear();
    matrix_.Init();
    /* the tests step a shifting text with every call; the timing of the steps
     * has tests of its own */
    matrix_.SetShiftInterval(0);
  }

  /* whatever a test did, the LEDs have to show what the buffer says - the get
   * functions read the buffer, because the MAX7219 cannot be read back, so a
   * buffer that drifts from the display makes them lie; and every bit went out
   * within a frame, with the interrupts off, which are on again afterwards */
  void TearDown() override {
    EXPECT_EQ(Display(),
              Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns))
        << "display differs from matrix_buffer_";
    EXPECT_EQ(mock::errors, 0) << "a transfer outside a frame, or a port "
                                  "written with the interrupts on";
    EXPECT_EQ(SREG, 0x80) << "the interrupts left off";
  }

  /* the ShiftTask() calls the last ShiftWholeText() took, each one a step of
   * the text on the panel */
  uint16_t shift_calls_ = 0;

  /* returns the number of frames the shift sent to the chain */
  int32_t ShiftWholeText(char const* text) {
    matrix_.Clear();
    int32_t const frames_before{mock::frames};
    matrix_.SetTextWithShift(text);
    for (shift_calls_ = 0;
         shift_calls_ < 1000 && matrix_.GetState() != max_matrix::State::kReady;
         shift_calls_++) {
      matrix_.ShiftTask();
    }
    return mock::frames - frames_before;
  }
};
static std::array<max_matrix::MatrixOrientation, 2> const kMatrixOrientations{
    {max_matrix::MatrixOrientation::k0, max_matrix::MatrixOrientation::k180}};

/* all eight, in the order of the model's kMax7219Wirings, which
 * MaxMatrixModuleTest checks */
static std::array<max_matrix::ModuleType, 8> const kModuleTypes{
    {max_matrix::ModuleType::kDr0Cr0Rr0, max_matrix::ModuleType::kDr0Cr0Rr1,
     max_matrix::ModuleType::kDr0Cr1Rr0, max_matrix::ModuleType::kDr0Cr1Rr1,
     max_matrix::ModuleType::kDr1Cr0Rr0, max_matrix::ModuleType::kDr1Cr0Rr1,
     max_matrix::ModuleType::kDr1Cr1Rr0, max_matrix::ModuleType::kDr1Cr1Rr1}};

/* the name of a module type as the model names its wiring, DR0CR0RR0 to
 * DR1CR1RR1 */
std::string ModuleTypeName(max_matrix::ModuleType module_type);

using MappingPair =
    std::tuple<max_matrix::MatrixOrientation, max_matrix::ModuleType>;

/* every test of this fixture runs in all sixteen combinations of matrix
 * orientation and module type */
class MaxMatrixMappingTest : public MaxMatrixTest,
                             public ::testing::WithParamInterface<MappingPair> {
 protected:
  void SetUp() override {
    MaxMatrixTest::SetUp();
    matrix_.Clear();
    matrix_.SetMatrixOrientation(std::get<0>(GetParam()));
    matrix_.SetModuleType(std::get<1>(GetParam()));
  }
};

/* whether the dot at x, y is lit in the pattern the tests draw */
bool Pattern(int16_t x, int16_t y);

/* a picture as rows of '#' and '.', so that a failure prints both pictures */
template <typename DotType>
std::vector<std::string> Picture(DotType dot, byte columns = kNumberOfColumns) {
  std::vector<std::string> rows;
  for (int16_t y = 0; y < max_matrix::kRowNumberOfModule; y++) {
    std::string row;
    for (int16_t x = 0; x < columns; x++) row += dot(x, y) ? '#' : '.';
    rows.push_back(row);
  }
  return rows;
}

/* the picture as the library reports it, through GetDot */
std::vector<std::string> ShownPicture(max_matrix::Matrix& matrix);

/* draws Pattern() on the whole panel, dot by dot */
void DrawPattern(max_matrix::Matrix& matrix);

#endif  // MAXMATRIX_TEST_MAXMATRIXTESTSUPPORT_H_
