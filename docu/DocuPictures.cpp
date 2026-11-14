// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief Draws the pictures in docu/ from what the library does.
 *
 * `build/DocuPictures --write docu` writes them; `build/DocuPictures --check
 * docu` fails if one in docu/ is not what it would write.
 *
 * Nothing in the pictures is drawn by hand. Every LED is placed by calling the
 * library, with SetDot(), SetColumn() or SetRow(), and reading back which
 * register bit of which MAX7219 the model in test/model/ latched, or which dot
 * GetDot() reports. The module types in the titles are the wirings of the model
 * on which a pattern set with that module type appears upright. So a picture
 * cannot say something the library does not do, and the test docu/pictures
 * fails when the library changes and the pictures were not written again.
 */

#include <string.h>

#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"
#include "DocuSvg.h"
#include "MaxMatrix.h"

namespace {

/* layout, in pixels */
constexpr int32_t kCell{28};
constexpr int32_t kLed{22};
constexpr int32_t kModuleGap{28};
constexpr int32_t kLeft{60};
constexpr int32_t kTop{86};
constexpr int32_t kRight{120};
constexpr int32_t kBottom{104};

/* where a text or a shape lies from what it belongs to */
constexpr int32_t kTitleBaseline{30};    // from the top of the picture
constexpr int32_t kFramePadding{4};      // a module's frame around its cells
constexpr int32_t kModuleNameAbove{14};  // the module's name above its cells
constexpr int32_t kArrowLength{70};      // the arrow into the module of DIN
constexpr int32_t kArrowThickness{3};
constexpr int32_t kDinAbove{10};      // DIN above its arrow
constexpr int32_t kBaselineShift{4};  // a text below the middle of its cell
constexpr int32_t kAxisBelow{22};     // the columns' numbers below the panel
constexpr int32_t kAxisLeft{14};      // the rows' numbers left of the panel
constexpr int32_t kAxisNameLeft{40};  // Y left of the panel
constexpr int32_t kAxisNameShift{5};  // Y below the middle of the panel
constexpr int32_t kLegendBelow{56};   // the legend below the last cell
constexpr int32_t kLegendLine{22};    // between the two lines of the legend
constexpr int32_t kNamesBelow{46};    // the functions' names below the panel
constexpr uint8_t kLedRadius{3};
constexpr uint8_t kFrameRadius{4};

/* the coordinate picture shows the panel only, from just above the module names
 * to just below the function names */
constexpr int32_t kCoordinatesAbove{40};
constexpr int32_t kCoordinatesBelow{60};

/* font sizes */
constexpr uint8_t kFontTitle{20};
constexpr uint8_t kFontModule{15};
constexpr uint8_t kFontAxis{13};
constexpr uint8_t kFontLed{12};
constexpr uint8_t kFontLegend{15};

/* colours */
constexpr char kColourText[] = "#222222";
constexpr char kColourFrame[] = "#666666";
constexpr char kColourLed[] = "#e53935";
constexpr char kColourLedBit0[] = "#a31515";
constexpr char kColourLedOff[] = "#d6d6d6";
constexpr char kColourBand[] = "#bbdefb";
constexpr char kColourRow[] = "#1e88e5";

/* the texts of a picture */
constexpr DocuFont kTitleText{kFontTitle, kColourText, "start", true};
constexpr DocuFont kNameText{kFontModule, kColourText, "middle", true};
constexpr DocuFont kDinText{kFontModule, kColourText, "middle", false};
constexpr DocuFont kColumnAxisText{kFontAxis, kColourText, "middle", false};
constexpr DocuFont kRowAxisText{kFontAxis, kColourText, "end", false};
constexpr DocuFont kXText{kFontModule, kColourText, "end", true};
constexpr DocuFont kLedText{kFontLed, "#ffffff", "middle", true};
constexpr DocuFont kLegendText{kFontLegend, kColourText, "start", false};
constexpr DocuFont kColumnFunctionText{kFontModule, kColourLed, "middle", true};
constexpr DocuFont kRowFunctionText{kFontModule, kColourRow, "middle", true};

/* a digit register of the MAX7219 drives eight segments, a bit each */
constexpr uint8_t kBitsPerDigit{8};

/* the pins the examples use; the mock needs chip select to be the one the
 * library drives */
constexpr uint8_t kDataInPin{11};
constexpr uint8_t kChipSelectPin{10};
constexpr uint8_t kClockPin{13};
/* the chain of the examples, four modules, which the pictures show */
constexpr uint8_t kNumberOfModules{4};
constexpr uint8_t kNumberOfColumns{kNumberOfModules *
                                   max_matrix::kColumnNumberOfModule};

/* what the coordinate picture shows SetColumn() and SetRow() with */
constexpr uint8_t kExampleColumn{5};
constexpr uint8_t kExampleRowModule{2};
constexpr uint8_t kExampleRow{2};

/*!
 * \brief Where the LED of one dot is driven: the chip in the chain, 0 at DIN,
 *        its digit register and the bit in it.
 */
struct DocuLed {
  uint8_t chip;
  uint8_t digit;
  uint8_t bit;
};

/*! \brief A dot of the panel, as the viewer sees it. */
struct Dot {
  uint8_t x;
  uint8_t y;
};

// the matrix, as a sketch has it, which every picture draws on
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
max_matrix::MatrixOf<kNumberOfModules> matrix{kDataInPin, kChipSelectPin,
                                              kClockPin};

/* the names MD_MAX72XX gives four of the eight wirings, as the README uses them
 */
struct ModuleName {
  Max7219Wiring const* wiring;
  char const* name;
};

constexpr std::array<ModuleName, 4> kModuleNames{{
    {&kMaX7219WiringFC16, "FC-16"},
    {&kMaX7219WiringGeneric, "Generic"},
    {&kMaX7219WiringParola, "Parola"},
    {&kMaX7219WiringIcstation, "ICStation"},
}};

/* something for every dot of the panel, as [x][y] */
template <typename Type>
using PerDot = std::array<std::array<Type, max_matrix::kRowNumberOfModule>,
                          kNumberOfColumns>;

/*!
 * \brief Counts the lit LEDs of one digit register, and keeps the last of them.
 *
 * \param[in] chip The chip in the chain, 0 at DIN.
 * \param[in] digit The digit register.
 * \param[in,out] led The chip, digit and bit of the last lit LED found.
 * \return How many of its LEDs are lit.
 */
uint8_t CountLit(uint8_t chip, uint8_t digit, DocuLed* led) {
  uint8_t const segments{mock::chain.chips[chip].digit[digit]};
  uint8_t lit{0};

  for (uint8_t bit{0}; bit < kBitsPerDigit; bit++) {
    if (bitRead(segments, bit)) {
      *led = DocuLed{chip, digit, bit};
      lit++;
    }
  }
  return lit;
}

/*!
 * \brief Finds the one LED the model has lit, in the digit registers of every
 *        chip.
 *
 * \param[out] led The chip, digit and bit of the lit LED.
 * \return True if exactly one LED is lit, or false if none or more than one is.
 */
bool FindLed(DocuLed* led) {
  uint8_t lit{0};

  for (uint8_t chip{0}; chip < mock::chain.chips.size(); chip++) {
    for (uint8_t digit{0}; digit < Max7219::kNumberOfDigits; digit++) {
      lit = static_cast<uint8_t>(lit + CountLit(chip, digit, led));
    }
  }
  return lit == 1;
}

/*!
 * \brief Finds the LED that drives a dot as the viewer sees it.
 *
 * It clears the matrix, sets the one dot with SetDot() and asks the model which
 * LED it lit.
 *
 * \param[in] x The column, as the viewer sees it.
 * \param[in] y The row, as the viewer sees it.
 * \param[out] led The chip, digit and bit.
 * \return True if the dot lit exactly one LED, or false if it did not.
 */
bool FindLedOfDot(uint8_t x, uint8_t y, DocuLed* led) {
  matrix.Clear();
  matrix.SetDot(x, y, true);
  return FindLed(led);
}

/*!
 * \brief Returns whether a module of a wiring shows the panel upright.
 *
 * It sets a pattern without symmetry dot by dot and compares it with what the
 * model lights for that wiring, as MaxMatrixModuleTest does.
 *
 * \param[in] wiring The wiring of the modules.
 * \return True if every dot is lit where it was set.
 */
bool IsUpright(Max7219Wiring const& wiring) {
  bool upright{true};

  matrix.Clear();
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      matrix.SetDot(x, y, ((x * x) + (3 * y) + (x * y)) % 3 == 0);
    }
  }
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      if (mock::chain.IsLit(wiring, x, y) !=
          (((x * x) + (3 * y) + (x * y)) % 3 == 0)) {
        upright = false;
      }
    }
  }
  return upright;
}

