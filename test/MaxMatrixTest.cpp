// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* Host-side tests of MaxMatrix against the Arduino stand-in in mock/.
 *
 * Built twice by CMakeLists.txt, once with bit banging and once with
 * MAXMATRIX_USE_SPI=STD_ON, because the two transfer paths are separate code.
 * What this cannot show is anything about timing or the electrical side -
 * whether a real MAX7219 accepts the frames is only known on hardware. */
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
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

using max_matrix::Batch;
using max_matrix::Command;
using max_matrix::CommandState;
using max_matrix::Error;
using max_matrix::Font;
using max_matrix::Glyph;
using max_matrix::kDefaultFont;
using max_matrix::Matrix;
using max_matrix::MatrixOf;
using max_matrix::MatrixOrientation;
using max_matrix::ModuleType;
using max_matrix::Result;
using max_matrix::ShiftFill;
using max_matrix::State;
using max_matrix::Status;

static byte const kDataInPin = 11;
static byte const kChipSelectPin = 10;
static byte const kClockPin = 13;
/* the chain of the examples; the tests of other lengths make their own */
static byte const kNumberOfModules = 4;
static byte const kNumberOfColumns =
    kNumberOfModules * max_matrix::kColumnNumberOfModule;
/* a row of the whole panel, one byte per module */
typedef byte PanelRow[kNumberOfModules];

/* fonts in the format of MD_MAX72XX for the tests of SetFont(): the digits 0
 * and 1, 7 rows high, with a 2 of width 0 that the font lacks, in version 1 and
 * in version 2; a char of 8 columns; and an array that is no font */
static uint8_t const kDigitFont[] PROGMEM = {
    'F', 1,         '0',       '2', 7, 3, B00111110, B01000001, B00111110,  // 0
    2,   B01000010, B01111111,                                              // 1
    0,  // 2, not in the font
};
static uint8_t const kDigitFontVersion2[] PROGMEM = {
    'F', 2,         0,         '0',       0,         '1',
    7,   3,         B00111110, B01000001, B00111110,  // 0
    2,   B01000010, B01111111,                        // 1
};
static uint8_t const kWideFont[] PROGMEM = {
    'F',  1,    'W',  'W',  8,    8,    0x01,
    0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,  // W, a diagonal
};
static uint8_t const kNoFont[] PROGMEM = {'G', 1, 32, 126, 8};

/* every error by name, in the order of the enum */
static char const* const kErrorNames[] = {
    "Error::kOk",          "Error::kOutOfRange", "Error::kUnknownChar",
    "Error::kNoText",      "Error::kNotANumber", "Error::kUnknownCommand",
    "Error::kLineTooLong", "Error::kNotAFont"};
static uint8_t const kNumberOfErrors =
    sizeof(kErrorNames) / sizeof(kErrorNames[0]);

/* GoogleTest finds what prints and compares a type of the library by ADL, so
 * these lie in its namespace */
namespace max_matrix {

/* GoogleTest would print an error as its number */
void PrintTo(Error error, std::ostream* out) {
  if (static_cast<uint8_t>(error) < kNumberOfErrors)
    *out << kErrorNames[static_cast<uint8_t>(error)];
  else
    *out << "error " << static_cast<uint16_t>(error);
}

/* what Feed() returns, compared with a state as with an error: a line is done
 * only if it did not fail as well */
bool operator==(Result<CommandState> const& result, CommandState state) {
  return result.IsOk() && result.GetValue() == state;
}

void PrintTo(Result<CommandState> const& result, std::ostream* out) {
  if (!result.IsOk())
    PrintTo(result.GetError(), out);
  else if (result.GetValue() == CommandState::kDone)
    *out << "CommandState::kDone";
  else if (result.GetValue() == CommandState::kAnswered)
    *out << "CommandState::kAnswered";
  else
    *out << "CommandState::kPending";
}

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
static std::string Text(__FlashStringHelper const* text) {
  return reinterpret_cast<char const*>(text);
}

/* a whole line to a command of its own, and why it failed */
static Error FeedText(Matrix& matrix, std::string const& text) {
  Command command;
  TextPrint answer;
  Result<CommandState> result = CommandState::kPending;

  for (char c : text) result = command.Feed(matrix, c, answer);
  return result.GetError();
}

/* What the library reported, with MAXMATRIX_DEV_ERROR_DETECT on: the definition
 * here wins over the weak one of the library, which prints. With it off the
 * list stays empty, so that a test can expect a report in every variant. */
typedef std::pair<std::string, Error> Report;
static bool const kDevErrorDetect = (MAXMATRIX_DEV_ERROR_DETECT == STD_ON);
static std::vector<Report> reports;

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
void max_matrix::ReportError(__FlashStringHelper const* api, Error error) {
  reports.push_back(Report(Text(api), error));
}
#endif

/* GoogleTest prints a vector element by element, where a failed memcmp only
 * says that something differs */
static std::vector<byte> Bytes(byte const* data, size_t size) {
  return std::vector<byte>(data, data + size);
}

/* the library sends the pair of module 0 first, so it ends up in the chip at
 * the far end of the chain */
static Max7219 const& ChipOfModule(byte module) {
  return mock::chain.chips[mock::chain.chips.size() - 1 - module];
}

/* what the chain shows, laid out like matrix_buffer_: a column of a module is a
 * digit, a row a segment */
static std::vector<byte> Display() {
  std::vector<byte> columns;
  for (byte module = 0; module < mock::chain.chips.size(); module++) {
    for (byte column = 0; column < max_matrix::kColumnNumberOfModule;
         column++) {
      columns.push_back(ChipOfModule(module).Segments(column));
    }
  }
  return columns;
}

class MaxMatrixTest : public ::testing::Test {
 protected:
  MatrixOf<kNumberOfModules> matrix_{kDataInPin, kChipSelectPin, kClockPin};

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
    int32_t frames_before = mock::frames;
    matrix_.SetTextWithShift(text);
    for (shift_calls_ = 0;
         shift_calls_ < 1000 && matrix_.GetState() != State::kReady;
         shift_calls_++)
      matrix_.ShiftTask();
    return mock::frames - frames_before;
  }
};

/* a picture in flash, a column per byte, for DrawBitmap() */
static byte const kBitmap[] PROGMEM = {0x01, 0x82, 0xFF, 0x00, 0x3C};

static MatrixOrientation const kMatrixOrientations[] = {
    MatrixOrientation::k0, MatrixOrientation::k180};

/* all eight, in the order of the model's kMax7219Wirings, which
 * MaxMatrixModuleTest checks */
static ModuleType const kModuleTypes[] = {
    ModuleType::kDr0Cr0Rr0, ModuleType::kDr0Cr0Rr1, ModuleType::kDr0Cr1Rr0,
    ModuleType::kDr0Cr1Rr1, ModuleType::kDr1Cr0Rr0, ModuleType::kDr1Cr0Rr1,
    ModuleType::kDr1Cr1Rr0, ModuleType::kDr1Cr1Rr1};

/* the name of a module type as the model names its wiring, DR0CR0RR0 to
 * DR1CR1RR1 */
static std::string ModuleTypeName(ModuleType module_type) {
  return kMax7219Wirings[static_cast<uint8_t>(module_type)].name;
}

typedef std::tuple<MatrixOrientation, ModuleType> MappingPair;

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

static std::string MappingName(
    ::testing::TestParamInfo<MappingPair> const& info) {
  return std::string("matrix") +
         (std::get<0>(info.param) == MatrixOrientation::k180 ? "180" : "0") +
         "_" + ModuleTypeName(std::get<1>(info.param));
}

INSTANTIATE_TEST_SUITE_P(
    AllMappings, MaxMatrixMappingTest,
    ::testing::Combine(::testing::ValuesIn(kMatrixOrientations),
                       ::testing::ValuesIn(kModuleTypes)),
    MappingName);

/* the bit position used to run on across modules, so only module 0 got its row
 */
TEST_P(MaxMatrixMappingTest, RowRoundTrip) {
  PanelRow in, out;
  for (byte module = 0; module < kNumberOfModules; module++)
    in[module] = 0xA5 ^ (module * 0x3C);

  for (byte row = 0; row < max_matrix::kRowNumberOfModule; row++) {
    SCOPED_TRACE(::testing::Message() << "Row " << static_cast<uint16_t>(row));
    matrix_.SetRow(row, in);
    EXPECT_EQ(matrix_.GetRow(row, out).GetError(), Error::kOk);
    EXPECT_EQ(Bytes(out, sizeof(out)), Bytes(in, sizeof(in)));
  }
}

/* the display has to follow the buffer during the shift too, not only once the
 * text has left it */
TEST_P(MaxMatrixMappingTest, DisplayFollowsTheShift) {
  char const text[] = "Hi \xC4!";

  matrix_.SetTextWithShift(text);
  for (uint16_t step = 0; step < 1000 && matrix_.GetState() != State::kReady;
       step++) {
    matrix_.ShiftTask();
    ASSERT_EQ(Display(),
              Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns))
        << "after step " << step;
  }
}

/* Whatever a call draws, it sends one frame per changed column of a module at
 * most. Every dot and column used to be a frame of its own: SetText("Hi!") on
 * FC-16 modules sent 64, and each step of a shifting text nine. */
