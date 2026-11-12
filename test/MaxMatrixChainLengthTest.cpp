// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Chains of other lengths than the examples'. */

#include <array>
#include <string>
#include <tuple>
#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::Command;
using max_matrix::Error;
using max_matrix::Matrix;
using max_matrix::MatrixOrientation;
using max_matrix::ModuleType;
using max_matrix::ShiftFill;
using max_matrix::State;

namespace {

/* Chains of other lengths than the examples\', in every orientation: one
 * module, which is a pair of itself when the orientation changes, three, with
 * one module in the middle, and six. The buffer is the test's, filled with a
 * pattern that the matrix has to empty. */
std::array<byte, 3> const kChainLengths{{1, 3, 6}};
using ChainParameter = std::tuple<byte, MatrixOrientation, ModuleType>;

class MaxMatrixChainLengthTest
    : public ::testing::TestWithParam<ChainParameter> {
 protected:
  byte number_of_modules_ = std::get<0>(GetParam());
  byte number_of_columns_ =
      static_cast<byte>(number_of_modules_ * max_matrix::kColumnNumberOfModule);
  std::vector<byte> buffer_ =
      std::vector<byte>(max_matrix::BufferSize(number_of_modules_), 0xAA);
  Matrix matrix_{kDataInPin, kChipSelectPin, kClockPin, buffer_.data(),
                 number_of_modules_};

  void SetUp() override {
    mock::Reset(kDataInPin, kChipSelectPin, kClockPin, number_of_modules_);
    reports.clear();
    matrix_.Init();
    matrix_.SetShiftInterval(0);
    matrix_.SetMatrixOrientation(std::get<1>(GetParam()));
    matrix_.SetModuleType(std::get<2>(GetParam()));
  }

  /* as in MaxMatrixTest: the LEDs show the buffer, and every bit went out
   * within a frame */
  void TearDown() override {
    EXPECT_EQ(Display(), buffer_) << "display differs from the buffer";
    EXPECT_EQ(mock::errors, 0);
  }

  std::vector<std::string> Pattern() const {
    return Picture(::Pattern, number_of_columns_);
  }
};

std::string ChainName(::testing::TestParamInfo<ChainParameter> const& info) {
  return std::to_string(std::get<0>(info.param)) + "modules_matrix" +
         (std::get<1>(info.param) == MatrixOrientation::k180 ? "180" : "0") +
         "_" + ModuleTypeName(std::get<2>(info.param));
}

INSTANTIATE_TEST_SUITE_P(
    Lengths, MaxMatrixChainLengthTest,
    ::testing::Combine(::testing::ValuesIn(kChainLengths),
                       ::testing::ValuesIn(kMatrixOrientations),
                       ::testing::ValuesIn(kModuleTypes)),
    ChainName);

/* the matrix empties the buffer it is handed, and a frame carries an address
 * and a value for every module */
TEST_P(MaxMatrixChainLengthTest, EmptiesItsBufferAndFramesEveryModule) {
  EXPECT_EQ(matrix_.GetNumberOfModules(), number_of_modules_);
  EXPECT_EQ(matrix_.GetNumberOfColumns(), number_of_columns_);
  EXPECT_EQ(buffer_, std::vector<byte>(number_of_columns_, 0x00));
  EXPECT_EQ(mock::last_frame.size(), 2u * number_of_modules_);
}

TEST_P(MaxMatrixChainLengthTest, DotsRoundTrip) {
  DrawPattern(matrix_);
  EXPECT_EQ(ShownPicture(matrix_), Pattern());
  EXPECT_EQ(matrix_.GetDot(number_of_columns_, 0).GetError(),
            Error::kOutOfRange);
  matrix_.SetDot(number_of_columns_, 0, true);
  EXPECT_EQ(ShownPicture(matrix_), Pattern())
      << "a dot right of the panel drawn";
}

/* the mapping changes a pair of modules at a time, m and n - 1 - m; the picture
 * has to stay for every pair */
TEST_P(MaxMatrixChainLengthTest, PictureStaysWhenTheMappingChanges) {
  for (MatrixOrientation matrix_orientation : kMatrixOrientations) {
    for (ModuleType module_type : kModuleTypes) {
      matrix_.SetMatrixOrientation(std::get<1>(GetParam()));
      matrix_.SetModuleType(std::get<2>(GetParam()));
      matrix_.Clear();
      DrawPattern(matrix_);
      matrix_.SetMatrixOrientation(matrix_orientation);
      matrix_.SetModuleType(module_type);
      EXPECT_EQ(ShownPicture(matrix_), Pattern())
          << "to matrix " << static_cast<uint16_t>(matrix_orientation) << ", "
          << ModuleTypeName(module_type);
    }
  }
}

/* the shifts work column by column in place: a rotation round and back, and up
 * and down, give the picture again, and a shift fills the column it leaves */
TEST_P(MaxMatrixChainLengthTest, ShiftsMoveThePicture) {
  byte const columns = number_of_columns_;

  DrawPattern(matrix_);
  matrix_.ShiftLeft(ShiftFill::kRotate);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture(
                [columns](int16_t x, int16_t y) {
                  return ::Pattern(static_cast<int16_t>((x + 1) % columns), y);
                },
                columns));
  matrix_.ShiftRight(ShiftFill::kRotate);
  EXPECT_EQ(ShownPicture(matrix_), Pattern());
  matrix_.ShiftUp(ShiftFill::kRotate);
  matrix_.ShiftDown(ShiftFill::kRotate);
  EXPECT_EQ(ShownPicture(matrix_), Pattern());
  matrix_.ShiftRight();
  EXPECT_EQ(
      ShownPicture(matrix_),
      Picture([](int16_t x, int16_t y) { return x > 0 && ::Pattern(x - 1, y); },
              columns));
  EXPECT_EQ(matrix_.ShiftLeft(0x81), 0x00)
      << "the empty column the last shift brought";
  EXPECT_EQ(matrix_.GetColumn(columns - 1).GetValue(), 0x81);
}

