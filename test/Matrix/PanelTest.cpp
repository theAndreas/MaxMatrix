// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The panel as the viewer sees it: every matrix orientation and module type,
 * dots, rows, columns and shifts, and the shapes drawn on it. */

#include <avr/pgmspace.h>
#include <gtest/gtest.h>
#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Arduino.h"
#include "Max7219.h"
#include "MaxMatrixTestSupport.h"
#include "Utility/Bit.h"

namespace max_matrix {
namespace {

/* a picture in flash, a column per byte, for DrawBitmap() */
constexpr auto kBitmap PROGMEM{
    make_array<uint8_t>(0x01, 0x82, 0xFF, 0x00, 0x3C)};

std::string MappingName(::testing::TestParamInfo<MappingPair> const& info) {
  return std::string("matrix") +
         ((std::get<0>(info.param) == MatrixOrientation::k180) ? "180" : "0") +
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
  PanelRow in;
  PanelRow out;
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    in[module] = Byte{
        Bit<uint8_t>::ToggleBits(0xA5u, static_cast<uint8_t>(module * 0x3Cu))};
  }

  for (uint8_t row{0}; row < kRowNumberOfModule; row++) {
    SCOPED_TRACE(::testing::Message() << "Row " << static_cast<uint16_t>(row));
    matrix_.SetRow(Y{row}, in);
    EXPECT_EQ(matrix_.GetRow(Y{row}, out).GetError(), Error::kOk);
    EXPECT_EQ(Bytes(out), Bytes(in));
  }
}

/* the display has to follow the buffer during the shift too, not only once the
 * text has left it */
TEST_P(MaxMatrixMappingTest, DisplayFollowsTheShift) {
  char const text[] = "Hi \xC4!";

  matrix_.SetTextWithShift(text);
  for (uint16_t step{0}; (step < 1000) && (matrix_.GetState() != State::kReady);
       step++) {
    matrix_.ShiftTask();
    ASSERT_EQ(Display(), Bytes(matrix_.panel_.wired_.matrix_buffer_))
        << "after step " << step;
  }
}

/* Whatever a call draws, it sends one frame per changed column of a module at
 * most. Every dot and column used to be a frame of its own: SetText("Hi!") on
 * FC-16 modules sent 64, and each step of a shifting text nine. */
TEST_P(MaxMatrixMappingTest, NoCallSendsMoreThanEightFrames) {
  struct Call {
    char const* name;
    void (*draw)(Matrix&);
  };
  std::array<Call, 11> const calls{{
      {"SetText", [](Matrix& m) { m.SetText("Hi!"); }},
      {"SetChar off the top row",
       [](Matrix& m) { m.SetChar(Point{X{9}, Y{1}}, 'A'); }},
      {"SetRow",
       [](Matrix& m) {
         PanelRow const row{Byte{0xFF}, Byte{0x0F}, Byte{0xF0}, Byte{0x3C}};
         m.SetRow(Y{5}, row);
       }},
      {"SetColumn", [](Matrix& m) { m.SetColumn(X{17}, Byte{0x5A}); }},
      {"ShiftUp", [](Matrix& m) { m.ShiftUp(ShiftFill::kRotate); }},
      {"ShiftLeft a column", [](Matrix& m) { m.ShiftLeft(Byte{0x3C}); }},
      {"clear", [](Matrix& m) { m.Clear(); }},
      {"DrawLine",
       [](Matrix& m) {
         m.DrawLine(Point{X{0}, Y{7}}, Point{X{31}, Y{0}}, true);
       }},
      {"FillRect",
       [](Matrix& m) {
         m.FillRect(Point{X{-1}, Y{-1}}, Point{X{32}, Y{8}}, true);
       }},
      {"DrawCircle",
       [](Matrix& m) { m.DrawCircle(Point{X{15}, Y{3}}, 9, true); }},
      {"DrawBitmap",
       [](Matrix& m) { m.DrawBitmap(Point{X{3}, Y{1}}, kBitmap); }},
  }};
  char const text[] = "Hi \xC4!";

  for (auto const& call : calls) {
    int32_t const frames_before{mock::frames};
    call.draw(matrix_);
    EXPECT_LE(mock::frames - frames_before, kColumnNumberOfModule) << call.name;
  }
  matrix_.SetTextWithShift(text);
  for (uint16_t step{0}; (step < 1000) && (matrix_.GetState() != State::kReady);
       step++) {
    int32_t const frames_before{mock::frames};
    matrix_.ShiftTask();
    ASSERT_LE(mock::frames - frames_before, kColumnNumberOfModule)
        << "shift step " << step;
  }
}

/* what reads the picture changes nothing, so a function handed the matrix only
 * to look at it takes it as Matrix const& (FN-11); the getters were not const
 */
TEST_F(MaxMatrixTest, PictureIsReadThroughAConstMatrix) {
  PanelRow row;
  matrix_.SetDot(Point{X{3}, Y{2}}, true);
  Matrix const& seen{matrix_};

  EXPECT_TRUE(seen.GetDot(Point{X{3}, Y{2}}).GetValue());
  EXPECT_EQ(seen.GetColumn(X{3}).GetValue(), Byte{0x04});
  EXPECT_EQ(seen.GetColumn(Module{0}, 3).GetValue(), Byte{0x04});
  EXPECT_EQ(seen.GetRow(Module{0}, 2).GetValue(), Byte{0x10});
  ASSERT_EQ(seen.GetRow(Y{2}, row).GetError(), Error::kOk);
  EXPECT_EQ(row[0], Byte{0x10});
}

/* a column of a module is the column of the panel that many modules in */
TEST_F(MaxMatrixTest, ColumnOfAModuleIsAColumnOfThePanel) {
  matrix_.SetColumn(Module{1}, 2, Byte{0xA3});

  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return (x == 10) && bitRead(0xA3, y);
            }));
  Result<Byte> const value{matrix_.GetColumn(Module{1}, 2)};
  EXPECT_EQ(value.GetError(), Error::kOk);
  EXPECT_EQ(value.GetValue(), Byte{0xA3});
}