TEST_P(MaxMatrixMappingTest, NoCallSendsMoreThanEightFrames) {
  struct {
    char const* name;
    void (*draw)(Matrix&);
  } const calls[] = {
      {"SetText", [](Matrix& m) { m.SetText("Hi!"); }},
      {"SetChar off the top row", [](Matrix& m) { m.SetChar(9, 1, 'A'); }},
      {"SetRow",
       [](Matrix& m) {
         PanelRow row = {0xFF, 0x0F, 0xF0, 0x3C};
         m.SetRow(5, row);
       }},
      {"SetColumn", [](Matrix& m) { m.SetColumn(17, 0x5A); }},
      {"ShiftUp", [](Matrix& m) { m.ShiftUp(ShiftFill::kRotate); }},
      {"ShiftLeft a column", [](Matrix& m) { m.ShiftLeft(0x3C); }},
      {"clear", [](Matrix& m) { m.Clear(); }},
      {"DrawLine", [](Matrix& m) { m.DrawLine(0, 7, 31, 0, true); }},
      {"FillRect", [](Matrix& m) { m.FillRect(-1, -1, 34, 10, true); }},
      {"DrawCircle", [](Matrix& m) { m.DrawCircle(15, 3, 9, true); }},
      {"DrawBitmap",
       [](Matrix& m) { m.DrawBitmap(3, 1, kBitmap, sizeof(kBitmap)); }},
  };
  char const text[] = "Hi \xC4!";

  for (auto const& call : calls) {
    int32_t frames_before = mock::frames;
    call.draw(matrix_);
    EXPECT_LE(mock::frames - frames_before, max_matrix::kColumnNumberOfModule)
        << call.name;
  }
  matrix_.SetTextWithShift(text);
  for (uint16_t step = 0; step < 1000 && matrix_.GetState() != State::kReady;
       step++) {
    int32_t frames_before = mock::frames;
    matrix_.ShiftTask();
    ASSERT_LE(mock::frames - frames_before, max_matrix::kColumnNumberOfModule)
        << "shift step " << step;
  }
}

static bool Pattern(int16_t x, int16_t y) {
  return (x * x + 3 * y + x * y) % 3 == 0;
}

/* a picture as rows of '#' and '.', so that a failure prints both pictures */
template <typename DotType>
static std::vector<std::string> Picture(DotType dot,
                                        byte columns = kNumberOfColumns) {
  std::vector<std::string> rows;
  for (int16_t y = 0; y < max_matrix::kRowNumberOfModule; y++) {
    std::string row;
    for (int16_t x = 0; x < columns; x++) row += dot(x, y) ? '#' : '.';
    rows.push_back(row);
  }
  return rows;
}

/* the picture as the library reports it, through GetDot */
static std::vector<std::string> ShownPicture(Matrix& matrix) {
  return Picture(
      [&matrix](int16_t x, int16_t y) {
        return matrix.GetDot(x, y).GetValue();
      },
      matrix.GetNumberOfColumns());
}

static void DrawPattern(Matrix& matrix) {
  for (byte x = 0; x < matrix.GetNumberOfColumns(); x++) {
    for (byte y = 0; y < max_matrix::kRowNumberOfModule; y++)
      matrix.SetDot(x, y, Pattern(x, y));
  }
}

/* rows and columns are two views of one picture: in orientation 180 SetColumn
 * turned the whole panel while SetRow turned each module */
TEST_P(MaxMatrixMappingTest, RowsAndColumnsAgree) {
  PanelRow row;
  for (byte module = 0; module < kNumberOfModules; module++)
    row[module] = 0x81 ^ (module * 0x16);

  matrix_.SetRow(2, row);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&row](int16_t x, int16_t y) {
              return y == 2 &&
                     bitRead(row[x / max_matrix::kColumnNumberOfModule],
                             7 - x % max_matrix::kColumnNumberOfModule);
            }));

  matrix_.Clear();
  matrix_.SetColumn(5, 0xA3);
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return x == 5 && bitRead(0xA3, y);
            }));
  for (byte y = 0; y < max_matrix::kRowNumberOfModule; y++) {
    Result<byte> value = matrix_.GetRow(0, y);
    EXPECT_EQ(value.GetError(), Error::kOk);
    EXPECT_EQ(value.GetValue(), bitRead(0xA3, y) ? 1 << (7 - 5) : 0)
        << "Row " << static_cast<uint16_t>(y);
  }
}

/* every shift moves the whole picture by one dot as the viewer sees it, across
 * the modules, and fills the edge it leaves as it is told: with nothing, with
 * what fell out at the other edge, or with what the edge held */
TEST_P(MaxMatrixMappingTest, ShiftsMoveThePicture) {
  struct {
    char const* name;
    void (*shift)(Matrix&);
    int16_t source_x, source_y;
    ShiftFill fill;
  } const cases[] = {
      {"ShiftLeft", [](Matrix& m) { m.ShiftLeft(); }, 1, 0, ShiftFill::kZero},
      {"ShiftLeft rotating", [](Matrix& m) { m.ShiftLeft(ShiftFill::kRotate); },
       1, 0, ShiftFill::kRotate},
      {"ShiftLeft keeping", [](Matrix& m) { m.ShiftLeft(ShiftFill::kKeep); }, 1,
       0, ShiftFill::kKeep},
      {"ShiftRight", [](Matrix& m) { m.ShiftRight(); }, -1, 0,
       ShiftFill::kZero},
      {"ShiftRight rotating",
       [](Matrix& m) { m.ShiftRight(ShiftFill::kRotate); }, -1, 0,
       ShiftFill::kRotate},
      {"ShiftRight keeping", [](Matrix& m) { m.ShiftRight(ShiftFill::kKeep); },
       -1, 0, ShiftFill::kKeep},
      {"ShiftUp", [](Matrix& m) { m.ShiftUp(); }, 0, 1, ShiftFill::kZero},
      {"ShiftUp rotating", [](Matrix& m) { m.ShiftUp(ShiftFill::kRotate); }, 0,
       1, ShiftFill::kRotate},
      {"ShiftUp keeping", [](Matrix& m) { m.ShiftUp(ShiftFill::kKeep); }, 0, 1,
       ShiftFill::kKeep},
      {"ShiftDown", [](Matrix& m) { m.ShiftDown(); }, 0, -1, ShiftFill::kZero},
      {"ShiftDown rotating", [](Matrix& m) { m.ShiftDown(ShiftFill::kRotate); },
       0, -1, ShiftFill::kRotate},
      {"ShiftDown keeping", [](Matrix& m) { m.ShiftDown(ShiftFill::kKeep); }, 0,
       -1, ShiftFill::kKeep},
  };
  int16_t const width = kNumberOfColumns,
                height = max_matrix::kRowNumberOfModule;

  for (auto const& test_case : cases) {
    matrix_.Clear();
    DrawPattern(matrix_);
    test_case.shift(matrix_);
    EXPECT_EQ(
        ShownPicture(matrix_), Picture([&test_case](int16_t x, int16_t y) {
          int16_t source_x = x + test_case.source_x,
                  source_y = y + test_case.source_y;
          bool const inside = source_x >= 0 && source_x < width &&
                              source_y >= 0 && source_y < height;
          if (inside) return Pattern(source_x, source_y);
          if (test_case.fill == ShiftFill::kRotate)
            return Pattern(static_cast<int16_t>((source_x + width) % width),
                           static_cast<int16_t>((source_y + height) % height));
          if (test_case.fill == ShiftFill::kKeep) return Pattern(x, y);
          return false;
        }))
        << test_case.name;
  }
}

/* a column given to ShiftLeft() or ShiftRight() comes in at the edge the shift
 * leaves, and the one that falls out at the other edge comes back, both bit 0
 * at the top; a literal is a column, not a fill */
TEST_P(MaxMatrixMappingTest, ShiftTakesAColumnInAndGivesOneOut) {
  DrawPattern(matrix_);
  std::vector<std::string> const before = ShownPicture(matrix_);
  byte const leftmost = matrix_.GetColumn(0).GetValue();

  EXPECT_EQ(matrix_.ShiftLeft(0xA5), leftmost);
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return x == kNumberOfColumns - 1 ? bitRead(0xA5, y) != 0
                                               : Pattern(x + 1, y);
            }))
      << "left";

  EXPECT_EQ(matrix_.ShiftRight(leftmost), 0xA5);
  EXPECT_EQ(ShownPicture(matrix_), before) << "right, back to where it was";
}

/* columns fed one by one make a band longer than the panel, and come out at the
 * other edge in the order they went in: what a sketch passes on to a second
 * matrix */
TEST_F(MaxMatrixTest, ShiftedColumnsMakeABand) {
  std::vector<byte> band;
  band.reserve(size_t{3} * kNumberOfColumns);
  for (uint16_t index = 0; index < 3 * kNumberOfColumns; index++)
    band.push_back(static_cast<byte>(index * 37 + 11));
  std::vector<byte> out;
  out.reserve(band.size());

  for (byte column : band) out.push_back(matrix_.ShiftLeft(column));
  for (byte column = 0; column < kNumberOfColumns; column++) {
    EXPECT_EQ(matrix_.GetColumn(column).GetValue(),
              band[band.size() - kNumberOfColumns + column])
        << "column " << static_cast<uint16_t>(column);
  }
  EXPECT_EQ(std::vector<byte>(out.begin(), out.begin() + kNumberOfColumns),
            std::vector<byte>(kNumberOfColumns, 0x00))
      << "the empty panel first";
  EXPECT_EQ(std::vector<byte>(out.begin() + kNumberOfColumns, out.end()),
            std::vector<byte>(band.begin(), band.end() - kNumberOfColumns))
      << "then the band, in its order";
}

/* changing the orientation or the module type rewrites the buffer along, so the
 * viewer still sees the same picture; changing the module orientation to or
 * from 180 turned each module while SetColumn had turned the whole panel */
TEST_P(MaxMatrixMappingTest, PictureStaysWhenTheMappingChanges) {
  for (MatrixOrientation matrix_orientation : kMatrixOrientations) {
    for (ModuleType module_type : kModuleTypes) {
      matrix_.SetMatrixOrientation(std::get<0>(GetParam()));
      matrix_.SetModuleType(std::get<1>(GetParam()));
      matrix_.Clear();
      DrawPattern(matrix_);
      matrix_.SetMatrixOrientation(matrix_orientation);
      matrix_.SetModuleType(module_type);
      EXPECT_EQ(ShownPicture(matrix_), Picture(Pattern))
          << "to matrix " << static_cast<uint16_t>(matrix_orientation) << ", "
          << ModuleTypeName(module_type);
    }
  }
}