/* a row of the whole panel takes one byte per module: an array longer than the
 * chain is taken up to the chain, one shorter is refused by GetRow() and drawn
 * as far as it reaches by SetRow() */
TEST_P(MaxMatrixChainLengthTest, RowsTakeAByteForEveryModule) {
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a row as a sketch has one
  byte const values[6] = {0x81, 0x42, 0x24, 0x18, 0xF0, 0x0F};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a row as a sketch has one
  byte read[6] = {0};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a row as a sketch has one
  byte read_one[1] = {0};
  // NOLINTNEXTLINE(modernize-avoid-c-arrays): T-20, a row as a sketch has one
  byte const one[1] = {0xFF};

  matrix_.SetRow(2, values);
  ASSERT_EQ(matrix_.GetRow(2, read).GetError(), Error::kOk);
  for (byte module = 0; module < number_of_modules_; module++) {
    EXPECT_EQ(read[module], values[module])
        << "module " << static_cast<uint16_t>(module);
    EXPECT_EQ(matrix_.GetRow(module, 2).GetValue(), values[module])
        << "module " << static_cast<uint16_t>(module);
  }
  EXPECT_EQ(matrix_.GetRow(2, read_one).GetError(),
            number_of_modules_ == 1 ? Error::kOk : Error::kOutOfRange);
  matrix_.SetRow(3, one);
  EXPECT_EQ(matrix_.GetRow(0, 3).GetValue(), 0xFF);
  if (number_of_modules_ > 1) {
    EXPECT_EQ(matrix_.GetRow(1, 3).GetValue(), 0x00)
        << "drawn beyond the array";
  }
}

/* a text shifts in from the right edge of this chain and stops; picture answers
 * with every column of it */
TEST_P(MaxMatrixChainLengthTest, TextShiftsAndPictureAnswersEveryColumn) {
  char const text[] = "Hi";
  Command command;
  TextPrint answer;
  uint16_t steps = 0;

  ASSERT_EQ(matrix_.SetTextWithShift(text).GetError(), Error::kOk);
  for (; steps < 1000 && matrix_.GetState() != State::kReady; steps++) {
    matrix_.ShiftTask();
  }
  EXPECT_LT(steps, 1000);
  EXPECT_NE(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }, number_of_columns_))
      << "nothing shifted in";
  for (char const c : std::string("picture\n")) {
    command.Feed(matrix_, c, answer);
  }
  EXPECT_EQ(answer.text.size(), 8 + 2u * number_of_columns_ + 2);
}

}  // namespace