/* rows and columns are two views of one picture: in orientation 180 SetColumn
 * turned the whole panel while SetRow turned each module */
TEST_P(MaxMatrixMappingTest, RowsAndColumnsAgree) {
  PanelRow row;
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    row[module] = Byte{
        Bit<uint8_t>::ToggleBits(0x81u, static_cast<uint8_t>(module * 0x16u))};
  }

  matrix_.SetRow(Y{2}, row);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&row](int16_t x, int16_t y) {
              return (y == 2) &&
                     ReadBit(
                         row[static_cast<size_t>(x / kColumnNumberOfModule)],
                         static_cast<uint8_t>(7 - (x % kColumnNumberOfModule)));
            }));

  matrix_.Clear();
  matrix_.SetColumn(X{5}, Byte{0xA3});
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return (x == 5) && bitRead(0xA3, y);
            }));
  for (uint8_t y{0}; y < kRowNumberOfModule; y++) {
    Result<Byte> const value{matrix_.GetRow(Module{0}, y)};
    EXPECT_EQ(value.GetError(), Error::kOk);
    EXPECT_EQ(value.GetValue().ToInteger(),
              bitRead(0xA3, y) ? Bit<uint8_t>::ToMask(7u - 5u) : 0u)
        << "Row " << static_cast<uint16_t>(y);
  }
}

/* every shift moves the whole picture by one dot as the viewer sees it, across
 * the modules, and fills the edge it leaves as it is told: with nothing, with
 * what fell out at the other edge, or with what the edge held */