/*!
 * \brief Returns the modules the current module type fits.
 *
 * It names every wiring of the model on which the panel is upright, with the
 * name MD_MAX72XX gives it where there is one.
 *
 * \return The names, separated by commas, or `"no module"`.
 */
std::string GetModules() {
  std::string modules;

  for (Max7219Wiring const& wiring : kMax7219Wirings) {
    if (!IsUpright(wiring)) {
      continue;
    }
    if (!modules.empty()) {
      modules += ", ";
    }
    modules += wiring.name;
    for (auto const& named : kModuleNames) {
      if (named.wiring->digits_as_rows == wiring.digits_as_rows &&
          named.wiring->reversed_columns == wiring.reversed_columns &&
          named.wiring->reversed_rows == wiring.reversed_rows) {
        modules += std::string(" (") + named.name + ")";
      }
    }
  }
  return modules.empty() ? "no module" : modules;
}

/*!
 * \brief Returns the left edge of the cell of a column, with a gap between the
 *        modules.
 *
 * \param[in] x The column, as the viewer sees it.
 * \return The pixels from the left.
 */
int32_t GetCellX(uint8_t x) {
  return kLeft +
         ((x / max_matrix::kColumnNumberOfModule) *
          ((max_matrix::kColumnNumberOfModule * kCell) + kModuleGap)) +
         ((x % max_matrix::kColumnNumberOfModule) * kCell);
}