TEST_P(MaxMatrixMappingTest, DotRoundTrip) {
  for (byte column = 0; column < kNumberOfColumns; column++) {
    for (byte row = 0; row < max_matrix::kRowNumberOfModule; row++) {
      SCOPED_TRACE(::testing::Message()
                   << "Column " << static_cast<uint16_t>(column) << ", Row "
                   << static_cast<uint16_t>(row));
      bool expected = (column * 3 + row) & 1;
      matrix_.SetDot(column, row, expected);
      Result<bool> value = matrix_.GetDot(column, row);
      EXPECT_EQ(value.GetError(), Error::kOk);
      EXPECT_EQ(value.GetValue(), expected);
    }
  }
}

/* Every module type shows the picture upright on the wiring of the model it is
 * named after, the matrix orientation 180 on a board mounted upside down, as
 * the README tells a user. Four of the eight are mirror images of the others,
 * which the module orientation of 0 to 270 degrees did not reach (OIL item 9);
 * the named four have to be the wirings MD_MAX72XX gives those names. */
typedef std::tuple<ModuleType, bool> Module;

class MaxMatrixModuleTest : public MaxMatrixTest,
                            public ::testing::WithParamInterface<Module> {};

static std::string ModuleName(::testing::TestParamInfo<Module> const& info) {
  return ModuleTypeName(std::get<0>(info.param)) +
         (std::get<1>(info.param) ? "_upside_down" : "");
}

INSTANTIATE_TEST_SUITE_P(Modules, MaxMatrixModuleTest,
                         ::testing::Combine(::testing::ValuesIn(kModuleTypes),
                                            ::testing::Bool()),
                         ModuleName);

TEST_F(MaxMatrixTest, NamedModuleTypesAreTheirWirings) {
  EXPECT_EQ(ModuleTypeName(ModuleType::kFc16), kMaX7219WiringFC16.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kGeneric), kMaX7219WiringGeneric.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kParola), kMaX7219WiringParola.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kIcstation),
            kMaX7219WiringIcstation.name);
}

/* a dot set at X, Y is lit at X, Y of the panel; the pattern has no symmetry a
 * wrong mapping could hide behind */
TEST_P(MaxMatrixModuleTest, DotsAppearWhereTheyAreSet) {
  ModuleType const module_type = std::get<0>(GetParam());
  bool const upside_down = std::get<1>(GetParam());
  Max7219Wiring const& wiring =
      kMax7219Wirings[static_cast<uint8_t>(module_type)];
  auto pattern = [](uint16_t x, uint8_t y) {
    return (x * x + 3 * y + x * y) % 3 == 0;
  };

  matrix_.SetMatrixOrientation(upside_down ? MatrixOrientation::k180
                                           : MatrixOrientation::k0);
  matrix_.SetModuleType(module_type);
  for (byte x = 0; x < kNumberOfColumns; x++) {
    for (byte y = 0; y < max_matrix::kRowNumberOfModule; y++)
      matrix_.SetDot(x, y, pattern(x, y));
  }
  for (uint16_t x = 0; x < kNumberOfColumns; x++) {
    for (uint8_t y = 0; y < max_matrix::kRowNumberOfModule; y++) {
      EXPECT_EQ(mock::chain.IsLit(wiring, x, y, upside_down), pattern(x, y))
          << "X " << x << ", Y " << y;
    }
  }
}

TEST_F(MaxMatrixTest, InitConfiguresEveryChip) {
  for (byte module = 0; module < kNumberOfModules; module++) {
    SCOPED_TRACE(::testing::Message()
                 << "Module " << static_cast<uint16_t>(module));
    Max7219 const& chip = ChipOfModule(module);
    EXPECT_EQ(chip.shutdown, max_matrix::max7219::kShutdownModeNormalOperation);
    EXPECT_EQ(chip.display_test,
              max_matrix::max7219::kDisplayTestNormalOperation);
    EXPECT_EQ(chip.scan_limit, max_matrix::max7219::kScanLimitDisplayDigit0To7);
    EXPECT_EQ(chip.decode_mode, max_matrix::max7219::kDecodeModeNoDecode);
    EXPECT_EQ(chip.intensity, MAXMATRIX_INTENSITY_INIT_VALUE);
  }
}

TEST_F(MaxMatrixTest, IntensityReachesEveryChip) {
  EXPECT_EQ(
      matrix_.SetIntensity(max_matrix::max7219::kIntensityMaxValue).GetError(),
      Error::kOk);
  for (byte module = 0; module < kNumberOfModules; module++) {
    EXPECT_EQ(ChipOfModule(module).intensity,
              max_matrix::max7219::kIntensityMaxValue)
        << "Module " << static_cast<uint16_t>(module);
  }
}

TEST_F(MaxMatrixTest, OutOfRangeChangesNothing) {
  std::vector<byte> before =
      Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  int32_t frames_before = mock::frames;

  EXPECT_EQ(matrix_.SetIntensity(max_matrix::max7219::kIntensityMaxValue + 1)
                .GetError(),
            Error::kOutOfRange);
  EXPECT_EQ(Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns),
            before);
  EXPECT_EQ(mock::frames, frames_before);
}

/* every way a call can fail, with the reason it gives: with stdReturnType a
 * test stayed green when another check than the one meant failed. A failing
 * call changes nothing, draws nothing and starts no shift. "999x" is no number
 * rather than too large, although the digits run past the maximum before the
 * letter comes. With MAXMATRIX_DEV_ERROR_DETECT, the function of MaxMatrix that
 * failed reports it once, under its own name. The command reports nothing of
 * its own, as a wrong line is no mistake of the sketch and its answer says what
 * was wrong; only a line that fails in a function of Matrix, text= with a
 * char the font lacks, is reported by that function. */