TEST_P(MaxMatrixMappingTest, ShiftsMoveThePicture) {
  struct Shift {
    char const* name;
    void (*shift)(Matrix&);
    int16_t source_x, source_y;
    ShiftFill fill;
  };
  std::array<Shift, 12> const cases{{
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
  }};
  int16_t const width{kNumberOfColumns};
  int16_t const height{kRowNumberOfModule};

  for (auto const& test_case : cases) {
    matrix_.Clear();
    DrawPattern(matrix_);
    test_case.shift(matrix_);
    EXPECT_EQ(
        ShownPicture(matrix_), Picture([&test_case](int16_t x, int16_t y) {
          int16_t const source_x{static_cast<int16_t>(x + test_case.source_x)};
          int16_t const source_y{static_cast<int16_t>(y + test_case.source_y)};
          bool const inside = (source_x >= 0) && (source_x < width) &&
                              (source_y >= 0) && (source_y < height);
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
  Byte const leftmost{matrix_.GetColumn(X{0}).GetValue()};

  EXPECT_EQ(matrix_.ShiftLeft(Byte{0xA5}), leftmost);
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return (x == (kNumberOfColumns - 1)) ? (bitRead(0xA5, y) != 0)
                                                   : Pattern(x + 1, y);
            }))
      << "left";

  EXPECT_EQ(matrix_.ShiftRight(leftmost), Byte{0xA5});
  EXPECT_EQ(ShownPicture(matrix_), before) << "right, back to where it was";
}

/* columns fed one by one make a band longer than the panel, and come out at the
 * other edge in the order they went in: what a sketch passes on to a second
 * matrix */
TEST_F(MaxMatrixTest, ShiftedColumnsMakeABand) {
  std::vector<uint8_t> band;
  band.reserve(size_t{3} * kNumberOfColumns);
  for (uint16_t index{0}; index < (3 * kNumberOfColumns); index++) {
    band.push_back(static_cast<uint8_t>((index * 37) + 11));
  }
  std::vector<uint8_t> out;
  out.reserve(band.size());

  for (uint8_t const column : band) {
    out.push_back(matrix_.ShiftLeft(Byte{column}).ToInteger());
  }
  for (uint8_t column{0}; column < kNumberOfColumns; column++) {
    EXPECT_EQ(matrix_.GetColumn(X{column}).GetValue().ToInteger(),
              band[band.size() - kNumberOfColumns + column])
        << "column " << static_cast<uint16_t>(column);
  }
  EXPECT_EQ(std::vector<uint8_t>(out.begin(), out.begin() + kNumberOfColumns),
            std::vector<uint8_t>(kNumberOfColumns, 0x00))
      << "the empty panel first";
  EXPECT_EQ(std::vector<uint8_t>(out.begin() + kNumberOfColumns, out.end()),
            std::vector<uint8_t>(band.begin(), band.end() - kNumberOfColumns))
      << "then the band, in its order";
}

/* changing the orientation or the module type rewrites the buffer along, so the
 * viewer still sees the same picture; changing the module orientation to or
 * from 180 turned each module while SetColumn had turned the whole panel */
TEST_P(MaxMatrixMappingTest, PictureStaysWhenTheMappingChanges) {
  for (MatrixOrientation const matrix_orientation : kMatrixOrientations) {
    for (ModuleType const module_type : kModuleTypes) {
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
  for (uint8_t column{0}; column < kNumberOfColumns; column++) {
    for (uint8_t row{0}; row < kRowNumberOfModule; row++) {
      SCOPED_TRACE(::testing::Message()
                   << "Column " << static_cast<uint16_t>(column) << ", Row "
                   << static_cast<uint16_t>(row));
      bool const expected{Bit<uint32_t>::ReadBit((column * 3u) + row, 0)};
      Point const dot{X{column}, Y{row}};
      matrix_.SetDot(dot, expected);
      Result<bool> const value{matrix_.GetDot(dot)};
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
using ModuleCase = std::tuple<ModuleType, bool>;

class MaxMatrixModuleTest : public MaxMatrixTest,
                            public ::testing::WithParamInterface<ModuleCase> {};

std::string ModuleName(::testing::TestParamInfo<ModuleCase> const& info) {
  return ModuleTypeName(std::get<0>(info.param)) +
         (std::get<1>(info.param) ? "_upside_down" : "");
}

INSTANTIATE_TEST_SUITE_P(Modules, MaxMatrixModuleTest,
                         ::testing::Combine(::testing::ValuesIn(kModuleTypes),
                                            ::testing::Bool()),
                         ModuleName);

TEST_F(MaxMatrixTest, NamedModuleTypesAreTheirWirings) {
  EXPECT_EQ(ModuleTypeName(ModuleType::kFc16), kMax7219WiringFc16.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kGeneric), kMax7219WiringGeneric.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kParola), kMax7219WiringParola.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kIcstation),
            kMax7219WiringIcstation.name);
}

/* a dot set at X, Y is lit at X, Y of the panel; the pattern has no symmetry a
 * wrong mapping could hide behind */
TEST_P(MaxMatrixModuleTest, DotsAppearWhereTheyAreSet) {
  ModuleType const module_type{std::get<0>(GetParam())};
  bool const upside_down{std::get<1>(GetParam())};
  Max7219Wiring const& wiring{
      kMax7219Wirings[static_cast<uint8_t>(module_type)]};
  auto const pattern{[](uint16_t x, uint8_t y) {
    return (((x * x) + (3 * y) + (x * y)) % 3) == 0;
  }};

  matrix_.SetMatrixOrientation(upside_down ? MatrixOrientation::k180
                                           : MatrixOrientation::k0);
  matrix_.SetModuleType(module_type);
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < kRowNumberOfModule; y++) {
      matrix_.SetDot(Point{X{x}, Y{y}}, pattern(x, y));
    }
  }
  for (uint16_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < kRowNumberOfModule; y++) {
      EXPECT_EQ(mock::chain.IsLit(wiring, x, y, upside_down), pattern(x, y))
          << "X " << x << ", Y " << y;
    }
  }
}