/*!
 * \brief Returns the top edge of the cell of a row, the rows counted from the
 *        top of the panel.
 *
 * \param[in] y The row, as the viewer sees it.
 * \return The pixels from the top.
 */
int32_t GetCellY(uint8_t y) { return kTop + (y * kCell); }

/*!
 * \brief Returns the width of a picture of the whole panel.
 *
 * It leaves room on the right for the arrow into the module DIN reaches first.
 *
 * \return The width in pixels.
 */
uint16_t GetPictureWidth() {
  return static_cast<uint16_t>(GetCellX(kNumberOfColumns - 1) + kCell + kRight);
}

/*!
 * \brief Returns the height of a picture of the whole panel.
 *
 * It leaves room below the panel for the coordinates and two lines of legend.
 *
 * \return The height in pixels.
 */
uint16_t GetPictureHeight() {
  return static_cast<uint16_t>(GetCellY(max_matrix::kRowNumberOfModule - 1) +
                               kCell + kBottom);
}

/*!
 * \brief Draws the LED of a dot: a rounded square in the middle of its cell,
 *        with the label in white on it.
 *
 * \param[in,out] svg The picture.
 * \param[in] dot The dot.
 * \param[in] colour The fill colour.
 * \param[in] label The text in the LED, or empty.
 */
void DrawLed(DocuSvg* svg, Dot dot, char const* colour,
             std::string const& label) {
  int32_t const inset{(kCell - kLed) / 2};
  int32_t const left{GetCellX(dot.x)};
  int32_t const top{GetCellY(dot.y)};

  svg->AddRect({left + inset, top + inset},
               {static_cast<uint16_t>(kLed), static_cast<uint16_t>(kLed)},
               {colour, "none", kLedRadius});
  if (!label.empty()) {
    svg->AddText({left + (kCell / 2), top + (kCell / 2) + kBaselineShift},
                 label, kLedText);
  }
}

/*!
 * \brief Draws a module: its frame, its name above it, and an arrow into it if
 *        DIN reaches it first, which it asks the model.
 *
 * \param[in,out] svg The picture.
 * \param[in] module The module, from the left.
 */
