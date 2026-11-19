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
#include "MaxMatrixAttributes.h"
#include "MaxMatrixTestSupport.h"

namespace max_matrix {
namespace {

/* a picture in flash, a column per byte, for DrawBitmap() */
constexpr auto kBitmap PROGMEM{
    make_array<uint8_t>(0x01, 0x82, 0xFF, 0x00, 0x3C)};

/* The tests on the fixture take the name of the file, as TS-7 asks. */
class PanelTest : public MaxMatrixTest {};

/* MaxMatrixMappingTest is the facade in every mapping; the tests of Command
 * instantiate it in their binary as well */
INSTANTIATE_TEST_SUITE_P(
    AllMappings, MaxMatrixMappingTest,
    ::testing::Combine(::testing::ValuesIn(kMatrixOrientations),
                       ::testing::ValuesIn(kModuleTypes)),
    MappingPairName);

/* the dots lit, and every other dark */
std::vector<std::string> Dots(std::vector<Point> const& dots) {
  return Picture([&dots](int16_t x, int16_t y) {
    return std::any_of(dots.begin(), dots.end(), [x, y](Point const& dot) {
      return (dot.x.number == x) && (dot.y.number == y);
    });
  });
}

/* a row of the whole panel, a byte per module: A5, 99, DD and 11 differ in
 * every module, so that a row that reached module 0 alone shows */
constexpr PanelRow kRow{{Byte{0xA5}, Byte{0x99}, Byte{0xDD}, Byte{0x11}}};

/* Bug: the bit position used to run on across modules, so only module 0 got its
 * row */
TEST_P(MaxMatrixMappingTest, TopRowRoundTrips) {
  PanelRow read{};
  matrix_.SetRow(Y{0}, kRow);

  ASSERT_EQ(matrix_.GetRow(Y{0}, read).GetError(), Error::kOk);

  EXPECT_EQ(Bytes(read), (std::vector<uint8_t>{0xA5, 0x99, 0xDD, 0x11}));
}

TEST_P(MaxMatrixMappingTest, BottomRowRoundTrips) {
  PanelRow read{};
  matrix_.SetRow(Y{7}, kRow);

  ASSERT_EQ(matrix_.GetRow(Y{7}, read).GetError(), Error::kOk);

  EXPECT_EQ(Bytes(read), (std::vector<uint8_t>{0xA5, 0x99, 0xDD, 0x11}));
}

/* the display has to follow the buffer during the shift too, not only once the
 * text has left it: seven steps are inside the second char of "Hi" */
TEST_P(MaxMatrixMappingTest, DisplayFollowsTheShift) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi").GetError(), Error::kOk);

  ShiftSteps(matrix_, 7);

  EXPECT_EQ(Display(), Bytes(buffer_));
}

/* a call that draws, for the frames it sends */
struct DrawCall {
  char const* name;
  void (*draw)(Matrix&);
};

constexpr std::array<DrawCall, 11> kDrawCalls{{
    {"SetText", [](Matrix& m) { m.SetText("Hi!"); }},
    {"SetCharOffTheTopRow",
     [](Matrix& m) { m.SetChar(Point{X{9}, Y{1}}, 'A'); }},
    {"SetRow",
     [](Matrix& m) {
       PanelRow const row{{Byte{0xFF}, Byte{0x0F}, Byte{0xF0}, Byte{0x3C}}};
       m.SetRow(Y{5}, row);
     }},
    {"SetColumn", [](Matrix& m) { m.SetColumn(X{17}, Byte{0x5A}); }},
    {"ShiftUpRotating", [](Matrix& m) { m.ShiftUp(ShiftFill::kRotate); }},
    {"ShiftLeftAColumn", [](Matrix& m) { m.ShiftLeft(Byte{0x3C}); }},
    {"Clear", [](Matrix& m) { m.Clear(); }},
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
    {"DrawBitmap", [](Matrix& m) { m.DrawBitmap(Point{X{3}, Y{1}}, kBitmap); }},
}};

/* Whatever a call draws, it sends one frame per changed column of a module at
 * most. Every dot and column used to be a frame of its own: SetText("Hi!") on
 * FC-16 modules sent 64, and each step of a shifting text nine. The calls run
 * on a module type with digits as columns and on one with digits as rows,
 * where a column of the viewer is a row of the wiring; the other bits of the
 * mapping turn a module or the panel and change no count of columns. */
using FrameCase = std::tuple<ModuleType, DrawCall>;

class FrameCountTest : public PanelTest,
                       public ::testing::WithParamInterface<FrameCase> {
 protected:
  void SetUp() override {
    PanelTest::SetUp();
    matrix_.SetModuleType(std::get<0>(GetParam()));
  }
};

std::string FrameCaseName(::testing::TestParamInfo<FrameCase> const& info) {
  return ModuleTypeName(std::get<0>(info.param)) + "_" +
         std::get<1>(info.param).name;
}

INSTANTIATE_TEST_SUITE_P(
    Calls, FrameCountTest,
    ::testing::Combine(::testing::Values(ModuleType::kDr0Cr0Rr1,
                                         ModuleType::kDr1Cr0Rr1),
                       ::testing::ValuesIn(kDrawCalls)),
    FrameCaseName);

TEST_P(FrameCountTest, CallSendsAtMostEightFrames) {
  int32_t const frames_before{mock::frames};

  std::get<1>(GetParam()).draw(matrix_);

  EXPECT_LE(mock::frames - frames_before, 8);
}

TEST_P(MaxMatrixMappingTest, ShiftStepSendsAtMostEightFrames) {
  ASSERT_EQ(matrix_.SetTextWithShift("Hi").GetError(), Error::kOk);
  ShiftSteps(matrix_, 6);
  int32_t const frames_before{mock::frames};

  matrix_.ShiftTask();

  EXPECT_LE(mock::frames - frames_before, 8);
}

/* what reads the picture changes nothing, so a function handed the matrix only
 * to look at it takes it as Matrix const& (FN-11); the getters were not const:
 * the dot at 3, 2 is 04 in its column and 10 in its row of module 0 */
TEST_F(PanelTest, PictureIsReadThroughAConstMatrix) {
  PanelRow row{};
  matrix_.SetDot(Point{X{3}, Y{2}}, true);

  Matrix const& seen{matrix_};

  EXPECT_TRUE(seen.GetDot(Point{X{3}, Y{2}}).GetValue());
  EXPECT_EQ(seen.GetColumn(X{3}).GetValue(), Byte{0x04});
  EXPECT_EQ(seen.GetColumn(Module{0}, 3).GetValue(), Byte{0x04});
  EXPECT_EQ(seen.GetRow(Module{0}, 2).GetValue(), Byte{0x10});
  ASSERT_EQ(seen.GetRow(Y{2}, row).GetError(), Error::kOk);
  EXPECT_EQ(row[0], Byte{0x10});
}

/* a column of a module is the column of the panel that many modules in: column
 * 2 of module 1 is column 10, A3 its rows 0, 1, 5 and 7 */
TEST_F(PanelTest, ColumnOfAModuleIsAColumnOfThePanel) {
  matrix_.SetColumn(Module{1}, 2, Byte{0xA3});

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "..........#.....................",
                                       "..........#.....................",
                                       "................................",
                                       "................................",
                                       "................................",
                                       "..........#.....................",
                                       "................................",
                                       "..........#.....................",
                                   }));
}