/* drawing off the panel leaves the panel as it was and sends nothing; column 8
 * of module 0 used to be checked against the panel alone, which would have
 * drawn it on module 1 */
TEST_F(MaxMatrixTest, DrawingOffThePanelIsLeftOut) {
  PanelRow const row{Byte{0xFF}, Byte{0xFF}, Byte{0xFF}, Byte{0xFF}};
  struct OffPanel {
    char const* name;
    void (*draw)(Matrix&, PanelRow const&);
  };
  std::array<OffPanel, 12> const cases{{
      {"SetDot left",
       [](Matrix& m, PanelRow const&) { m.SetDot(Point{X{-1}, Y{0}}, true); }},
      {"SetDot right",
       [](Matrix& m, PanelRow const&) {
         m.SetDot(Point{X{kNumberOfColumns}, Y{0}}, true);
       }},
      {"SetDot above",
       [](Matrix& m, PanelRow const&) { m.SetDot(Point{X{0}, Y{-1}}, true); }},
      {"SetDot below",
       [](Matrix& m, PanelRow const&) {
         m.SetDot(Point{X{0}, Y{kRowNumberOfModule}}, true);
       }},
      {"SetColumn left",
       [](Matrix& m, PanelRow const&) { m.SetColumn(X{-1}, Byte{0xFF}); }},
      {"SetColumn right",
       [](Matrix& m, PanelRow const&) {
         m.SetColumn(X{kNumberOfColumns}, Byte{0xFF});
       }},
      {"SetColumn no module",
       [](Matrix& m, PanelRow const&) {
         m.SetColumn(Module{kNumberOfModules}, 0, Byte{0xFF});
       }},
      {"SetColumn off its module",
       [](Matrix& m, PanelRow const&) {
         m.SetColumn(Module{0}, kColumnNumberOfModule, Byte{0xFF});
       }},
      {"SetRow above",
       [](Matrix& m, PanelRow const& r) { m.SetRow(Y{-1}, r); }},
      {"SetRow below",
       [](Matrix& m, PanelRow const& r) {
         m.SetRow(Y{kRowNumberOfModule}, r);
       }},
      {"SetRow no module",
       [](Matrix& m, PanelRow const&) {
         m.SetRow(Module{kNumberOfModules}, 0, Byte{0xFF});
       }},
      {"SetRow off its module",
       [](Matrix& m, PanelRow const&) {
         m.SetRow(Module{0}, kRowNumberOfModule, Byte{0xFF});
       }},
  }};

  for (auto const& test_case : cases) {
    matrix_.Clear();
    int32_t const frames_before{mock::frames};
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
  Result<Glyph> const glyph{Font::GetGlyph(kDefaultFont, 'A')};
  ASSERT_EQ(glyph.GetError(), Error::kOk);
  auto const width{static_cast<int16_t>(glyph.GetValue().columns.size())};
  int16_t const height{glyph.GetValue().height};
  span<uint8_t const> const columns{glyph.GetValue().columns};
  struct Place {
    int16_t x, y;
  };
  std::array<Place, 4> const places{{{-2, 0},
                                     {kNumberOfColumns - 2, 0},
                                     {-2, -3},
                                     {kNumberOfColumns - 2, 5}}};

  for (auto const& place : places) {
    matrix_.Clear();
    ASSERT_EQ(matrix_.SetChar(Point{X{place.x}, Y{place.y}}, 'A').GetError(),
              Error::kOk);
    EXPECT_EQ(ShownPicture(matrix_),
              Picture([columns, &place, width, height](int16_t x, int16_t y) {
                int16_t const sprite_x{static_cast<int16_t>(x - place.x)};
                int16_t const sprite_y{static_cast<int16_t>(y - place.y)};
                return (sprite_x >= 0) && (sprite_x < width) &&
                       (sprite_y >= 0) && (sprite_y < height) &&
                       bitRead(columns[static_cast<size_t>(sprite_x)],
                               sprite_y);
              }))
        << "X " << place.x << ", Y " << place.y;
  }
}

/* a line from (x0, y0) to (x1, y1) */
struct Line {
  int16_t x0, y0, x1, y1;
};

/* a line as seen along its longer axis: from along_0 to along_1, at across_0
 * and across_1 on the other axis; steep if the longer axis is the rows */
struct Axes {
  bool steep;
  double along_0, along_1, across_0, across_1;
};

/* Whether the dots across the line at `along` are the line's: exactly one
 * between the ends, at most half a dot from the true line, none beyond them. */
// FN-16: a test's helper; its one caller passes `along` and `breadth` by
// those names
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
void ExpectDotsAt(Matrix const& matrix, Axes const& axes, int16_t along,
                  int16_t breadth) {
  bool const inside{(along >= std::min(axes.along_0, axes.along_1)) &&
                    (along <= std::max(axes.along_0, axes.along_1))};
  double const ideal{(axes.along_0 == axes.along_1)
                         ? axes.across_0
                         : (axes.across_0 + ((axes.across_1 - axes.across_0) *
                                             (along - axes.along_0) /
                                             (axes.along_1 - axes.along_0)))};
  uint16_t lit{0};

  for (int16_t across{0}; across < breadth; across++) {
    Point const steep_dot{X{across}, Y{along}};
    Point const flat_dot{X{along}, Y{across}};
    bool const dot{matrix.GetDot(axes.steep ? steep_dot : flat_dot).GetValue()};
    if (!dot) {
      continue;
    }
    lit++;
    EXPECT_TRUE(inside) << "a dot beyond the ends at " << along;
    EXPECT_LE(std::abs(across - ideal), 0.5)
        << "at " << along << ", " << across << " off the line";
  }
  bool const ideal_on_panel{(std::round(ideal) >= 0) &&
                            (std::round(ideal) < breadth)};
  if (inside && ideal_on_panel) {
    EXPECT_EQ(lit, 1u) << "at " << along;
  }
}

/* Whether `line`, drawn, is the line on the panel: along its longer axis the
 * dots of every column or row are the line's. */
void ExpectLine(Matrix const& matrix, Line const& line) {
  bool const steep{std::abs(line.y1 - line.y0) > std::abs(line.x1 - line.x0)};
  Axes const axes{steep, static_cast<double>(steep ? line.y0 : line.x0),
                  static_cast<double>(steep ? line.y1 : line.x1),
                  static_cast<double>(steep ? line.x0 : line.y0),
                  static_cast<double>(steep ? line.x1 : line.y1)};
  int16_t const length{steep ? kRowNumberOfModule
                             : matrix.GetNumberOfColumns()};
  int16_t const breadth{steep ? matrix.GetNumberOfColumns()
                              : kRowNumberOfModule};

  for (int16_t along{0}; along < length; along++) {
    ExpectDotsAt(matrix, axes, along, breadth);
  }
}

/* a line in each of the eight directions, both ways round, and lines whose ends
 * lie off the panel, as far as an int16_t reaches: the sum of their differences
 * does not fit in an int, which has 16 bits on the Uno */
TEST_F(MaxMatrixTest, DrawLineRunsFromEndToEnd) {
  std::array<Line, 13> const lines{{
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
  }};

  for (auto const& line : lines) {
    for (bool const reverse : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "(" << line.x0 << ", " << line.y0 << ") to (" << line.x1
                   << ", " << line.y1 << ")" << (reverse ? " reversed" : ""));
      matrix_.Clear();
      Point const from{X{line.x0}, Y{line.y0}};
      Point const to{X{line.x1}, Y{line.y1}};
      if (reverse) {
        matrix_.DrawLine(to, from, true);
      } else {
        matrix_.DrawLine(from, to, true);
      }
      ExpectLine(matrix_, line);
    }
  }
  matrix_.Clear();
  Point const far_left{X{-32768}, Y{0}};
  Point const far_right{X{32767}, Y{7}};
  matrix_.DrawLine(far_left, far_right, true);
  matrix_.DrawLine(far_left, far_right, false);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }))
      << "a line turned off";
}