void DrawModule(DocuSvg* svg, uint8_t module) {
  int32_t const module_width{max_matrix::kColumnNumberOfModule * kCell};
  int32_t const module_height{max_matrix::kRowNumberOfModule * kCell};
  int32_t const left{GetCellX(module * max_matrix::kColumnNumberOfModule)};
  int32_t const middle{kTop + (module_height / 2)};
  DocuLed led{};

  svg->AddRect({left - kFramePadding, kTop - kFramePadding},
               {static_cast<uint16_t>(module_width + (2 * kFramePadding)),
                static_cast<uint16_t>(module_height + (2 * kFramePadding))},
               {"none", kColourFrame, kFrameRadius});
  svg->AddText({left + (module_width / 2), kTop - kModuleNameAbove},
               "Module " + std::to_string(module), kNameText);
  if (!FindLedOfDot(module * max_matrix::kColumnNumberOfModule, 0, &led) ||
      led.chip != 0) {
    return;
  }
  bool const right{module == kNumberOfModules - 1};
  int32_t const edge{right ? left + module_width + (2 * kFramePadding)
                           : left - (2 * kFramePadding)};
  int32_t const outside{right ? edge + kArrowLength : edge - kArrowLength};
  svg->AddArrow({outside, middle}, edge, kArrowThickness, kColourFrame);
  svg->AddText({(outside + edge) / 2, middle - kDinAbove}, "DIN", kDinText);
}

/*!
 * \brief Draws the coordinates along both edges of the panel, and X and Y.
 *
 * \param[in,out] svg The picture.
 */
void DrawAxes(DocuSvg* svg) {
  int32_t const module_height{max_matrix::kRowNumberOfModule * kCell};
  int32_t const below{kTop + module_height + kAxisBelow};

  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    svg->AddText({GetCellX(x) + (kCell / 2), below}, std::to_string(x),
                 kColumnAxisText);
  }
  for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
    svg->AddText(
        {kLeft - kAxisLeft, GetCellY(y) + (kCell / 2) + kBaselineShift},
        std::to_string(y), kRowAxisText);
  }
  svg->AddText(
      {kLeft - kAxisNameLeft, kTop + (module_height / 2) + kAxisNameShift}, "Y",
      kNameText);
  svg->AddText({kLeft - kAxisLeft, below}, "X", kXText);
}

/*!
 * \brief Draws what both pictures share: the title, every module, and the
 *        coordinates.
 *
 * \param[in,out] svg The picture.
 * \param[in] title The line above the panel, or empty for none.
 */
void DrawPanel(DocuSvg* svg, std::string const& title) {
  if (!title.empty()) {
    svg->AddText({kLeft, kTitleBaseline}, title, kTitleText);
  }
  for (uint8_t module{0}; module < kNumberOfModules; module++) {
    DrawModule(svg, module);
  }
  DrawAxes(svg);
}

/*!
 * \brief Finds the LED of every dot with the module type that is set.
 *
 * \param[out] leds The chip, digit and bit of each dot's LED.
 * \return True if every dot lit exactly one LED.
 */
bool FindLeds(PerDot<DocuLed>* leds) {
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      if (!FindLedOfDot(x, y, &(*leds)[x][y])) {
        return false;
      }
    }
  }
  return true;
}

/*!
 * \brief Draws a band from each LED to its neighbour of the same register, on
 *        the right or below.
 *
 * \param[in,out] svg The picture.
 * \param[in] leds The chip, digit and bit of each dot's LED.
 */
void DrawBands(DocuSvg* svg, PerDot<DocuLed> const& leds) {
  int32_t const inset{((kCell - kLed) / 2) - 2};
  auto const across{static_cast<uint16_t>(kCell + kLed + 4)};
  auto const along{static_cast<uint16_t>(kLed + 4)};

  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      DocuPoint const corner{GetCellX(x) + inset, GetCellY(y) + inset};
      bool const right_in_module{x % max_matrix::kColumnNumberOfModule <
                                 max_matrix::kColumnNumberOfModule - 1};
      bool const below_on_panel{y < max_matrix::kRowNumberOfModule - 1};
      if (right_in_module && leds[x + 1][y].digit == leds[x][y].digit) {
        svg->AddRect(corner, {across, along}, {kColourBand, "none", 0});
      }
      if (below_on_panel && leds[x][y + 1].digit == leds[x][y].digit) {
        svg->AddRect(corner, {along, across}, {kColourBand, "none", 0});
      }
    }
  }
}

/*!
 * \brief Draws the two lines below the picture of a module type.
 *
 * \param[in,out] svg The picture.
 */