TEST_F(PanelTest, ColumnOfAModuleIsReadBack) {
  matrix_.SetColumn(X{10}, Byte{0xA3});

  Result<Byte> const value{matrix_.GetColumn(Module{1}, 2)};

  EXPECT_EQ(value.GetError(), Error::kOk);
  EXPECT_EQ(value.GetValue(), Byte{0xA3});
}

/* Rows and columns are two views of one picture: in orientation 180 SetColumn
 * turned the whole panel while SetRow turned each module. A row of a module has
 * its bit 7 at the left: 81, 97, AD and C3 are #......#, #..#.###, #.#.##.#
 * and ##....## */
TEST_P(MaxMatrixMappingTest, RowIsDrawnAsTheViewerSeesIt) {
  PanelRow const row{{Byte{0x81}, Byte{0x97}, Byte{0xAD}, Byte{0xC3}}};

  matrix_.SetRow(Y{2}, row);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "................................",
                                       "................................",
                                       "#......##..#.####.#.##.###....##",
                                       "................................",
                                       "................................",
                                       "................................",
                                       "................................",
                                       "................................",
                                   }));
}

/* a column has its bit 0 at the top: A3 is rows 0, 1, 5 and 7 */
TEST_P(MaxMatrixMappingTest, ColumnIsDrawnAsTheViewerSeesIt) {
  matrix_.SetColumn(X{5}, Byte{0xA3});

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       ".....#..........................",
                                       ".....#..........................",
                                       "................................",
                                       "................................",
                                       "................................",
                                       ".....#..........................",
                                       "................................",
                                       ".....#..........................",
                                   }));
}

/* column 5 is bit 2 of a row of module 0, as its bit 7 is at the left */
TEST_P(MaxMatrixMappingTest, RowOfAModuleCrossingALitColumnHasItsBit) {
  matrix_.SetColumn(X{5}, Byte{0xA3});

  Result<Byte> const value{matrix_.GetRow(Module{0}, 0)};

  EXPECT_EQ(value.GetError(), Error::kOk);
  EXPECT_EQ(value.GetValue(), Byte{0x04});
}

TEST_P(MaxMatrixMappingTest, RowOfAModuleCrossingADarkDotOfAColumnIsDark) {
  matrix_.SetColumn(X{5}, Byte{0xA3});

  Result<Byte> const value{matrix_.GetRow(Module{0}, 2)};

  EXPECT_EQ(value.GetError(), Error::kOk);
  EXPECT_EQ(value.GetValue(), Byte{0x00});
}

/* A shift and the dots it leaves of 0, 0, of 5, 3 and of 31, 7: it moves the
 * whole picture by one dot as the viewer sees it, across the modules, and fills
 * the edge it leaves as it is told, with nothing, with what fell out at the
 * other edge, or with what the edge held. */
struct ShiftCase {
  char const* name;
  void (*shift)(Matrix&);
  std::array<Point, 3> dots;
};

/* where a shift leaves two dots, the third lies off the panel, where no picture
 * shows it */
constexpr Point kNoDot{X{-1}, Y{-1}};