/* a rectangle on the panel, one partly off it, one of a row, a column or a
 * dot, and one as large as an int16_t allows, by either pair of opposite
 * corners in either order; the outline is what the filled one has more than its
 * inside */
TEST_F(MaxMatrixTest, RectanglesCoverTheirArea) {
  struct Rectangle {
    int16_t left, top, right, bottom;
  };
  std::array<Rectangle, 9> const rectangles{{
      {2, 1, 6, 4},
      {10, 0, 10, 7},
      {20, 6, 23, 6},
      {12, 4, 12, 4},
      {-3, -2, 2, 2},
      {29, 5, 38, 14},
      {-32768, -32768, 32767, 32767},
      {0, 0, 32767, 32767},
      {32767, 0, 32767, 7},
  }};

  for (auto const& r : rectangles) {
    auto inside{[&](int16_t x, int16_t y) {
      return (x >= r.left) && (x <= r.right) && (y >= r.top) && (y <= r.bottom);
    }};
    auto const edge{[&](int16_t x, int16_t y) {
      return inside(x, y) && ((x == r.left) || (x == r.right) || (y == r.top) ||
                              (y == r.bottom));
    }};
    Point const top_left{X{r.left}, Y{r.top}};
    Point const bottom_right{X{r.right}, Y{r.bottom}};
    Point const top_right{X{r.right}, Y{r.top}};
    Point const bottom_left{X{r.left}, Y{r.bottom}};
    std::array<std::pair<Point, Point>, 4> const corners{{
        {top_left, bottom_right},
        {bottom_right, top_left},
        {top_right, bottom_left},
        {bottom_left, top_right},
    }};

    for (auto const& pair : corners) {
      SCOPED_TRACE(::testing::Message()
                   << "(" << pair.first.x.number << ", " << pair.first.y.number
                   << ") to (" << pair.second.x.number << ", "
                   << pair.second.y.number << ")");
      matrix_.Clear();
      matrix_.FillRect(pair.first, pair.second, true);
      EXPECT_EQ(ShownPicture(matrix_), Picture(inside)) << "filled";
      matrix_.Clear();
      matrix_.DrawRect(pair.first, pair.second, true);
      EXPECT_EQ(ShownPicture(matrix_), Picture(edge)) << "outline";
    }
  }

  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);
  matrix_.FillRect(Point{X{1}, Y{1}}, Point{X{3}, Y{2}}, false);
  matrix_.DrawRect(Point{X{5}, Y{3}}, Point{X{7}, Y{5}}, false);
  EXPECT_EQ(
      ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
        bool const filled_off{(x >= 1) && (x <= 3) && (y >= 1) && (y <= 2)};
        bool const in_outline{(x >= 5) && (x <= 7) && (y >= 3) && (y <= 5)};
        bool const off_centre{(x != 6) || (y != 4)};
        bool const outline_off{in_outline && off_centre};
        return !(filled_off || outline_off);
      }))
      << "turned off";
}