TEST_F(MaxMatrixTest, EveryErrorSaysWhy) {
  struct {
    char const* name;
    Error (*call)(Matrix&);
    Error expected;
    char const* api;
  } const cases[] = {
      {"GetColumn left", [](Matrix& m) { return m.GetColumn(-1).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn right",
       [](Matrix& m) { return m.GetColumn(kNumberOfColumns).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn no module",
       [](Matrix& m) { return m.GetColumn(kNumberOfModules, 0).GetError(); },
       Error::kOutOfRange, "GetColumn"},
      {"GetColumn off its module",
       [](Matrix& m) {
         return m.GetColumn(0, max_matrix::kColumnNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetColumn"},
      {"GetRow above",
       [](Matrix& m) {
         PanelRow row;
         return m.GetRow(-1, row).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow below",
       [](Matrix& m) {
         PanelRow row;
         return m.GetRow(max_matrix::kRowNumberOfModule, row).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetRow no module",
       [](Matrix& m) { return m.GetRow(kNumberOfModules, 0).GetError(); },
       Error::kOutOfRange, "GetRow"},
      {"GetRow off its module",
       [](Matrix& m) {
         return m.GetRow(0, max_matrix::kRowNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetRow"},
      {"GetDot left", [](Matrix& m) { return m.GetDot(-1, 0).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot right",
       [](Matrix& m) { return m.GetDot(kNumberOfColumns, 0).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot above", [](Matrix& m) { return m.GetDot(0, -1).GetError(); },
       Error::kOutOfRange, "GetDot"},
      {"GetDot below",
       [](Matrix& m) {
         return m.GetDot(0, max_matrix::kRowNumberOfModule).GetError();
       },
       Error::kOutOfRange, "GetDot"},
      {"SetFont", [](Matrix& m) { return m.SetFont(kNoFont).GetError(); },
       Error::kNotAFont, "SetFont"},
      {"SetIntensity",
       [](Matrix& m) {
         return m.SetIntensity(max_matrix::max7219::kIntensityMaxValue + 1)
             .GetError();
       },
       Error::kOutOfRange, "SetIntensity"},
      {"SetChar", [](Matrix& m) { return m.SetChar(0, 0, '\x01').GetError(); },
       Error::kUnknownChar, "SetChar"},
      {"SetText unknown char",
       [](Matrix& m) { return m.SetText("\x01").GetError(); },
       Error::kUnknownChar, "SetText"},
      {"SetText NULL", [](Matrix& m) { return m.SetText(NULL).GetError(); },
       Error::kNoText, "SetText"},
      {"SetCharWithShift",
       [](Matrix& m) { return m.SetCharWithShift('\x01').GetError(); },
       Error::kUnknownChar, "SetCharWithShift"},
      {"SetTextWithShift NULL",
       [](Matrix& m) { return m.SetTextWithShift(NULL).GetError(); },
       Error::kNoText, "SetTextWithShift"},
      {"SetTextWithShift NULL repeating",
       [](Matrix& m) { return m.SetTextWithShift(NULL, true).GetError(); },
       Error::kNoText, "SetTextWithShift"},
      {"number empty",
       [](Matrix&) { return Command::ConvertToNumber("", 255).GetError(); },
       Error::kNotANumber, NULL},
      {"number with a letter",
       [](Matrix&) { return Command::ConvertToNumber("12a", 255).GetError(); },
       Error::kNotANumber, NULL},
      {"number too large",
       [](Matrix&) { return Command::ConvertToNumber("256", 255).GetError(); },
       Error::kOutOfRange, NULL},
      {"number too large, then a letter",
       [](Matrix&) { return Command::ConvertToNumber("999x", 255).GetError(); },
       Error::kNotANumber, NULL},
      {"unknown command", [](Matrix& m) { return FeedText(m, "blink\n"); },
       Error::kUnknownCommand, NULL},
      {"line too long",
       [](Matrix& m) {
         return FeedText(
             m,
             "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");
       },
       Error::kLineTooLong, NULL},
  };

  for (auto const& test_case : cases) {
    matrix_.Clear();
    int32_t frames_before = mock::frames;
    byte intensity_before = ChipOfModule(0).intensity;
    reports.clear();
    EXPECT_EQ(test_case.call(matrix_), test_case.expected) << test_case.name;
    EXPECT_EQ(ShownPicture(matrix_),
              Picture([](int16_t, int16_t) { return false; }))
        << test_case.name;
    EXPECT_EQ(mock::frames, frames_before) << test_case.name;
    EXPECT_EQ(ChipOfModule(0).intensity, intensity_before) << test_case.name;
    EXPECT_EQ(matrix_.GetState(), State::kReady) << test_case.name;
    std::vector<Report> expected;
    if (kDevErrorDetect && test_case.api != NULL)
      expected.push_back(Report(test_case.api, test_case.expected));
    EXPECT_EQ(reports, expected) << test_case.name;
  }
}

/* every error has its text, as SerialControl answers with it, and a byte off
 * the enum gets one as well; the texts lie in a table of their own, so one
 * moved by a place would give every error after it the text of the next */
TEST(MaxMatrixErrorTest, EveryErrorHasItsText) {
  char const* const texts[] = {
      "ok",           "out of range",    "unknown char",  "no text",
      "not a number", "unknown command", "line too long", "not a font"};
  ASSERT_EQ(sizeof(texts) / sizeof(texts[0]), kNumberOfErrors);

  for (uint8_t error = 0; error < kNumberOfErrors; error++) {
    EXPECT_EQ(Text(Status(static_cast<Error>(error)).GetErrorText()),
              texts[error])
        << kErrorNames[error];
  }
  EXPECT_EQ(Text(Result<byte>(Error::kUnknownChar).GetErrorText()),
            "unknown char");
  /* a new error that kErrorNames lacks has a text of its own here, and fails */
  EXPECT_EQ(Text(Status(static_cast<Error>(kNumberOfErrors)).GetErrorText()),
            "unknown error");
}

#if (MAXMATRIX_DEV_ERROR_DETECT == STD_ON)
/* the default report prints what the tests record instead, so the printing is a
 * function of its own and tested here */
TEST(MaxMatrixErrorTest, PrintedReportNamesFunctionAndError) {
  mock::serial_output.clear();
  max_matrix::PrintError(F("SetText"), Error::kUnknownChar);
  EXPECT_EQ(mock::serial_output, "MaxMatrix SetText: unknown char\r\n");
}
#endif

/* drawing off the panel leaves the panel as it was and sends nothing; column 8
 * of module 0 used to be checked against the panel alone, which would have
 * drawn it on module 1 */
TEST_F(MaxMatrixTest, DrawingOffThePanelIsLeftOut) {
  PanelRow row = {0xFF, 0xFF, 0xFF, 0xFF};
  struct {
    char const* name;
    void (*draw)(Matrix&, PanelRow const&);
  } const cases[] = {
      {"SetDot left",
       [](Matrix& m, PanelRow const&) { m.SetDot(-1, 0, true); }},
      {"SetDot right",
       [](Matrix& m, PanelRow const&) { m.SetDot(kNumberOfColumns, 0, true); }},
      {"SetDot above",
       [](Matrix& m, PanelRow const&) { m.SetDot(0, -1, true); }},
      {"SetDot below",
       [](Matrix& m, PanelRow const&) {
         m.SetDot(0, max_matrix::kRowNumberOfModule, true);
       }},
      {"SetColumn left",
       [](Matrix& m, PanelRow const&) { m.SetColumn(-1, 0xFF); }},
      {"SetColumn right",
       [](Matrix& m, PanelRow const&) { m.SetColumn(kNumberOfColumns, 0xFF); }},
      {"SetColumn no module",
       [](Matrix& m, PanelRow const&) {
         m.SetColumn(kNumberOfModules, 0, 0xFF);
       }},
      {"SetColumn off its module",
       [](Matrix& m, PanelRow const&) {
         m.SetColumn(0, max_matrix::kColumnNumberOfModule, 0xFF);
       }},
      {"SetRow above", [](Matrix& m, PanelRow const& r) { m.SetRow(-1, r); }},
      {"SetRow below",
       [](Matrix& m, PanelRow const& r) {
         m.SetRow(max_matrix::kRowNumberOfModule, r);
       }},
      {"SetRow no module",
       [](Matrix& m, PanelRow const&) { m.SetRow(kNumberOfModules, 0, 0xFF); }},
      {"SetRow off its module",
       [](Matrix& m, PanelRow const&) {
         m.SetRow(0, max_matrix::kRowNumberOfModule, 0xFF);
       }},
  };

  for (auto const& test_case : cases) {
    matrix_.Clear();
    int32_t frames_before = mock::frames;
    test_case.draw(matrix_, row);
    EXPECT_EQ(ShownPicture(matrix_),
              Picture([](int16_t, int16_t) { return false; }))
        << test_case.name;
    EXPECT_EQ(mock::frames, frames_before) << test_case.name;
    EXPECT_TRUE(reports.empty()) << test_case.name << " reported an error";
  }
}

/* a char partly off the panel shows the part on it, through either path of
 * SetSprite(): whole columns for a char at the top, single dots for one moved
 * down */
TEST_P(MaxMatrixMappingTest, CharPartlyOffThePanelShowsThePartOnIt) {
  Result<Glyph> const glyph = Font::GetGlyph(kDefaultFont, 'A');
  ASSERT_EQ(glyph.GetError(), Error::kOk);
  int16_t const width = glyph.GetValue().width,
                height = glyph.GetValue().height;
  uint8_t const* const columns = glyph.GetValue().columns;
  struct {
    int16_t x, y;
  } const places[] = {
      {-2, 0}, {kNumberOfColumns - 2, 0}, {-2, -3}, {kNumberOfColumns - 2, 5}};

  for (auto const& place : places) {
    matrix_.Clear();
    ASSERT_EQ(matrix_.SetChar(place.x, place.y, 'A').GetError(), Error::kOk);
    EXPECT_EQ(ShownPicture(matrix_),
              Picture([columns, &place, width, height](int16_t x, int16_t y) {
                int16_t sprite_x = x - place.x, sprite_y = y - place.y;
                return sprite_x >= 0 && sprite_x < width && sprite_y >= 0 &&
                       sprite_y < height &&
                       bitRead(columns[sprite_x], sprite_y);
              }))
        << "X " << place.x << ", Y " << place.y;
  }
}

/* a changed column of a module goes out in one frame with the same column of
 * every other module */
TEST_F(MaxMatrixTest, ChangedColumnIsSentForEveryModule) {
  byte const module = kNumberOfModules - 2;
  int32_t frames_before = mock::frames;

  matrix_.panel_.wired_.StoreBit(module * max_matrix::kColumnNumberOfModule + 3,
                                 1, true);
  matrix_.Update();
  EXPECT_EQ(mock::frames - frames_before, 1);
  ASSERT_EQ(mock::last_frame.size(), 2u * kNumberOfModules);
  for (byte i = 0; i < kNumberOfModules; i++) {
    SCOPED_TRACE(::testing::Message() << "Module " << static_cast<uint16_t>(i));
    EXPECT_EQ(mock::last_frame[size_t{2} * i],
              max_matrix::max7219::kDigit3Address);
    EXPECT_EQ(mock::last_frame[size_t{2} * i + 1], i == module ? 0x02 : 0x00);
  }
}

/* Clear() has to reach LEDs the buffer knows nothing of: after power-up the
 * registers hold anything while the buffer is empty, so a clear that sent only
 * what changed in the buffer would send nothing */
TEST_F(MaxMatrixTest, InitClearsWhatThePowerUpLeft) {
  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  for (Max7219& chip : mock::chain.chips) {
    for (byte digit = 0; digit < Max7219::kNumberOfDigits; digit++)
      chip.digit[digit] = 0xA5;
  }
  matrix_.Init();
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));
}

TEST_F(MaxMatrixTest, DrawingWhatIsShownSendsNothing) {
  matrix_.SetText("Hi!");
  int32_t frames_before = mock::frames;

  matrix_.SetText("Hi!");
  matrix_.SetDot(0, 0, matrix_.panel_.wired_.GetColumn(0) & 1);
  EXPECT_EQ(mock::frames, frames_before);
}

/* the same drawing in both modes: a frame for every call that changed
 * something, or none until Update(), which sends each changed column of a
 * module once */
TEST_F(MaxMatrixTest, DrawingInBothModes) {
  std::vector<byte> drawn;
  int32_t frames_before = mock::frames;

  for (byte column = 0; column < kNumberOfColumns; column++)
    matrix_.SetDot(
        column, static_cast<int16_t>(column % max_matrix::kRowNumberOfModule),
        true);
  EXPECT_EQ(mock::frames - frames_before, kNumberOfColumns);
  drawn = Display();

  matrix_.Clear();
  matrix_.SetAutoUpdate(false);
  frames_before = mock::frames;
  for (byte column = 0; column < kNumberOfColumns; column++)
    matrix_.SetDot(
        column, static_cast<int16_t>(column % max_matrix::kRowNumberOfModule),
        true);
  EXPECT_EQ(mock::frames, frames_before);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0))
      << "shown before Update()";

  matrix_.Update();
  EXPECT_EQ(mock::frames - frames_before, max_matrix::kColumnNumberOfModule);
  EXPECT_EQ(Display(), drawn);
}

TEST_F(MaxMatrixTest, AutoUpdateOnSendsWhatWasDrawn) {
  matrix_.SetAutoUpdate(false);
  matrix_.SetText("Hi!");
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));

  matrix_.SetAutoUpdate(true);
  EXPECT_NE(Display(), std::vector<byte>(kNumberOfColumns, 0));
}

TEST_F(MaxMatrixTest, BatchSendsAtTheEndOfTheBlock) {
  int32_t frames_before = mock::frames;

  {
    Batch batch(matrix_);
    for (byte column = 0; column < kNumberOfColumns; column++)
      matrix_.SetDot(
          column, static_cast<int16_t>(column % max_matrix::kRowNumberOfModule),
          true);
    EXPECT_FALSE(matrix_.GetAutoUpdate());
    EXPECT_EQ(mock::frames, frames_before);
    EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0))
        << "shown inside the block";
  }
  EXPECT_TRUE(matrix_.GetAutoUpdate());
  EXPECT_EQ(mock::frames - frames_before, max_matrix::kColumnNumberOfModule);
}

/* the inner batch puts back the state it found, which the outer one had turned
 * off */
TEST_F(MaxMatrixTest, NestedBatchesSendAtTheOutermost) {
  int32_t frames_before = mock::frames;

  {
    Batch outer(matrix_);
    {
      Batch inner(matrix_);
      matrix_.SetDot(1, 1, true);
    }
    EXPECT_EQ(mock::frames, frames_before)
        << "sent at the end of the inner block";
    matrix_.SetDot(2, 2, true);
  }
  EXPECT_GT(mock::frames, frames_before);
}

/* a batch inside a sketch that sends with Update() itself leaves it that way */
TEST_F(MaxMatrixTest, BatchKeepsAutoUpdateOff) {
  matrix_.SetAutoUpdate(false);
  int32_t frames_before = mock::frames;

  {
    Batch batch(matrix_);
    matrix_.SetDot(1, 1, true);
  }
  EXPECT_FALSE(matrix_.GetAutoUpdate());
  EXPECT_EQ(mock::frames, frames_before);
  matrix_.Update();
}

TEST_F(MaxMatrixTest, ShiftIntervalStartsAtTheConfiguredValue) {
  MatrixOf<kNumberOfModules> fresh(kDataInPin, kChipSelectPin, kClockPin);

  EXPECT_EQ(fresh.GetShiftInterval(), MAXMATRIX_SHIFT_INTERVAL_INIT_VALUE);
}

/* The tests below count the steps of a shifting char in sprite_shift_counter_,
 * which CharShiftTask() moves on by one per step, and hold the clock, so that a
 * step 50 ms on does not depend on how fast the host is. */

/* the first step comes at once, every further one an interval after the step
 * before */
TEST_F(MaxMatrixTest, ShiftWaitsForTheInterval) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};

  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1)
      << "no first step at once";
  mock::SetMillis(1049);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1)
      << "a step before the interval had passed";
  mock::SetMillis(1050);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
}