constexpr std::array<ShiftCase, 12> kShiftCases{{
    {"ShiftLeft",
     [](Matrix& m) { m.ShiftLeft(); },
     {{Point{X{4}, Y{3}}, Point{X{30}, Y{7}}, kNoDot}}},
    {"ShiftLeftRotating",
     [](Matrix& m) { m.ShiftLeft(ShiftFill::kRotate); },
     {{Point{X{4}, Y{3}}, Point{X{30}, Y{7}}, Point{X{31}, Y{0}}}}},
    {"ShiftLeftKeeping",
     [](Matrix& m) { m.ShiftLeft(ShiftFill::kKeep); },
     {{Point{X{4}, Y{3}}, Point{X{30}, Y{7}}, Point{X{31}, Y{7}}}}},
    {"ShiftRight",
     [](Matrix& m) { m.ShiftRight(); },
     {{Point{X{1}, Y{0}}, Point{X{6}, Y{3}}, kNoDot}}},
    {"ShiftRightRotating",
     [](Matrix& m) { m.ShiftRight(ShiftFill::kRotate); },
     {{Point{X{1}, Y{0}}, Point{X{6}, Y{3}}, Point{X{0}, Y{7}}}}},
    {"ShiftRightKeeping",
     [](Matrix& m) { m.ShiftRight(ShiftFill::kKeep); },
     {{Point{X{1}, Y{0}}, Point{X{6}, Y{3}}, Point{X{0}, Y{0}}}}},
    {"ShiftUp",
     [](Matrix& m) { m.ShiftUp(); },
     {{Point{X{5}, Y{2}}, Point{X{31}, Y{6}}, kNoDot}}},
    {"ShiftUpRotating",
     [](Matrix& m) { m.ShiftUp(ShiftFill::kRotate); },
     {{Point{X{5}, Y{2}}, Point{X{31}, Y{6}}, Point{X{0}, Y{7}}}}},
    {"ShiftUpKeeping",
     [](Matrix& m) { m.ShiftUp(ShiftFill::kKeep); },
     {{Point{X{5}, Y{2}}, Point{X{31}, Y{6}}, Point{X{31}, Y{7}}}}},
    {"ShiftDown",
     [](Matrix& m) { m.ShiftDown(); },
     {{Point{X{0}, Y{1}}, Point{X{5}, Y{4}}, kNoDot}}},
    {"ShiftDownRotating",
     [](Matrix& m) { m.ShiftDown(ShiftFill::kRotate); },
     {{Point{X{0}, Y{1}}, Point{X{5}, Y{4}}, Point{X{31}, Y{0}}}}},
    {"ShiftDownKeeping",
     [](Matrix& m) { m.ShiftDown(ShiftFill::kKeep); },
     {{Point{X{0}, Y{1}}, Point{X{5}, Y{4}}, Point{X{0}, Y{0}}}}},
}};

using ShiftParameter = std::tuple<MatrixOrientation, ModuleType, ShiftCase>;

class ShiftTest : public PanelTest,
                  public ::testing::WithParamInterface<ShiftParameter> {
 protected:
  void SetUp() override {
    PanelTest::SetUp();
    matrix_.SetMatrixOrientation(std::get<0>(GetParam()));
    matrix_.SetModuleType(std::get<1>(GetParam()));
  }

  MAXMATRIX_NODISCARD static std::vector<std::string> Expected() {
    std::array<Point, 3> const& dots{std::get<2>(GetParam()).dots};
    return Dots({dots[0], dots[1], dots[2]});
  }
};

std::string ShiftName(::testing::TestParamInfo<ShiftParameter> const& info) {
  return MappingName(std::get<0>(info.param), std::get<1>(info.param)) + "_" +
         std::get<2>(info.param).name;
}

INSTANTIATE_TEST_SUITE_P(
    Shifts, ShiftTest,
    ::testing::Combine(::testing::ValuesIn(kMatrixOrientations),
                       ::testing::ValuesIn(kModuleTypes),
                       ::testing::ValuesIn(kShiftCases)),
    ShiftName);

TEST_P(ShiftTest, MovesThePicture) {
  matrix_.SetDot(Point{X{0}, Y{0}}, true);
  matrix_.SetDot(Point{X{5}, Y{3}}, true);
  matrix_.SetDot(Point{X{31}, Y{7}}, true);

  std::get<2>(GetParam()).shift(matrix_);

  EXPECT_EQ(ShownPicture(matrix_), Expected());
}

/* A column given to ShiftLeft() or ShiftRight() comes in at the edge the shift
 * leaves, and the one that falls out at the other edge comes back, both bit 0
 * at the top; a literal is a column, not a fill. 5A is rows 1, 3, 4 and 6, A5
 * rows 0, 2, 5 and 7. */
TEST_P(MaxMatrixMappingTest, ShiftLeftGivesTheLeftColumnOut) {
  matrix_.SetColumn(X{0}, Byte{0x5A});

  Byte const out{matrix_.ShiftLeft(Byte{0xA5})};

  EXPECT_EQ(out, Byte{0x5A});
}

TEST_P(MaxMatrixMappingTest, ShiftLeftTakesTheColumnInAtTheRight) {
  matrix_.SetDot(Point{X{5}, Y{3}}, true);

  matrix_.ShiftLeft(Byte{0xA5});

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "...............................#",
                                       "................................",
                                       "...............................#",
                                       "....#...........................",
                                       "................................",
                                       "...............................#",
                                       "................................",
                                       "...............................#",
                                   }));
}