/* the outlines the midpoint circle gives for small radii, round and closed,
 * also across the edge of the panel */
TEST_F(MaxMatrixTest, DrawCircleDrawsItsOutline) {
  std::vector<std::string> const radius_3{
      "..###..", ".#...#.", "#.....#", "#.....#",
      "#.....#", ".#...#.", "..###..",
  };

  matrix_.DrawCircle(Point{X{10}, Y{3}}, 3, true);
  EXPECT_EQ(
      ShownPicture(matrix_), Picture([&radius_3](int16_t x, int16_t y) {
        return (x >= 7) && (x <= 13) && (y <= 6) &&
               (radius_3[static_cast<size_t>(y)][static_cast<size_t>(x - 7)] ==
                '#');
      }));

  matrix_.Clear();
  matrix_.DrawCircle(Point{X{0}, Y{0}}, 3, true);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&radius_3](int16_t x, int16_t y) {
              return (x <= 3) && (y <= 3) &&
                     (radius_3[static_cast<size_t>(y + 3)]
                              [static_cast<size_t>(x + 3)] == '#');
            }))
      << "the quarter on the panel";

  matrix_.Clear();
  matrix_.DrawCircle(Point{X{20}, Y{4}}, 0, true);
  matrix_.DrawCircle(Point{X{25}, Y{4}}, 1, true);
  matrix_.DrawCircle(Point{X{28}, Y{4}}, -1, true);
  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return ((x == 20) && (y == 4)) ||
                     ((std::abs(x - 25) + std::abs(y - 4)) == 1);
            }))
      << "radius 0 is a dot, 1 a cross, a negative one nothing";

  matrix_.Clear();
  matrix_.DrawCircle(Point{X{16}, Y{3}}, 32767, true);
  matrix_.DrawCircle(Point{X{-32768}, Y{-32768}}, 32767, true);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }))
      << "circles far beyond the panel";
}