/* a call after loop() was held up takes one step, not the ones it missed, and
 * the interval counts from there */
TEST_F(MaxMatrixTest, LateShiftDoesNotCatchUp) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};

  matrix_.ShiftTask();
  mock::SetMillis(1180);
  matrix_.ShiftTask();
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
  mock::SetMillis(1229);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 2);
  mock::SetMillis(1230);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 3);
}

TEST_F(MaxMatrixTest, ShiftIntervalZeroStepsOnEveryCall) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(0);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};

  for (uint8_t call = 0; call < 3; call++) matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 3);
}

/* a new char or text starts at once, however recently the one before took a
 * step */
TEST_F(MaxMatrixTest, NewShiftStartsAtOnce) {
  mock::SetMillis(1000);
  matrix_.SetShiftInterval(50);
  ASSERT_EQ(matrix_.SetCharWithShift('#').GetError(), Error::kOk);
  matrix_.ShiftTask();

  mock::SetMillis(1010);
  ASSERT_EQ(matrix_.SetCharWithShift('+').GetError(), Error::kOk);
  uint16_t const start{matrix_.text_.sprite_shift_counter_};
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.text_.sprite_shift_counter_, start + 1);
}

/* chip select was not an output in SPI mode, and the SPI transaction was never
 * ended */
TEST_F(MaxMatrixTest, TransfersAreFramed) {
  char const text[] = "Hello";

  EXPECT_EQ(mock::pin_mode[kChipSelectPin], OUTPUT);
  matrix_.SetText("Hi!");
  matrix_.SetModuleType(ModuleType::kIcstation);
  ShiftWholeText(text);

  EXPECT_GT(mock::frames, 0);
  EXPECT_EQ(mock::errors, 0);
  EXPECT_EQ(mock::pin_level[kChipSelectPin], HIGH);
  EXPECT_FALSE(mock::transaction_open);
  EXPECT_EQ(mock::UsesSpi(), MAXMATRIX_USE_SPI == STD_ON);
}

/* SetTextWithShift() took a char*, though the shift only reads the text, so
 * that a sketch could not make its text const */
TEST_F(MaxMatrixTest, TextShiftFinishes) {
  char const text[] = "Hello World!";

  ShiftWholeText(text);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* A text that repeats goes on with its first char right after its last (OIL
 * item 31): once the picture before it has left the panel, every picture comes
 * again one pass of the text later, with no column between its end and its
 * start, and the shift does not end. A pass is what the text takes when it does
 * not repeat, less the last step, which only shifts the panel once more. */
TEST_F(MaxMatrixTest, RepeatingTextStartsOver) {
  char const text[] = "Hi \xC4\x01 ";
  std::vector<std::vector<std::string>> pictures;

  ShiftWholeText(text);
  uint16_t const pass = shift_calls_ - 1;
  ASSERT_GT(pass, 0);

  matrix_.Clear();
  matrix_.SetDot(0, 0, true);
  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  for (uint16_t step = 0; step < kNumberOfColumns + 3 * pass; step++) {
    matrix_.ShiftTask();
    pictures.push_back(ShownPicture(matrix_));
  }
  EXPECT_EQ(matrix_.GetState(), State::kStringShift);
  EXPECT_NE(pictures.back(), Picture([](int16_t, int16_t) { return false; }));
  for (uint16_t step = kNumberOfColumns; step + pass < pictures.size();
       step++) {
    ASSERT_EQ(pictures[step + pass], pictures[step])
        << "step " << step << " and one pass later";
  }
}

/* a text that repeats but has no char of the font ends at once, rather than
 * reading itself over and over in one call */
TEST_F(MaxMatrixTest, RepeatingTextWithoutAKnownCharEnds) {
  char const text[] = "\x01\x02";

  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  matrix_.ShiftTask();
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* StopShift() ends a text that would repeat for ever, and leaves the picture as
 * it stands */
TEST_F(MaxMatrixTest, StopShiftHoldsThePicture) {
  char const text[] = "Hello ";

  ASSERT_EQ(matrix_.SetTextWithShift(text, true).GetError(), Error::kOk);
  for (byte step = 0; step < 20; step++) matrix_.ShiftTask();
  std::vector<std::string> const stopped = ShownPicture(matrix_);

  matrix_.StopShift();
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  for (byte step = 0; step < 20; step++) matrix_.ShiftTask();
  EXPECT_EQ(ShownPicture(matrix_), stopped);
  matrix_.StopShift();
  EXPECT_EQ(matrix_.GetState(), State::kReady) << "stopped twice";
}

/* the Arduino IDE saves sketches as UTF-8, so an umlaut arrives as two bytes */
TEST_F(MaxMatrixTest, UmlautsFromUtf8AndLatin1Match) {
  std::vector<byte> utf8, latin1;

  matrix_.Clear();
  EXPECT_EQ(matrix_.SetText("Äöü!").GetError(), Error::kOk);
  utf8 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  matrix_.Clear();
  EXPECT_EQ(matrix_.SetText("\xC4\xF6\xFC!").GetError(), Error::kOk);
  latin1 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  EXPECT_EQ(utf8, latin1);

  char shift_utf8[] = "Grüße Ä";
  char shift_latin1[] =
      "Gr\xFC\xDF"
      "e \xC4";
  ShiftWholeText(shift_utf8);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
  utf8 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  ShiftWholeText(shift_latin1);
  latin1 = Bytes(matrix_.panel_.wired_.matrix_buffer_, kNumberOfColumns);
  EXPECT_EQ(utf8, latin1);
}

/* the chars beyond ASCII lie at their latin-1 codes in the default font, each
 * its own */
TEST_F(MaxMatrixTest, EveryCharBeyondAsciiHasItsOwnGlyph) {
  char const* chars[] = {"Ä", "Ö", "Ü", "ä", "ö", "ü", "ß", "°"};
  byte const codes[] = {0xC4, 0xD6, 0xDC, 0xE4, 0xF6, 0xFC, 0xDF, 0xB0};
  char c;

  for (byte i = 0; i < 8; i++) {
    SCOPED_TRACE(chars[i]);
    EXPECT_EQ(Font::DecodeChar(chars[i], &c), 2);
    EXPECT_EQ(static_cast<byte>(c), codes[i]);
    EXPECT_EQ(Font::GetGlyph(kDefaultFont, c).GetError(), Error::kOk);
  }

  /* the Ü sprite used to be a copy of the O */
  Glyph const ue = Font::GetGlyph(kDefaultFont, '\xDC').GetValue();
  Glyph const o = Font::GetGlyph(kDefaultFont, 'O').GetValue();
  EXPECT_NE(Bytes(ue.columns, ue.width), Bytes(o.columns, o.width));
}

/* Bug: the font had no ß and no °, so SetText() failed on them and a shifting
 * text, "Grüße" or "21 °C", lost them without a word */
TEST_F(MaxMatrixTest, EszettAndDegreeAreDrawn) {
  char const* texts[] = {"ß", "°", "\xDF", "\xB0"};

  for (char const* text : texts) {
    SCOPED_TRACE(text);
    matrix_.Clear();
    EXPECT_EQ(matrix_.SetText(text).GetError(), Error::kOk);
    std::vector<byte> const shown = Display();
    EXPECT_TRUE(std::any_of(shown.begin(), shown.end(),
                            [](byte column) { return column != 0; }));
  }
}

TEST_F(MaxMatrixTest, DecodeStopsAtEndOfString) {
  char c;

  EXPECT_EQ(Font::DecodeChar("A", &c), 1);
  EXPECT_EQ(c, 'A');
  EXPECT_EQ(Font::DecodeChar("\xC3", &c), 1);

  char truncated[] = "a\xC3";
  ShiftWholeText(truncated);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* an unknown char used to be shifted in all the same, with whatever sprite was
 * left over; that the shift finishes does not show it, so what went to the
 * chain is compared with the text without it. Skipping it then took a
 * ShiftTask() call of its own that sent nothing, and the text stood still for
 * a step, so the calls are compared as well - in the middle of the text and at
 * its end. */
TEST_F(MaxMatrixTest, UnknownChars) {
  EXPECT_EQ(matrix_.SetText("a\x01").GetError(), Error::kUnknownChar);

  char known[] = "ab";
  int32_t frames = ShiftWholeText(known);
  uint16_t calls = shift_calls_;

  char in_the_middle[] =
      "a\x01\x02"
      "b";
  EXPECT_EQ(ShiftWholeText(in_the_middle), frames);
  EXPECT_EQ(shift_calls_, calls);
  EXPECT_EQ(matrix_.GetState(), State::kReady);

  char at_the_end[] = "ab\x01";
  EXPECT_EQ(ShiftWholeText(at_the_end), frames);
  EXPECT_EQ(shift_calls_, calls);
  EXPECT_EQ(matrix_.GetState(), State::kReady);
}

/* shutdown darkens every module and loses nothing: a dot drawn meanwhile shows
 * once it ends, at the intensity set before */
TEST_F(MaxMatrixTest, ShutdownKeepsThePicture) {
  std::vector<byte> const dark(kNumberOfColumns, 0x00);

  ASSERT_TRUE(matrix_.SetIntensity(9).IsOk());
  ASSERT_TRUE(matrix_.SetText("Hi").IsOk());
  matrix_.SetShutdown(true);
  for (byte module = 0; module < kNumberOfModules; module++) {
    EXPECT_EQ(ChipOfModule(module).shutdown, 0)
        << "module " << static_cast<uint16_t>(module);
  }
  EXPECT_EQ(Display(), dark);
  matrix_.SetDot(31, 7, true);
  EXPECT_EQ(Display(), dark) << "lit in shutdown";

  matrix_.SetShutdown(false);
  EXPECT_TRUE(matrix_.GetDot(31, 7).GetValue());
  EXPECT_EQ(Display()[31], 0x80) << "the dot drawn in shutdown";
  EXPECT_EQ(ChipOfModule(0).intensity, 9);
}

/* the display test lights every LED of every module at full brightness, even in
 * shutdown, and the picture is back when it ends */
TEST_F(MaxMatrixTest, DisplayTestLightsEveryLed) {
  ASSERT_TRUE(matrix_.SetText("Hi").IsOk());
  std::vector<byte> const shown = Display();

  matrix_.SetShutdown(true);
  matrix_.SetDisplayTest(true);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0xFF));
  matrix_.SetDisplayTest(false);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0x00))
      << "still in shutdown";
  matrix_.SetShutdown(false);
  EXPECT_EQ(Display(), shown);
}