TEST_P(MaxMatrixMappingTest, ShiftRightGivesTheRightColumnOut) {
  matrix_.SetColumn(X{31}, Byte{0x5A});

  Byte const out{matrix_.ShiftRight(Byte{0xA5})};

  EXPECT_EQ(out, Byte{0x5A});
}

TEST_P(MaxMatrixMappingTest, ShiftRightTakesTheColumnInAtTheLeft) {
  matrix_.SetDot(Point{X{5}, Y{3}}, true);

  matrix_.ShiftRight(Byte{0xA5});

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "#...............................",
                                       "................................",
                                       "#...............................",
                                       "......#.........................",
                                       "................................",
                                       "#...............................",
                                       "................................",
                                       "#...............................",
                                   }));
}

/* shifts each of `columns` in at the right and returns what came out at the
 * left, in their order */
std::vector<uint8_t> ShiftInAtTheRight(Matrix& matrix,
                                       std::vector<uint8_t> const& columns) {
  std::vector<uint8_t> out;
  out.reserve(columns.size());
  for (uint8_t const column : columns) {
    out.push_back(matrix.ShiftLeft(Byte{column}).ToInteger());
  }
  return out;
}

/* Columns fed one by one make a band longer than the panel, and come out at the
 * other edge in the order they went in: what a sketch passes on to a second
 * matrix. The band is 32 columns, 01 to 20, and three more. */
std::vector<uint8_t> const kBand{
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
    0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20};

TEST_F(PanelTest, ColumnsShiftedInStandInTheirOrder) {
  ShiftInAtTheRight(matrix_, kBand);

  EXPECT_EQ(LeftEdge(matrix_, 32), kBand);
}

TEST_F(PanelTest, ColumnsShiftedInPushTheEmptyPanelOut) {
  std::vector<uint8_t> const out{ShiftInAtTheRight(matrix_, kBand)};

  EXPECT_EQ(out, std::vector<uint8_t>(32));
}

TEST_F(PanelTest, ColumnsShiftedOnComeOutInTheirOrder) {
  ShiftInAtTheRight(matrix_, kBand);

  std::vector<uint8_t> const out{
      ShiftInAtTheRight(matrix_, std::vector<uint8_t>{0x21, 0x22, 0x23})};

  EXPECT_EQ(out, (std::vector<uint8_t>{0x01, 0x02, 0x03}));
}

/* every dot set is read back as it was set, on a pattern with no symmetry a
 * wrong mapping could hide behind */
TEST_P(MaxMatrixMappingTest, DotsRoundTrip) {
  DrawPattern(matrix_);

  EXPECT_EQ(ShownPicture(matrix_), Picture(Pattern));
}

/* Every module type shows the picture upright on the wiring of the model it is
 * named after, the matrix orientation 180 on a board mounted upside down, as
 * the README tells a user. Four of the eight are mirror images of the others,
 * which the module orientation of 0 to 270 degrees did not reach (OIL item 9);
 * the named four have to be the wirings MD_MAX72XX gives those names. */
using ModuleCase = std::tuple<ModuleType, bool>;

class PanelModuleTest : public PanelTest,
                        public ::testing::WithParamInterface<ModuleCase> {};

std::string ModuleName(::testing::TestParamInfo<ModuleCase> const& info) {
  return ModuleTypeName(std::get<0>(info.param)) +
         (std::get<1>(info.param) ? "_upside_down" : "");
}

INSTANTIATE_TEST_SUITE_P(Modules, PanelModuleTest,
                         ::testing::Combine(::testing::ValuesIn(kModuleTypes),
                                            ::testing::Bool()),
                         ModuleName);

TEST_F(PanelTest, NamedModuleTypesAreTheirWirings) {
  EXPECT_EQ(ModuleTypeName(ModuleType::kFc16), kMax7219WiringFc16.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kGeneric), kMax7219WiringGeneric.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kParola), kMax7219WiringParola.name);
  EXPECT_EQ(ModuleTypeName(ModuleType::kIcstation),
            kMax7219WiringIcstation.name);
}

/* the LEDs as the viewer sees them on `wiring`, mounted upside down or not */
std::vector<std::string> LitPicture(Max7219Wiring const& wiring,
                                    bool upside_down) {
  return Picture([&wiring, upside_down](int16_t x, int16_t y) {
    return mock::chain.IsLit(wiring, static_cast<uint16_t>(x),
                             static_cast<uint8_t>(y), upside_down);
  });
}

/* a dot set at X, Y is lit at X, Y of the panel; the pattern has no symmetry a
 * wrong mapping could hide behind */
TEST_P(PanelModuleTest, DotsAppearWhereTheyAreSet) {
  ModuleType const module_type{std::get<0>(GetParam())};
  bool const upside_down{std::get<1>(GetParam())};
  matrix_.SetMatrixOrientation(upside_down ? MatrixOrientation::k180
                                           : MatrixOrientation::k0);
  matrix_.SetModuleType(module_type);

  DrawPattern(matrix_);

  EXPECT_EQ(LitPicture(kMax7219Wirings[static_cast<uint8_t>(module_type)],
                       upside_down),
            Picture(Pattern));
}

/* Drawing off the panel leaves the panel as it was, sends nothing and reports
 * nothing; column 8 of module 0 used to be checked against the panel alone,
 * which would have drawn it on module 1. */
struct OffPanel {
  char const* name;
  void (*draw)(Matrix&);
};