void DrawWiringLegend(DocuSvg* svg) {
  int32_t const legend{GetCellY(max_matrix::kRowNumberOfModule - 1) + kCell +
                       kLegendBelow};

  svg->AddText({kLeft, legend},
               "Each LED is labelled byte.bit: byte n is digit register n of "
               "the module's MAX7219 and "
               "matrix_buffer_[8 * module + n], bit b is bit b of it.",
               kLegendText);
  svg->AddText(
      {kLeft, legend + kLegendLine},
      "A blue band joins the LEDs of one byte, the darker one is its bit 0. "
      "Matrix orientation 0. "
      "Drawn by docu/DocuPictures.cpp from SetDot() and the MAX7219 model.",
      kLegendText);
}

/*!
 * \brief Draws the picture of how the bytes lie on the modules of one module
 *        type.
 *
 * It labels every LED with the digit register and the bit that drive it, and
 * joins the LEDs of one register in a band: a register is one byte of
 * `matrix_buffer_`.
 *
 * \param[in] module_type The module type.
 * \return The SVG text, or an empty text if a dot did not light exactly one
 *         LED.
 */
std::string DrawWiring(max_matrix::ModuleType module_type) {
  DocuSvg svg{GetPictureWidth(), GetPictureHeight()};
  PerDot<DocuLed> leds;

  matrix.SetModuleType(module_type);
  if (!FindLeds(&leds)) {
    return "";
  }
  DrawPanel(&svg, "Module type " + GetModules());
  DrawBands(&svg, leds);
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      char const* colour{leds[x][y].bit == 0 ? kColourLedBit0 : kColourLed};
      DrawLed(&svg, {x, y}, colour,
              std::to_string(leds[x][y].digit) + "." +
                  std::to_string(leds[x][y].bit));
    }
  }
  DrawWiringLegend(&svg);
  return svg.GetDocument();
}

/*!
 * \brief Labels with `bit` every dot that is lit now.
 *
 * \param[in] bit The bit of the value that lit them.
 * \param[in,out] labels The label of each dot.
 */
void LabelLitDots(uint8_t bit, PerDot<std::string>* labels) {
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      if (matrix.GetDot(x, y).GetValue()) {
        (*labels)[x][y] = std::to_string(bit);
      }
    }
  }
}

/*!
 * \brief Draws the names of SetColumn() and SetRow() under what they set.
 *
 * \param[in,out] svg The picture.
 */
void DrawFunctionNames(DocuSvg* svg) {
  int32_t const panel_height{max_matrix::kRowNumberOfModule * kCell};
  int32_t const names{kTop + panel_height + kNamesBelow};
  int32_t const row_module_left{
      GetCellX(kExampleRowModule * max_matrix::kColumnNumberOfModule)};

  svg->AddText({GetCellX(kExampleColumn) + (kCell / 2), names},
               "SetColumn(" + std::to_string(kExampleColumn) + ", value)",
               kColumnFunctionText);
  svg->AddText(
      {row_module_left + (max_matrix::kColumnNumberOfModule * kCell / 2),
       names},
      "SetRow(" + std::to_string(kExampleRowModule) + ", " +
          std::to_string(kExampleRow) + ", value)",
      kRowFunctionText);
}

/*!
 * \brief Draws the picture of the coordinates a sketch uses.
 *
 * It marks which dot each bit of SetColumn() and SetRow() sets, as GetDot()
 * reports it, and names the function under what it set. It has no title and no
 * legend: the README explains it right below, so the picture shows the panel
 * only.
 *
 * \return The SVG text.
 */