/* a bitmap is drawn column by column from flash, also partly off the panel, and
 * leaves the rows below its height */
TEST_F(MaxMatrixTest, DrawBitmapDrawsItsColumns) {
  auto bitmap_dot{[](int16_t column, int16_t row) {
    return (column >= 0) && (column < static_cast<int16_t>(kBitmap.size())) &&
           (row >= 0) && (row < 8) &&
           bitRead(kBitmap[static_cast<size_t>(column)], row);
  }};

  matrix_.DrawBitmap(Point{X{3}, Y{0}}, kBitmap);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&](int16_t x, int16_t y) {
              return bitmap_dot(x - 3, y);
            }));

  matrix_.Clear();
  matrix_.DrawBitmap(Point{X{-2}, Y{3}}, kBitmap);
  matrix_.DrawBitmap(Point{X{29}, Y{-1}}, kBitmap);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&](int16_t x, int16_t y) {
              return bitmap_dot(x + 2, y - 3) || bitmap_dot(x - 29, y + 1);
            }))
      << "partly off the panel";

  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);
  matrix_.DrawBitmap(Point{X{10}, Y{2}}, 3, kBitmap);
  EXPECT_EQ(ShownPicture(matrix_), Picture([&](int16_t x, int16_t y) {
              return (x < 10) || (x >= 15) || (y < 2) || (y >= 5) ||
                     bitmap_dot(x - 10, y - 2);
            }))
      << "three rows high, over a lit panel";

  matrix_.Clear();
  matrix_.DrawBitmap(Point{X{32767}, Y{0}}, kBitmap);
  matrix_.DrawBitmap(Point{X{-32768}, Y{0}}, kBitmap);
  matrix_.DrawBitmap(Point{X{0}, Y{32767}}, 8, kBitmap);
  matrix_.DrawBitmap(Point{X{0}, Y{-32768}}, 8, kBitmap);
  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return false; }))
      << "far off the panel";
}

/* whether DrawBitmap() takes a bitmap and then a number, as its width once
 * came after the bitmap */
template <typename Matrix, typename = void>
struct TakesAWidthAfterTheBitmap : std::false_type {};
template <typename Matrix>
struct TakesAWidthAfterTheBitmap<
    Matrix, std::void_t<decltype(std::declval<Matrix&>().DrawBitmap(
                Point{X{0}, Y{0}}, kBitmap, kBitmap.size()))>>
    : std::true_type {};

/* Bug, of the width taken from the bitmap: DrawBitmap(x, y, bitmap, width), as
 * it was called before, compiled with the height last and drew the width as
 * the height; the height goes before the bitmap, so that it does not compile */
TEST_F(MaxMatrixTest, DrawBitmapTakesNoWidthAfterTheBitmap) {
  static_assert(!TakesAWidthAfterTheBitmap<Matrix>::value,
                "a width after the bitmap would be read as its height");
}

/* a bitmap less than a module high at the top row leaves the rows below it,
 * where a whole column at once would have turned them off */
TEST_F(MaxMatrixTest, BitmapAtTheTopLeavesTheRowsBelowIt) {
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.DrawBitmap(Point{X{10}, Y{0}}, 3, kBitmap);

  EXPECT_EQ(ShownPicture(matrix_), Picture([](int16_t x, int16_t y) {
              return (x < 10) || (x >= 15) || (y >= 3) ||
                     bitRead(kBitmap[static_cast<size_t>(x - 10)], y);
            }));
}

}  // namespace
}  // namespace max_matrix