constexpr std::array<OffPanel, 12> kOffPanel{{
    {"SetDotLeft", [](Matrix& m) { m.SetDot(Point{X{-1}, Y{0}}, true); }},
    {"SetDotRight", [](Matrix& m) { m.SetDot(Point{X{32}, Y{0}}, true); }},
    {"SetDotAbove", [](Matrix& m) { m.SetDot(Point{X{0}, Y{-1}}, true); }},
    {"SetDotBelow", [](Matrix& m) { m.SetDot(Point{X{0}, Y{8}}, true); }},
    {"SetColumnLeft", [](Matrix& m) { m.SetColumn(X{-1}, Byte{0xFF}); }},
    {"SetColumnRight", [](Matrix& m) { m.SetColumn(X{32}, Byte{0xFF}); }},
    {"SetColumnOfNoModule",
     [](Matrix& m) { m.SetColumn(Module{4}, 0, Byte{0xFF}); }},
    {"SetColumnOffItsModule",
     [](Matrix& m) { m.SetColumn(Module{0}, 8, Byte{0xFF}); }},
    {"SetRowAbove", [](Matrix& m) { m.SetRow(Y{-1}, kRow); }},
    {"SetRowBelow", [](Matrix& m) { m.SetRow(Y{8}, kRow); }},
    {"SetRowOfNoModule", [](Matrix& m) { m.SetRow(Module{4}, 0, Byte{0xFF}); }},
    {"SetRowOffItsModule",
     [](Matrix& m) { m.SetRow(Module{0}, 8, Byte{0xFF}); }},
}};

class OffPanelTest : public PanelTest,
                     public ::testing::WithParamInterface<OffPanel> {};

INSTANTIATE_TEST_SUITE_P(Calls, OffPanelTest, ::testing::ValuesIn(kOffPanel),
                         CaseName<OffPanel>);

TEST_P(OffPanelTest, LeavesThePanelDark) {
  GetParam().draw(matrix_);

  EXPECT_EQ(Display(), std::vector<uint8_t>(kNumberOfColumns));
}

TEST_P(OffPanelTest, SendsNothing) {
  int32_t const frames_before{mock::frames};

  GetParam().draw(matrix_);

  EXPECT_EQ(mock::frames, frames_before);
}

TEST_P(OffPanelTest, ReportsNothing) {
  GetParam().draw(matrix_);

  EXPECT_TRUE(reports.empty());
}

/* A char partly off the panel shows the part on it, through either path of
 * SetSprite(): whole columns for a char at the top, single dots for one moved
 * down. The A is 7E 11 11 7E. */
TEST_P(MaxMatrixMappingTest, CharOffTheLeftEdgeShowsItsRightColumns) {
  ASSERT_EQ(matrix_.SetChar(Point{X{-2}, Y{0}}, 'A').GetError(), Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 3), (std::vector<uint8_t>{0x11, 0x7E, 0x00}));
}

TEST_P(MaxMatrixMappingTest, CharOffTheRightEdgeShowsItsLeftColumns) {
  ASSERT_EQ(matrix_.SetChar(Point{X{30}, Y{0}}, 'A').GetError(), Error::kOk);

  EXPECT_EQ(RightEdge(matrix_, 3), (std::vector<uint8_t>{0x00, 0x7E, 0x11}));
}

/* three rows up, 11 7E are 02 0F */
TEST_P(MaxMatrixMappingTest, CharOffTheTopLeftCornerShowsItsLowerRight) {
  ASSERT_EQ(matrix_.SetChar(Point{X{-2}, Y{-3}}, 'A').GetError(), Error::kOk);

  EXPECT_EQ(LeftEdge(matrix_, 3), (std::vector<uint8_t>{0x02, 0x0F, 0x00}));
}