std::string DrawCoordinates() {
  int32_t const panel_height{max_matrix::kRowNumberOfModule * kCell};
  DocuSvg svg{GetPictureWidth(),
              kCoordinatesAbove + panel_height + kCoordinatesBelow,
              kTop - kCoordinatesAbove};
  PerDot<std::string> column_labels;
  PerDot<std::string> row_labels;

  // The coordinates are the same for every module once the module type fits it,
  // so the picture is drawn for FC-16 boards.
  matrix.SetModuleType(max_matrix::ModuleType::kFc16);
  for (uint8_t bit{0}; bit < max_matrix::kRowNumberOfModule; bit++) {
    matrix.Clear();
    matrix.SetColumn(kExampleColumn, static_cast<uint8_t>(1U << bit));
    LabelLitDots(bit, &column_labels);
    matrix.Clear();
    matrix.SetRow(kExampleRowModule, kExampleRow,
                  static_cast<uint8_t>(1U << bit));
    LabelLitDots(bit, &row_labels);
  }

  DrawPanel(&svg, "");
  for (uint8_t x{0}; x < kNumberOfColumns; x++) {
    for (uint8_t y{0}; y < max_matrix::kRowNumberOfModule; y++) {
      if (!column_labels[x][y].empty()) {
        DrawLed(&svg, {x, y}, kColourLed, column_labels[x][y]);
      } else if (!row_labels[x][y].empty()) {
        DrawLed(&svg, {x, y}, kColourRow, row_labels[x][y]);
      } else {
        DrawLed(&svg, {x, y}, kColourLedOff, "");
      }
    }
  }
  DrawFunctionNames(&svg);
  return svg.GetDocument();
}

/*!
 * \brief Reads a whole file.
 *
 * \param[in] path The file.
 * \return What it holds, or an empty text if it cannot be read, which no
 *         picture equals.
 */
std::string ReadFile(std::string const& path) {
  std::ifstream const file{path};
  std::stringstream content;

  content << file.rdbuf();
  return content.str();
}

/*!
 * \brief Draws every picture.
 *
 * \return Each picture's file name and its SVG text, which is empty if a dot
 *         did not light exactly one LED.
 */
std::vector<std::pair<std::string, std::string>> DrawPictures() {
  std::vector<std::pair<std::string, std::string>> pictures;

  pictures.emplace_back("Coordinates.svg", DrawCoordinates());
  // The module types in the order of the model's wirings, whose names the files
  // take.
  for (uint8_t type{0}; type < kMax7219Wirings.size(); type++) {
    pictures.emplace_back(
        std::string("Wiring_") + kMax7219Wirings[type].name + ".svg",
        DrawWiring(static_cast<max_matrix::ModuleType>(type)));
  }
  return pictures;
}

/*!
 * \brief Writes the pictures into a folder, or checks that it holds them.
 *
 * \param[in] pictures Each picture's file name and its SVG text.
 * \param[in] folder The folder.
 * \param[in] write True to write, false to check.
 * \return How many pictures could not be drawn or differ.
 */
uint8_t WriteOrCheck(
    std::vector<std::pair<std::string, std::string>> const& pictures,
    std::string const& folder, bool write) {
  uint8_t failed{0};

  for (auto const& picture : pictures) {
    std::string const path = folder + "/" + picture.first;
    if (picture.second.empty()) {
      std::cerr << picture.first << ": a dot did not light exactly one LED\n";
      failed++;
    } else if (write) {
      std::ofstream(path) << picture.second;
    } else if (ReadFile(path) != picture.second) {
      std::cerr << path
                << " is not what the library draws; run DocuPictures --write\n";
      failed++;
    }
  }
  return failed;
}

}  // namespace

/*!
 * \brief Writes or checks the pictures.
 *
 * It draws every picture, then writes each one or compares it with the file of
 * that name.
 *
 * \param[in] argc The number of arguments, the program's name included.
 * \param[in] argv `--write` or `--check`, and the folder of the pictures.
 * \return 0 when all is written or up to date, 1 when a picture differs or
 *         cannot be drawn, 2 on a wrong command line.
 */
int main(int argc, char** argv) {  // NOLINT(misc-const-correctness): main()
  // takes argv as the standard declares it, whatever reads it.
  // T-20: main() is handed its arguments as a pointer to an array
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::vector<std::string> const arguments(argv, argv + argc);

  if (arguments.size() != 3 ||
      (arguments[1] != "--write" && arguments[1] != "--check")) {
    std::cerr << "usage: " << arguments[0] << " --write|--check FOLDER\n";
    return 2;
  }
  bool const write{arguments[1] == "--write"};

  mock::Reset(kDataInPin, kChipSelectPin, kClockPin, kNumberOfModules);
  matrix.Init();
  return WriteOrCheck(DrawPictures(), arguments[2], write) == 0 ? 0 : 1;
}