class MaxMatrixCommandTest : public MaxMatrixTest {
 protected:
  Command command_;
  TextPrint answer_;

  /* feeds a line char by char, as Serial would; every char but the last has to
   * leave the line pending, and answer nothing */
  Result<CommandState> FeedLine(std::string const& text) {
    answer_.text.clear();
    for (size_t i = 0; i + 1 < text.size(); i++) {
      EXPECT_EQ(command_.Feed(matrix_, text[i], answer_),
                CommandState::kPending)
          << "char " << i << " of " << text;
    }
    EXPECT_EQ(answer_.text, "") << "answered before the end of " << text;
    return command_.Feed(matrix_, text.back(), answer_);
  }

  /* the display after every step of the shift running now, until it is done */
  std::vector<std::vector<byte>> ShiftSteps() {
    std::vector<std::vector<byte>> steps;
    for (uint16_t step = 0; step < 1000 && matrix_.GetState() != State::kReady;
         step++) {
      matrix_.ShiftTask();
      steps.push_back(Display());
    }
    return steps;
  }
};

TEST_F(MaxMatrixCommandTest, TextShowsWhatSetTextShows) {
  matrix_.SetText("xx");
  ASSERT_EQ(FeedLine("text=Hi!\n"), CommandState::kDone);
  std::vector<byte> const shown = Display();

  matrix_.Clear();
  matrix_.SetText("Hi!");
  EXPECT_EQ(shown, Display());
}