/* five rows down, 7E 11 are C0 20 */
TEST_P(MaxMatrixMappingTest, CharOffTheBottomRightCornerShowsItsUpperLeft) {
  ASSERT_EQ(matrix_.SetChar(Point{X{30}, Y{5}}, 'A').GetError(), Error::kOk);

  EXPECT_EQ(RightEdge(matrix_, 3), (std::vector<uint8_t>{0x00, 0xC0, 0x20}));
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

/* a line, its name and its ends */
struct LineCase {
  char const* name;
  Line line;
};

/* A line in each of the eight directions, and lines whose ends lie off the
 * panel, as far as an int16_t reaches: the sum of their differences does not
 * fit in an int, which has 16 bits on the Uno. Each is checked against the
 * ideal line rather than a picture written down: a dot per column or row along
 * its longer axis, at most half a dot from it, and none beyond its ends. */
constexpr std::array<LineCase, 13> kLines{{
    {"Flat", {2, 3, 20, 3}},
    {"Upright", {7, 0, 7, 7}},
    {"Diagonal", {0, 0, 7, 7}},
    {"AcrossThePanel", {0, 0, 31, 7}},
    {"Steep", {3, 1, 9, 6}},
    {"SteepBackwards", {30, 0, 26, 7}},
    {"RisingAcrossThePanel", {0, 7, 31, 0}},
    {"Dot", {12, 4, 12, 4}},
    {"EndsOffThePanel", {-40, -10, 60, 17}},
    {"SteepEndsOffThePanel", {5, -20, 9, 30}},
    {"FlatAsFarAsInt16Reaches", {-32768, 0, 32767, 7}},
    {"DiagonalAsFarAsInt16Reaches", {-32768, -32768, 32767, 32767}},
    {"SteepAsFarAsInt16Reaches", {16, -32768, 17, 32767}},
}};

/* a line, and whether it is drawn from its second end to its first */
using LineParameter = std::tuple<LineCase, bool>;

class LineTest : public PanelTest,
                 public ::testing::WithParamInterface<LineParameter> {};

std::string LineName(::testing::TestParamInfo<LineParameter> const& info) {
  std::string const name{std::get<0>(info.param).name};
  return name + (std::get<1>(info.param) ? "_reversed" : "");
}

INSTANTIATE_TEST_SUITE_P(Lines, LineTest,
                         ::testing::Combine(::testing::ValuesIn(kLines),
                                            ::testing::Bool()),
                         LineName);

TEST_P(LineTest, RunsFromEndToEnd) {
  Line const& line{std::get<0>(GetParam()).line};
  Point const first{X{line.x0}, Y{line.y0}};
  Point const second{X{line.x1}, Y{line.y1}};
  bool const reversed{std::get<1>(GetParam())};

  matrix_.DrawLine(reversed ? second : first, reversed ? first : second, true);

  ExpectLine(matrix_, line);
}

TEST_F(PanelTest, LineDrawnDarkTurnsItsDotsOff) {
  Point const far_left{X{-32768}, Y{0}};
  Point const far_right{X{32767}, Y{7}};
  matrix_.DrawLine(far_left, far_right, true);

  matrix_.DrawLine(far_left, far_right, false);

  EXPECT_EQ(ShownPicture(matrix_), Dots({}));
}

/* a rectangle by its sides */
struct Rectangle {
  char const* name;
  int16_t left, top, right, bottom;
};

/* A rectangle on the panel, one partly off it, one of a column, a row or a
 * dot, and one as large as an int16_t allows. Each is checked against what it
 * is rather than a picture written down: the dots between its sides, or on
 * them. */
constexpr std::array<Rectangle, 9> kRectangles{{
    {"OnThePanel", 2, 1, 6, 4},
    {"Column", 10, 0, 10, 7},
    {"Row", 20, 6, 23, 6},
    {"Dot", 12, 4, 12, 4},
    {"OffTheTopLeft", -3, -2, 2, 2},
    {"OffTheBottomRight", 29, 5, 38, 14},
    {"AsLargeAsInt16Allows", -32768, -32768, 32767, 32767},
    {"FromTheCornerToTheEnd", 0, 0, 32767, 32767},
    {"ColumnAtTheEnd", 32767, 0, 32767, 7},
}};

/* which pair of opposite corners a rectangle is given by, in which order */
enum class Corners : uint8_t {
  kTopLeftFirst,
  kBottomRightFirst,
  kTopRightFirst,
  kBottomLeftFirst
};

constexpr std::array<Corners, 4> kCorners{
    {Corners::kTopLeftFirst, Corners::kBottomRightFirst,
     Corners::kTopRightFirst, Corners::kBottomLeftFirst}};

using RectangleParameter = std::tuple<Rectangle, Corners>;

class RectangleTest : public PanelTest,
                      public ::testing::WithParamInterface<RectangleParameter> {
 protected:
  /* the two corners of the rectangle in the order of the parameter */
  static std::pair<Point, Point> GetCorners() {
    Rectangle const& r{std::get<0>(GetParam())};
    Point const top_left{X{r.left}, Y{r.top}};
    Point const bottom_right{X{r.right}, Y{r.bottom}};
    Point const top_right{X{r.right}, Y{r.top}};
    Point const bottom_left{X{r.left}, Y{r.bottom}};
    std::array<std::pair<Point, Point>, 4> const pairs{{
        {top_left, bottom_right},
        {bottom_right, top_left},
        {top_right, bottom_left},
        {bottom_left, top_right},
    }};
    return pairs[static_cast<size_t>(std::get<1>(GetParam()))];
  }

  /* whether a dot lies between the sides of the rectangle */
  static bool Inside(int16_t x, int16_t y) {
    Rectangle const& r{std::get<0>(GetParam())};
    return (x >= r.left) && (x <= r.right) && (y >= r.top) && (y <= r.bottom);
  }

  /* whether a dot lies on a side of the rectangle */
  static bool OnASide(int16_t x, int16_t y) {
    Rectangle const& r{std::get<0>(GetParam())};
    bool const on_a_side{(x == r.left) || (x == r.right) || (y == r.top) ||
                         (y == r.bottom)};
    return Inside(x, y) && on_a_side;
  }
};

std::string RectangleName(
    ::testing::TestParamInfo<RectangleParameter> const& info) {
  std::array<char const*, 4> const corners{
      {"TopLeftFirst", "BottomRightFirst", "TopRightFirst", "BottomLeftFirst"}};
  std::string const name{std::get<0>(info.param).name};
  return name + "_" + corners[static_cast<size_t>(std::get<1>(info.param))];
}

INSTANTIATE_TEST_SUITE_P(Rectangles, RectangleTest,
                         ::testing::Combine(::testing::ValuesIn(kRectangles),
                                            ::testing::ValuesIn(kCorners)),
                         RectangleName);

TEST_P(RectangleTest, FillRectCoversItsArea) {
  std::pair<Point, Point> const corners{GetCorners()};

  matrix_.FillRect(corners.first, corners.second, true);

  EXPECT_EQ(ShownPicture(matrix_), Picture(Inside));
}

TEST_P(RectangleTest, DrawRectDrawsItsSides) {
  std::pair<Point, Point> const corners{GetCorners()};

  matrix_.DrawRect(corners.first, corners.second, true);

  EXPECT_EQ(ShownPicture(matrix_), Picture(OnASide));
}

TEST_F(PanelTest, FillRectOfFalseTurnsItsAreaOff) {
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.FillRect(Point{X{1}, Y{1}}, Point{X{3}, Y{2}}, false);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "################################",
                                       "#...############################",
                                       "#...############################",
                                       "################################",
                                       "################################",
                                       "################################",
                                       "################################",
                                       "################################",
                                   }));
}

