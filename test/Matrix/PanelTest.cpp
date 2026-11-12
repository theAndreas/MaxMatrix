// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/* The panel as the viewer sees it: every matrix orientation and module type,
 * dots, rows, columns and shifts, and the shapes drawn on it. */

#include <algorithm>
#include <cmath>
#include <string>
#include <tuple>
#include <vector>

#include "MaxMatrixTestSupport.h"

using max_matrix::Error;
using max_matrix::Font;
using max_matrix::Glyph;
using max_matrix::kDefaultFont;
using max_matrix::Matrix;
using max_matrix::MatrixOrientation;
using max_matrix::ModuleType;
using max_matrix::Result;
using max_matrix::ShiftFill;
using max_matrix::State;

namespace {

/* a picture in flash, a column per byte, for DrawBitmap() */
byte const kBitmap[] PROGMEM = {0x01, 0x82, 0xFF, 0x00, 0x3C};

std::string MappingName(::testing::TestParamInfo<MappingPair> const& info) {
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
  for (byte module = 0; module < kNumberOfModules; module++) {
    in[module] = static_cast<byte>(0xA5U ^ (module * 0x3CU));
  }

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
         PanelRow const row{0xFF, 0x0F, 0xF0, 0x3C};
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
    int32_t const frames_before{mock::frames};
    call.draw(matrix_);
    EXPECT_LE(mock::frames - frames_before, max_matrix::kColumnNumberOfModule)
        << call.name;
  }
  matrix_.SetTextWithShift(text);
  for (uint16_t step = 0; step < 1000 && matrix_.GetState() != State::kReady;
       step++) {
    int32_t const frames_before{mock::frames};
    matrix_.ShiftTask();
    ASSERT_LE(mock::frames - frames_before, max_matrix::kColumnNumberOfModule)
        << "shift step " << step;
  }
}
/* rows and columns are two views of one picture: in orientation 180 SetColumn
 * turned the whole panel while SetRow turned each module */
TEST_P(MaxMatrixMappingTest, RowsAndColumnsAgree) {
  PanelRow row;
  for (byte module = 0; module < kNumberOfModules; module++) {
    row[module] = static_cast<byte>(0x81U ^ (module * 0x16U));
  }

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
    Result<byte> const value{matrix_.GetRow(0, y)};
    EXPECT_EQ(value.GetError(), Error::kOk);
    EXPECT_EQ(value.GetValue(), bitRead(0xA3, y) ? 1U << (7U - 5U) : 0U)
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
          int16_t const source_x{static_cast<int16_t>(x + test_case.source_x)};
          int16_t const source_y{static_cast<int16_t>(y + test_case.source_y)};
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
  for (uint16_t index = 0; index < 3 * kNumberOfColumns; index++) {
    band.push_back(static_cast<byte>(index * 37 + 11));
  }
  std::vector<byte> out;
  out.reserve(band.size());

  for (byte const column : band) out.push_back(matrix_.ShiftLeft(column));
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
      bool const expected{((column * 3U + row) & 1U) != 0U};
      matrix_.SetDot(column, row, expected);
      Result<bool> const value{matrix_.GetDot(column, row)};
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

std::string ModuleName(::testing::TestParamInfo<Module> const& info) {
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
  auto const pattern = [](uint16_t x, uint8_t y) {
    return (x * x + 3 * y + x * y) % 3 == 0;
  };

  matrix_.SetMatrixOrientation(upside_down ? MatrixOrientation::k180
                                           : MatrixOrientation::k0);
  matrix_.SetModuleType(module_type);
  for (byte x = 0; x < kNumberOfColumns; x++) {
    for (byte y = 0; y < max_matrix::kRowNumberOfModule; y++) {
      matrix_.SetDot(x, y, pattern(x, y));
    }
  }
  for (uint16_t x = 0; x < kNumberOfColumns; x++) {
    for (uint8_t y = 0; y < max_matrix::kRowNumberOfModule; y++) {
      EXPECT_EQ(mock::chain.IsLit(wiring, x, y, upside_down), pattern(x, y))
          << "X " << x << ", Y " << y;
    }
  }
}
/* drawing off the panel leaves the panel as it was and sends nothing; column 8
 * of module 0 used to be checked against the panel alone, which would have
 * drawn it on module 1 */
TEST_F(MaxMatrixTest, DrawingOffThePanelIsLeftOut) {
  PanelRow const row{0xFF, 0xFF, 0xFF, 0xFF};
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
                int16_t const sprite_x{static_cast<int16_t>(x - place.x)};
                int16_t const sprite_y{static_cast<int16_t>(y - place.y)};
                return sprite_x >= 0 && sprite_x < width && sprite_y >= 0 &&
                       sprite_y < height &&
                       bitRead(columns[sprite_x], sprite_y);
              }))
        << "X " << place.x << ", Y " << place.y;
  }
}
/* Whether a line drawn from (x0, y0) to (x1, y1) is the line on the panel:
 * along its longer axis it has exactly one dot in each column or row between
 * the ends, at most half a dot from the true line, and nothing beyond the ends.
 */
void ExpectLine(Matrix& matrix, int16_t x0, int16_t y0, int16_t x1,
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
               .GetValue()) {
        continue;
      }
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
    for (bool const reverse : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "(" << line.x0 << ", " << line.y0 << ") to (" << line.x1
                   << ", " << line.y1 << ")" << (reverse ? " reversed" : ""));
      matrix_.Clear();
      if (reverse) {
        matrix_.DrawLine(line.x1, line.y1, line.x0, line.y0, true);
      } else {
        matrix_.DrawLine(line.x0, line.y0, line.x1, line.y1, true);
      }
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
    auto const edge = [&](int16_t x, int16_t y) {
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

}  // namespace