TEST_F(MaxMatrixCommandTest, SettingsAndDotsReachTheMatrix) {
  EXPECT_EQ(FeedLine("intensity=8\n"), CommandState::kDone);
  EXPECT_EQ(ChipOfModule(0).intensity, 8);
  EXPECT_EQ(FeedLine("speed=120\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetShiftInterval(), 120);
  EXPECT_EQ(FeedLine("space=3\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetSpaceBetweenChars(), 3);

  EXPECT_EQ(FeedLine("setDot=5,3,1\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetDot(5, 3).GetError(), Error::kOk);
  EXPECT_TRUE(matrix_.GetDot(5, 3).GetValue());
  EXPECT_EQ(FeedLine("setDot=5,3,0\n"), CommandState::kDone);
  EXPECT_EQ(matrix_.GetDot(5, 3).GetError(), Error::kOk);
  EXPECT_FALSE(matrix_.GetDot(5, 3).GetValue());

  matrix_.SetText("Hi");
  EXPECT_EQ(FeedLine("clear\n"), CommandState::kDone);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));
}

/* SetTextWithShift() keeps only a pointer: a shift that read its text out of
 * the line buffer would shift whatever line came next */
TEST_F(MaxMatrixCommandTest, TextShiftKeepsItsTextWhileLinesFollow) {
  char const text[] = "AB";

  ASSERT_EQ(FeedLine("textShift=AB\n"), CommandState::kDone);
  ASSERT_EQ(FeedLine("intensity=3\n"), CommandState::kDone);
  std::vector<std::vector<byte>> const commanded = ShiftSteps();

  matrix_.Clear();
  ASSERT_EQ(matrix_.SetTextWithShift(text).GetError(), Error::kOk);
  EXPECT_EQ(commanded, ShiftSteps());
}

TEST_F(MaxMatrixCommandTest, WrongLinesAreRefused) {
  struct {
    char const* line;
    Error error;
  } const cases[] = {
      {"blink\n", Error::kUnknownCommand},
      {"clear=1\n", Error::kUnknownCommand},
      {"intensity=16\n", Error::kOutOfRange},
      {"intensity=\n", Error::kNotANumber},
      {"intensity=x\n", Error::kNotANumber},
      {"speed=65536\n", Error::kOutOfRange},
      {"speed=99999999999999999999\n", Error::kOutOfRange},
      /* 2^32, which a count in 32 bits wraps round to 0, a speed it would take
       */
      {"speed=4294967296\n", Error::kOutOfRange},
      {"space=-1\n", Error::kNotANumber},
      {"setDot=1,2\n", Error::kNotANumber},
      {"setDot=32,0,1\n", Error::kOutOfRange},
      {"setDot=1,8,1\n", Error::kOutOfRange},
      {"setDot=1,2,2\n", Error::kOutOfRange},
      {"textShift=\n", Error::kNoText},
      {"text=a\x01\n", Error::kUnknownChar},
  };

  for (auto const& test_case : cases)
    EXPECT_EQ(FeedLine(test_case.line), test_case.error) << test_case.line;
  EXPECT_EQ(matrix_.GetShiftInterval(), 0) << "changed by a refused speed=";
}

TEST_F(MaxMatrixCommandTest, LineEndsInNewlineOrCarriageReturnNewline) {
  EXPECT_EQ(FeedLine("intensity=4\r\n"), CommandState::kDone);
  EXPECT_EQ(ChipOfModule(0).intensity, 4);
  EXPECT_EQ(FeedLine("\n"), CommandState::kPending) << "an empty line";
  EXPECT_EQ(FeedLine("intensity=5\n"), CommandState::kDone);
}

/* a line cut short would carry out a different command, so a line too long is
 * dropped whole; the next one is read */
TEST_F(MaxMatrixCommandTest, LineTooLongIsDroppedWhole) {
  std::string const long_text =
      "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n";

  EXPECT_EQ(FeedLine(long_text), Error::kLineTooLong);
  EXPECT_EQ(Display(), std::vector<byte>(kNumberOfColumns, 0));
  EXPECT_EQ(FeedLine("intensity=2\n"), CommandState::kDone);
  EXPECT_EQ(ChipOfModule(0).intensity, 2);
}

/* a program pairs every line it sent with one line back, so each line is
 * answered once, an empty line not at all */
TEST_F(MaxMatrixCommandTest, EveryLineIsAnsweredOnce) {
  struct {
    char const* line;
    char const* answer;
  } const cases[] = {
      {"intensity=8\n", "ok\r\n"},
      {"clear\r\n", "ok\r\n"},
      {"intensity=16\n", "error: out of range\r\n"},
      {"blink\n", "error: unknown command\r\n"},
      {"\n", ""},
      {"\r\n", ""},
  };

  for (auto const& test_case : cases) {
    FeedLine(test_case.line);
    EXPECT_EQ(answer_.text, test_case.answer) << test_case.line;
  }
  FeedLine("text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH, 'x') + "\n");
  EXPECT_EQ(answer_.text, "error: line too long\r\n");
}

/* picture answers in place of ok, and changes nothing */
TEST_F(MaxMatrixCommandTest, PictureIsAnsweredWithTheColumns) {
  matrix_.SetText("Hi");
  std::vector<byte> const shown = Display();

  EXPECT_EQ(FeedLine("picture\n"), CommandState::kAnswered);
  ASSERT_EQ(answer_.text.size(), 8 + 2 * kNumberOfColumns + 2);
  EXPECT_EQ(answer_.text.substr(0, 8), "picture=");
  EXPECT_EQ(answer_.text.substr(answer_.text.size() - 2), "\r\n");
  EXPECT_EQ(Display(), shown);
  EXPECT_EQ(FeedLine("picture=1\n"), Error::kUnknownCommand);
}

TEST_F(MaxMatrixCommandTest, LineOfFullLengthIsTaken) {
  std::string const text =
      "text=" + std::string(MAXMATRIX_COMMAND_LINE_LENGTH - 5, 'x') + "\n";

  EXPECT_EQ(FeedLine(text), CommandState::kDone);
}

/* picture answers with the panel as the viewer sees it, so that a program draws
 * it without knowing the orientations; one dot in a column of every module,
 * each in another row */
TEST_P(MaxMatrixMappingTest, PictureAnswerIsWhatTheViewerSees) {
  Command command;
  TextPrint answer;
  std::string expected = "picture=";

  matrix_.SetDot(0, 0, true);
  matrix_.SetDot(9, 1, true);
  matrix_.SetDot(16, 4, true);
  matrix_.SetDot(31, 7, true);
  for (char c : std::string("picture\n")) command.Feed(matrix_, c, answer);
  for (byte column = 0; column < kNumberOfColumns; column++) {
    expected += column == 0    ? "01"
                : column == 9  ? "02"
                : column == 16 ? "10"
                : column == 31 ? "80"
                               : "00";
  }
  EXPECT_EQ(answer.text, expected + "\r\n");
}

/* the default font, converted from the table of sprites the library had: every
 * ASCII char from the space to the tilde, no char between the tilde and the
 * degree sign, and 8 rows high */
TEST_F(MaxMatrixTest, DefaultFontHasAsciiAndNothingBetween) {
  EXPECT_EQ(matrix_.GetFont(), kDefaultFont);
  EXPECT_TRUE(Font::CheckFont(kDefaultFont, sizeof(kDefaultFont)).IsOk());
  for (uint16_t code = 32; code <= 126; code++) {
    Result<Glyph> const glyph =
        Font::GetGlyph(kDefaultFont, static_cast<char>(code));
    EXPECT_EQ(glyph.GetError(), Error::kOk) << code;
    EXPECT_EQ(glyph.GetValue().height, 8) << code;
  }
  for (uint16_t code = 127; code < 0xB0; code++) {
    EXPECT_EQ(Font::GetGlyph(kDefaultFont, static_cast<char>(code)).GetError(),
              Error::kUnknownChar)
        << code;
  }
  EXPECT_EQ(Font::GetGlyph(kDefaultFont, '\x1F').GetError(),
            Error::kUnknownChar);
  EXPECT_EQ(Font::GetGlyph(kDefaultFont, '\xFF').GetError(),
            Error::kUnknownChar);
}

/* a font of MD_MAX72XX that the sketch sets: its chars in its height, which is
 * not the panel's, so dot by dot; a char it lacks, by a width of 0 or beyond
 * its last, fails as one the default font lacks */
TEST_F(MaxMatrixTest, SetFontDrawsInThatFont) {
  byte const expected[] = {0x42, 0x7F, 0x00, 0x3E, 0x41, 0x3E, 0x00};

  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.GetFont(), kDigitFont);
  ASSERT_EQ(matrix_.SetText("10").GetError(), Error::kOk);
  for (byte column = 0; column < sizeof(expected); column++)
    EXPECT_EQ(matrix_.GetColumn(column).GetValue(), expected[column])
        << "column " << static_cast<uint16_t>(column);
  EXPECT_EQ(matrix_.SetText("2").GetError(), Error::kUnknownChar);
  EXPECT_EQ(matrix_.SetText("A").GetError(), Error::kUnknownChar);

  ASSERT_EQ(matrix_.SetFont(kDefaultFont).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.SetText("A").GetError(), Error::kOk);
}

/* version 2 gives first and last char in two bytes each, so its chars start two
 * bytes later */
TEST_F(MaxMatrixTest, SetFontTakesVersion2) {
  byte const expected[] = {0x3E, 0x41, 0x3E, 0x00, 0x42, 0x7F};

  ASSERT_EQ(matrix_.SetFont(kDigitFontVersion2).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetText("01").GetError(), Error::kOk);
  for (byte column = 0; column < sizeof(expected); column++)
    EXPECT_EQ(matrix_.GetColumn(column).GetValue(), expected[column])
        << "column " << static_cast<uint16_t>(column);
}

/* a sprite has five columns at most; a char of a font may have more, standing
 * and shifting in */
TEST_F(MaxMatrixTest, CharWiderThanASpriteIsDrawnWhole) {
  uint16_t steps = 0;

  ASSERT_EQ(matrix_.SetFont(kWideFont).GetError(), Error::kOk);
  ASSERT_EQ(matrix_.SetChar(0, 0, 'W').GetError(), Error::kOk);
  for (byte column = 0; column < 8; column++)
    EXPECT_EQ(matrix_.GetColumn(column).GetValue(), 1u << column)
        << "standing, column " << static_cast<uint16_t>(column);

  matrix_.Clear();
  ASSERT_EQ(matrix_.SetCharWithShift('W').GetError(), Error::kOk);
  for (; steps < 100 && matrix_.GetState() != State::kReady; steps++)
    matrix_.ShiftTask();
  EXPECT_EQ(steps, 8 + MAXMATRIX_SPACE_BETWEEN_CHARS_INIT_VALUE);
  for (byte column = 0; column < 8; column++) {
    EXPECT_EQ(matrix_.GetColumn(kNumberOfColumns - 10 + column).GetValue(),
              1u << column)
        << "shifted, column " << static_cast<uint16_t>(column);
  }
}

/* only what starts as a font is taken, and the font set before stays */
TEST_F(MaxMatrixTest, SetFontRefusesWhatIsNoFont) {
  static uint8_t const kVersion3[] PROGMEM = {'F', 3, 32, 32, 8, 1, 0};
  static uint8_t const kNoHeight[] PROGMEM = {'F', 1, 32, 32, 0, 1, 0};
  static uint8_t const kTooHigh[] PROGMEM = {'F', 1, 32, 32, 9, 1, 0};
  static uint8_t const kLastBeforeFirst[] PROGMEM = {'F', 1, 33, 32, 8, 1, 0};
  struct {
    uint8_t const* font;
    size_t size;
  } const fonts[] = {
      {NULL, 0},
      {kNoFont, sizeof(kNoFont)},
      {kVersion3, sizeof(kVersion3)},
      {kNoHeight, sizeof(kNoHeight)},
      {kTooHigh, sizeof(kTooHigh)},
      {kLastBeforeFirst, sizeof(kLastBeforeFirst)},
  };

  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  for (auto const& font : fonts) {
    EXPECT_EQ(matrix_.SetFont(font.font, font.size).GetError(),
              Error::kNotAFont)
        << static_cast<void const*>(font.font);
    EXPECT_EQ(matrix_.GetFont(), kDigitFont);
  }
}

/* Bug: only the header of a font was checked, as the format holds no length, so
 * a font whose header promised more chars than its array held was taken, and a
 * char beyond its end was read from whatever lay behind it in flash. Now the
 * chars have to fill the array exactly, from a header that fits into it. */
TEST_F(MaxMatrixTest, SetFontRefusesAFontThatDoesNotFillItsArray) {
  static uint8_t const kLastBeyondTheEnd[] PROGMEM = {'F', 1, '0', '1',
                                                      7,   1, 0x7F};
  static uint8_t const kByteAfterTheLast[] PROGMEM = {'F', 1, '0',  '0',
                                                      7,   1, 0x7F, 0};
  static uint8_t const kWidthBeyondTheEnd[] PROGMEM = {'F', 1, '0', '0',
                                                       7,   3, 0x7F};
  static uint8_t const kShortHeader[] PROGMEM = {'F', 1, '0', '0'};
  static uint8_t const kShortHeaderVersion2[] PROGMEM = {'F', 2, 0,
                                                         '0', 0, '0'};
  static uint8_t const kOneChar[] PROGMEM = {'F', 1, '0', '0', 7, 1, 0x7F};

  ASSERT_EQ(matrix_.SetFont(kDigitFont).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.SetFont(kLastBeyondTheEnd).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kByteAfterTheLast).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kWidthBeyondTheEnd).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kShortHeader).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kShortHeaderVersion2).GetError(), Error::kNotAFont);
  EXPECT_EQ(matrix_.SetFont(kDigitFont, sizeof(kDigitFont) - 1).GetError(),
            Error::kNotAFont);
  EXPECT_EQ(matrix_.GetFont(), kDigitFont);

  /* the same chars, filling the array, are taken, by the array and by its
   * address and length */
  EXPECT_EQ(matrix_.SetFont(kOneChar).GetError(), Error::kOk);
  EXPECT_EQ(matrix_.SetFont(kDigitFontVersion2, sizeof(kDigitFontVersion2))
                .GetError(),
            Error::kOk);
  EXPECT_EQ(matrix_.GetFont(), kDigitFontVersion2);
}

/* Bug: the count of the columns shifted in was a byte, so with the largest
 * space it never reached a char's width and space, and the char shifted for
 * ever */
TEST_F(MaxMatrixTest, CharWithTheLargestSpaceStopsShifting) {
  uint16_t steps = 0;

  matrix_.SetSpaceBetweenChars(255);
  ASSERT_EQ(matrix_.SetCharWithShift('A').GetError(), Error::kOk);
  for (; steps < 1000 && matrix_.GetState() != State::kReady; steps++)
    matrix_.ShiftTask();
  EXPECT_EQ(steps, 4 + 255);
}

/* Whether a line drawn from (x0, y0) to (x1, y1) is the line on the panel:
 * along its longer axis it has exactly one dot in each column or row between
 * the ends, at most half a dot from the true line, and nothing beyond the ends.
 */