TEST_F(PanelTest, DrawRectOfFalseTurnsItsSidesOff) {
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.DrawRect(Point{X{5}, Y{3}}, Point{X{7}, Y{5}}, false);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "################################",
                                       "################################",
                                       "################################",
                                       "#####...########################",
                                       "#####.#.########################",
                                       "#####...########################",
                                       "################################",
                                       "################################",
                                   }));
}

/* the outline the midpoint circle gives for a radius of 3, round and closed */
TEST_F(PanelTest, DrawCircleDrawsItsOutline) {
  matrix_.DrawCircle(Point{X{10}, Y{3}}, 3, true);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       ".........###....................",
                                       "........#...#...................",
                                       ".......#.....#..................",
                                       ".......#.....#..................",
                                       ".......#.....#..................",
                                       "........#...#...................",
                                       ".........###....................",
                                       "................................",
                                   }));
}

TEST_F(PanelTest, CircleAcrossTheCornerShowsTheQuarterOnThePanel) {
  matrix_.DrawCircle(Point{X{0}, Y{0}}, 3, true);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "...#............................",
                                       "...#............................",
                                       "..#.............................",
                                       "##..............................",
                                       "................................",
                                       "................................",
                                       "................................",
                                       "................................",
                                   }));
}

TEST_F(PanelTest, CircleOfRadius0IsADot) {
  matrix_.DrawCircle(Point{X{20}, Y{4}}, 0, true);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "................................",
                                       "................................",
                                       "................................",
                                       "................................",
                                       "....................#...........",
                                       "................................",
                                       "................................",
                                       "................................",
                                   }));
}

TEST_F(PanelTest, CircleOfRadius1IsACross) {
  matrix_.DrawCircle(Point{X{25}, Y{4}}, 1, true);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "................................",
                                       "................................",
                                       "................................",
                                       ".........................#......",
                                       "........................#.#.....",
                                       ".........................#......",
                                       "................................",
                                       "................................",
                                   }));
}

TEST_F(PanelTest, CircleOfANegativeRadiusDrawsNothing) {
  matrix_.DrawCircle(Point{X{28}, Y{4}}, -1, true);

  EXPECT_EQ(ShownPicture(matrix_), Dots({}));
}

/* the sums of a circle of the largest radius do not fit in an int, which has
 * 16 bits on the Uno */
TEST_F(PanelTest, CircleOfTheLargestRadiusAroundThePanelDrawsNothing) {
  matrix_.DrawCircle(Point{X{16}, Y{3}}, 32767, true);

  EXPECT_EQ(ShownPicture(matrix_), Dots({}));
}

TEST_F(PanelTest, CircleFarBeyondThePanelDrawsNothing) {
  matrix_.DrawCircle(Point{X{-32768}, Y{-32768}}, 32767, true);

  EXPECT_EQ(ShownPicture(matrix_), Dots({}));
}

/* a bitmap is drawn column by column from flash, bit 0 at the top: 01 82 FF 00
 * 3C */
TEST_F(PanelTest, DrawBitmapDrawsItsColumns) {
  matrix_.DrawBitmap(Point{X{3}, Y{0}}, kBitmap);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "...#.#..........................",
                                       "....##..........................",
                                       ".....#.#........................",
                                       ".....#.#........................",
                                       ".....#.#........................",
                                       ".....#.#........................",
                                       ".....#..........................",
                                       "....##..........................",
                                   }));
}

/* off the left edge, three rows down: FF 00 3C of its columns, of which the
 * rows on the panel */
TEST_F(PanelTest, BitmapOffTheLeftEdgeShowsThePartOnThePanel) {
  matrix_.DrawBitmap(Point{X{-2}, Y{3}}, kBitmap);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "................................",
                                       "................................",
                                       "................................",
                                       "#...............................",
                                       "#...............................",
                                       "#.#.............................",
                                       "#.#.............................",
                                       "#.#.............................",
                                   }));
}

/* off the right edge, a row up: 01 82 FF of its columns */
TEST_F(PanelTest, BitmapOffTheRightEdgeShowsThePartOnThePanel) {
  matrix_.DrawBitmap(Point{X{29}, Y{-1}}, kBitmap);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "..............................##",
                                       "...............................#",
                                       "...............................#",
                                       "...............................#",
                                       "...............................#",
                                       "...............................#",
                                       "..............................##",
                                       "................................",
                                   }));
}

/* three rows high over a lit panel: the rows 0 to 2 of its columns, 1 0 0,
 * 0 1 0, 1 1 1, 0 0 0 and 0 0 1, and the rest of the panel as it was */
TEST_F(PanelTest, BitmapOfThreeRowsLeavesTheRowsBelowIt) {
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.DrawBitmap(Point{X{10}, Y{2}}, 3, kBitmap);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "################################",
                                       "################################",
                                       "###########.#..#################",
                                       "##########.##..#################",
                                       "##########..#.##################",
                                       "################################",
                                       "################################",
                                       "################################",
                                   }));
}

/* a bitmap far off the panel, where the sum of its place and its size does not
 * fit in an int, which has 16 bits on the Uno */
struct FarBitmap {
  char const* name;
  void (*draw)(Matrix&);
};

constexpr std::array<FarBitmap, 4> kFarBitmaps{{
    {"Right", [](Matrix& m) { m.DrawBitmap(Point{X{32767}, Y{0}}, kBitmap); }},
    {"Left", [](Matrix& m) { m.DrawBitmap(Point{X{-32768}, Y{0}}, kBitmap); }},
    {"Below",
     [](Matrix& m) { m.DrawBitmap(Point{X{0}, Y{32767}}, 8, kBitmap); }},
    {"Above",
     [](Matrix& m) { m.DrawBitmap(Point{X{0}, Y{-32768}}, 8, kBitmap); }},
}};

class FarBitmapTest : public PanelTest,
                      public ::testing::WithParamInterface<FarBitmap> {};

INSTANTIATE_TEST_SUITE_P(Places, FarBitmapTest,
                         ::testing::ValuesIn(kFarBitmaps), CaseName<FarBitmap>);

TEST_P(FarBitmapTest, DrawsNothing) {
  GetParam().draw(matrix_);

  EXPECT_EQ(ShownPicture(matrix_), Dots({}));
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
TEST_F(PanelTest, DrawBitmapTakesNoWidthAfterTheBitmap) {
  static_assert(!TakesAWidthAfterTheBitmap<Matrix>::value,
                "a width after the bitmap would be read as its height");
}

/* a bitmap less than a module high at the top row leaves the rows below it,
 * where a whole column at once would have turned them off */
TEST_F(PanelTest, BitmapAtTheTopLeavesTheRowsBelowIt) {
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.DrawBitmap(Point{X{10}, Y{0}}, 3, kBitmap);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       "###########.#..#################",
                                       "##########.##..#################",
                                       "##########..#.##################",
                                       "################################",
                                       "################################",
                                       "################################",
                                       "################################",
                                       "################################",
                                   }));
}

/* a bitmap higher than a module read bits beyond its byte, which turned the
 * dots below its eighth row off or, from bit 32 on, shifted further than C++
 * defines; it is drawn eight rows high, and the rows below stay as they are */
class BitmapHeightTest : public PanelTest,
                         public ::testing::WithParamInterface<uint8_t> {};

TEST_P(BitmapHeightTest, BitmapHigherThanAModuleIsDrawnEightHigh) {
  constexpr auto kTopDot PROGMEM{make_array<uint8_t>(0x01)};
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.DrawBitmap(Point{X{0}, Y{-1}}, GetParam(), kTopDot);

  EXPECT_EQ(ShownPicture(matrix_), (std::vector<std::string>{
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       ".###############################",
                                       "################################",
                                   }));
}

std::string HeightName(::testing::TestParamInfo<uint8_t> const& info) {
  return "Height" + std::to_string(info.param);
}

INSTANTIATE_TEST_SUITE_P(Heights, BitmapHeightTest,
                         ::testing::Values(uint8_t{9}, uint8_t{255}),
                         HeightName);

TEST_F(PanelTest, BitmapOfNoRowDrawsNothing) {
  constexpr auto kDarkColumn PROGMEM{make_array<uint8_t>(0x00)};
  matrix_.FillRect(Point{X{0}, Y{0}}, Point{X{31}, Y{7}}, true);

  matrix_.DrawBitmap(Point{X{0}, Y{0}}, 0, kDarkColumn);

  EXPECT_EQ(ShownPicture(matrix_),
            Picture([](int16_t, int16_t) { return true; }));
}

TEST_F(PanelTest, GetRowOffThePanelLeavesTheValues) {
  PanelRow row{{Byte{0xA5}, Byte{0xA5}, Byte{0xA5}, Byte{0xA5}}};

  Error const error{matrix_.GetRow(Y{kRowNumberOfModule}, row).GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(Bytes(row), (std::vector<uint8_t>{0xA5, 0xA5, 0xA5, 0xA5}));
}

TEST_F(PanelTest, GetRowIntoTooFewBytesLeavesThem) {
  array<Byte, kNumberOfModules - 1> row{{Byte{0xA5}, Byte{0xA5}, Byte{0xA5}}};

  Error const error{matrix_.GetRow(Y{0}, row).GetError()};

  EXPECT_EQ(error, Error::kOutOfRange);
  EXPECT_EQ(Bytes(row), (std::vector<uint8_t>{0xA5, 0xA5, 0xA5}));
}

TEST_F(PanelTest, CoordinatesAreMadeWhenCompiling) {
  static_assert(X{3}.number == 3, "a column");
  static_assert(Y{-2}.number == -2, "a row");
  static_assert(Point{X{3}, Y{2}}.y.number == 2, "a dot");
  static_assert(Module{1}.number == 1, "a module");

  EXPECT_EQ((Point{X{3}, Y{2}}).x.number, 3);
}

}  // namespace
}  // namespace max_matrix