static void ExpectLine(Matrix& matrix, int16_t x0, int16_t y0, int16_t x1,
                       int16_t y1) {
  bool const steep = std::abs(y1 - y0) > std::abs(x1 - x0);
  int16_t const length =
      steep ? max_matrix::kRowNumberOfModule : matrix.GetNumberOfColumns();
  int16_t const breadth =
      steep ? matrix.GetNumberOfColumns() : max_matrix::kRowNumberOfModule;
  double const along_0 = steep ? y0 : x0, along_1 = steep ? y1 : x1;
  double const across_0 = steep ? x0 : y0, across_1 = steep ? x1 : y1;

  for (int16_t along = 0; along < length; along++) {
    bool const inside = along >= std::min(along_0, along_1) &&
                        along <= std::max(along_0, along_1);
    double const ideal = along_0 == along_1
                             ? across_0
                             : across_0 + (across_1 - across_0) *
                                              (along - along_0) /
                                              (along_1 - along_0);
    uint16_t lit = 0;
    for (int16_t across = 0; across < breadth; across++) {
      if (!(steep ? matrix.GetDot(across, along) : matrix.GetDot(along, across))
               .GetValue())
        continue;
      lit++;
      EXPECT_TRUE(inside) << "a dot beyond the ends at " << along;
      EXPECT_LE(std::abs(across - ideal), 0.5)
          << "at " << along << ", " << across << " off the line";
    }
    bool const ideal_on_panel =
        std::round(ideal) >= 0 && std::round(ideal) < breadth;
    if (inside && ideal_on_panel) {
      EXPECT_EQ(lit, 1u) << "at " << along;
    }
  }
}

/* a line in each of the eight directions, both ways round, and lines whose ends
 * lie off the panel, as far as an int16_t reaches: the sum of their differences
 * does not fit in an int, which has 16 bits on the Uno */
TEST_F(MaxMatrixTest, DrawLineRunsFromEndToEnd) {
  struct {
    int16_t x0, y0, x1, y1;
  } const lines[] = {
      {2, 3, 20, 3},
      {7, 0, 7, 7},
      {0, 0, 7, 7},
      {0, 0, 31, 7},
      {3, 1, 9, 6},
      {30, 0, 26, 7},
      {0, 7, 31, 0},
      {12, 4, 12, 4},
      {-40, -10, 60, 17},
      {5, -20, 9, 30},
      {-32768, 0, 32767, 7},
      {-32768, -32768, 32767, 32767},
      {16, -32768, 17, 32767},
  };

  for (auto const& line : lines) {
    for (bool reverse : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "(" << line.x0 << ", " << line.y0 << ") to (" << line.x1
                   << ", " << line.y1 << ")" << (reverse ? " reversed" : ""));
      matrix_.Clear();
      if (reverse)
        matrix_.DrawLine(line.x1, line.y1, line.x0, line.y0, true);
      else
        matrix_.DrawLine(line.x0, line.y0, line.x1, line.y1, true);
      ExpectLine(matrix_, line.x0, line.y0, line.x1, line.y1);
    }
  }
  matrix_.Clear();
  matrix_.DrawLine(-32768, 0, 32767, 7, true);
  matrix_.DrawLine(-32768, 0, 32767, 7, false);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }))
      << "a line turned off";
}

/* a rectangle on the panel, one partly off it and one as large as an int16_t
 * allows; the outline is what the filled one has more than its inside */
TEST_F(MaxMatrixTest, RectanglesCoverTheirArea) {
  struct {
    int16_t x, y, width, height;
  } const rectangles[] = {
      {2, 1, 5, 4},         {10, 0, 1, 8},
      {20, 6, 4, 1},        {-3, -2, 6, 5},
      {29, 5, 10, 10},      {-32768, -32768, 32767, 32767},
      {0, 0, 32767, 32767}, {32767, 0, 32767, 8},
  };

  for (auto const& r : rectangles) {
    SCOPED_TRACE(::testing::Message()
                 << r.x << ", " << r.y << ", " << r.width << " x " << r.height);
    int32_t const right = static_cast<int32_t>(r.x) + r.width - 1,
                  bottom = static_cast<int32_t>(r.y) + r.height - 1;
    auto inside = [&](int16_t x, int16_t y) {
      return x >= r.x && x <= right && y >= r.y && y <= bottom;
    };
    auto edge = [&](int16_t x, int16_t y) {
      return inside(x, y) &&
             (x == r.x || x == right || y == r.y || y == bottom);
    };

    matrix_.Clear();
    matrix_.FillRect(r.x, r.y, r.width, r.height, true);
    EXPECT_EQ(ShownPicture(matrix_), Picture(inside)) << "filled";
    matrix_.Clear();
    matrix_.DrawRect(r.x, r.y, r.width, r.height, true);
    EXPECT_EQ(ShownPicture(matrix_), Picture(edge)) << "outline";
  }

  matrix_.FillRect(0, 0, 32, 8, true);
  matrix_.FillRect(1, 1, 3, 2, false);
  matrix_.DrawRect(5, 0, 0, 8, false);
  matrix_.FillRect(5, 0, 8, -1, false);
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return !(x >= 1 && x <= 3 && y >= 1 && y <= 2);
            }))
      << "turned off, and nothing drawn without a width or height";
}

/* the outlines the midpoint circle gives for small radii, round and closed,
 * also across the edge of the panel */
TEST_F(MaxMatrixTest, DrawCircleDrawsItsOutline) {
  std::vector<std::string> const radius_3 = {
      "..###..", ".#...#.", "#.....#", "#.....#",
      "#.....#", ".#...#.", "..###..",
  };

  matrix_.DrawCircle(10, 3, 3, true);
  EXPECT_EQ(
      ShownPicture(matrix_), Picture([&radius_3](int16_t x, int16_t y) {
        return x >= 7 && x <= 13 && y <= 6 &&
               radius_3[static_cast<size_t>(y)][static_cast<size_t>(x - 7)] ==
                   '#';
      }));

  matrix_.Clear();
  matrix_.DrawCircle(0, 0, 3, true);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&radius_3](int16_t x, int16_t y) {
              return x <= 3 && y <= 3 &&
                     radius_3[static_cast<size_t>(y + 3)]
                             [static_cast<size_t>(x + 3)] == '#';
            }))
      << "the quarter on the panel";

  matrix_.Clear();
  matrix_.DrawCircle(20, 4, 0, true);
  matrix_.DrawCircle(25, 4, 1, true);
  matrix_.DrawCircle(28, 4, -1, true);
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return (x == 20 && y == 4) ||
                     (std::abs(x - 25) + std::abs(y - 4) == 1);
            }))
      << "radius 0 is a dot, 1 a cross, a negative one nothing";

  matrix_.Clear();
  matrix_.DrawCircle(16, 3, 32767, true);
  matrix_.DrawCircle(-32768, -32768, 32767, true);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }))
      << "circles far beyond the panel";
}

/* a bitmap is drawn column by column from flash, also partly off the panel, and
 * leaves the rows below its height */
TEST_F(MaxMatrixTest, DrawBitmapDrawsItsColumns) {
  auto bitmap_dot = [](int16_t column, int16_t row) {
    return column >= 0 && column < static_cast<int16_t>(sizeof(kBitmap)) &&
           row >= 0 && row < 8 && bitRead(kBitmap[column], row);
  };

  matrix_.DrawBitmap(3, 0, kBitmap, sizeof(kBitmap));
  EXPECT_EQ(ShownPicture(matrix_), Picture([&](int16_t x, int16_t y) {
              return bitmap_dot(x - 3, y);
            }));

  matrix_.Clear();
  matrix_.DrawBitmap(-2, 3, kBitmap, sizeof(kBitmap));
  matrix_.DrawBitmap(29, -1, kBitmap, sizeof(kBitmap));
  EXPECT_EQ(ShownPicture(matrix_), Picture([&](int16_t x, int16_t y) {
              return bitmap_dot(x + 2, y - 3) || bitmap_dot(x - 29, y + 1);
            }))
      << "partly off the panel";

  matrix_.FillRect(0, 0, 32, 8, true);
  matrix_.DrawBitmap(10, 2, kBitmap, sizeof(kBitmap), 3);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&](int16_t x, int16_t y) {
              return x < 10 || x >= 15 || y < 2 || y >= 5 ||
                     bitmap_dot(x - 10, y - 2);
            }))
      << "three rows high, over a lit panel";

  matrix_.Clear();
  matrix_.DrawBitmap(32767, 0, kBitmap, sizeof(kBitmap));
  matrix_.DrawBitmap(-32768, 0, kBitmap, sizeof(kBitmap));
  matrix_.DrawBitmap(0, 32767, kBitmap, sizeof(kBitmap), 8);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }))
      << "far off the panel";
}

/* Chains of other lengths than the examples\', in every orientation: one
 * module, which is a pair of itself when the orientation changes, three, with
 * one module in the middle, and six. The buffer is the test's, filled with a
 * pattern that the matrix has to empty. */
static byte const kChainLengths[] = {1, 3, 6};
typedef std::tuple<byte, MatrixOrientation, ModuleType> ChainParameter;

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

static std::string ChainName(
    ::testing::TestParamInfo<ChainParameter> const& info) {
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
  byte const values[6] = {0x81, 0x42, 0x24, 0x18, 0xF0, 0x0F};
  byte read[6] = {0};
  byte read_one[1] = {0};
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
  for (; steps < 1000 && matrix_.GetState() != State::kReady; steps++)
    matrix_.ShiftTask();
  EXPECT_LT(steps, 1000);
  EXPECT_NE(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }, number_of_columns_))
      << "nothing shifted in";
  for (char c : std::string("picture\n")) command.Feed(matrix_, c, answer);
  EXPECT_EQ(answer.text.size(), 8 + 2u * number_of_columns_ + 2);
}
