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
 * register bit of which MAX7219 the model in test/mock/ latched, or which dot
 * GetDot() reports. The module types in the titles are the wirings of the model
 * on which a pattern set with that module type appears upright. So a picture
 * cannot say something the library does not do, and the test docu/pictures
 * fails when the library changes and the pictures were not written again.
 */

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "Arduino.h"
#include "DocuSvg.h"
#include "MaxMatrix.h"

/* layout, in pixels */
#define DOCU_CELL 28
#define DOCU_LED 22
#define DOCU_MODULE_GAP 28
#define DOCU_LEFT 60
#define DOCU_TOP 86
#define DOCU_RIGHT 120
#define DOCU_BOTTOM 104

/* the coordinate picture shows the panel only, from just above the module names
 * to just below the function names */
#define DOCU_COORDINATES_ABOVE 40
#define DOCU_COORDINATES_BELOW 60

/* font sizes */
#define DOCU_FONT_TITLE 20
#define DOCU_FONT_MODULE 15
#define DOCU_FONT_AXIS 13
#define DOCU_FONT_LED 12
#define DOCU_FONT_LEGEND 15

/* colours */
#define DOCU_COLOUR_TEXT "#222222"
#define DOCU_COLOUR_FRAME "#666666"
#define DOCU_COLOUR_LED "#e53935"
#define DOCU_COLOUR_LED_BIT_0 "#a31515"
#define DOCU_COLOUR_LED_OFF "#d6d6d6"
#define DOCU_COLOUR_BAND "#bbdefb"
#define DOCU_COLOUR_ROW "#1e88e5"

/* the pins the examples use; the mock needs chip select to be the one the
 * library drives */
#define DOCU_DATA_IN_PIN 11
#define DOCU_CHIP_SELECT_PIN 10
#define DOCU_CLOCK_PIN 13
/* the chain of the examples, four modules, which the pictures show */
#define DOCU_NUMBER_OF_MODULES 4
#define DOCU_NUMBER_OF_COLUMNS \
  (DOCU_NUMBER_OF_MODULES * MAXMATRIX_COLUMN_NUMBER_OF_MODULE)

/* what the coordinate picture shows SetColumn() and SetRow() with */
#define DOCU_EXAMPLE_COLUMN 5
#define DOCU_EXAMPLE_ROW_MODULE 2
#define DOCU_EXAMPLE_ROW 2

/*!
 * \brief Where the LED of one dot is driven: the chip in the chain, 0 at DIN,
 *        its digit register and the bit in it.
 */
typedef struct {
  byte chip;
  byte digit;
  byte bit;
} DocuLed;

static max_matrix::MatrixOf<DOCU_NUMBER_OF_MODULES> matrix(DOCU_DATA_IN_PIN,
                                                           DOCU_CHIP_SELECT_PIN,
                                                           DOCU_CLOCK_PIN);

/* the names MD_MAX72XX gives four of the eight wirings, as the README uses them
 */
static struct {
  Max7219Wiring const* wiring;
  char const* name;
} const kModuleNames[] = {
    {&kMaX7219WiringFC16, "FC-16"},
    {&kMaX7219WiringGeneric, "Generic"},
    {&kMaX7219WiringParola, "Parola"},
    {&kMaX7219WiringIcstation, "ICStation"},
};

/*!
 * \brief Finds the one LED the model has lit, in the digit registers of every
 *        chip.
 *
 * \param[out] led The chip, digit and bit of the lit LED.
 * \return True if exactly one LED is lit, or false if none or more than one is.
 */
static bool FindLed(DocuLed* led) {
  byte lit = 0;

  for (byte chip = 0; chip < mock::chain.chips.size(); chip++) {
    for (byte digit = 0; digit < Max7219::kNumberOfDigits; digit++) {
      for (byte bit = 0; bit < 8; bit++) {
        if (bitRead(mock::chain.chips[chip].digit[digit], bit)) {
          led->chip = chip;
          led->digit = digit;
          led->bit = bit;
          lit++;
        }
      }
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
static bool FindLedOfDot(byte x, byte y, DocuLed* led) {
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
static bool IsUpright(Max7219Wiring const& wiring) {
  bool upright = true;

  matrix.Clear();
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++)
      matrix.SetDot(x, y, (x * x + 3 * y + x * y) % 3 == 0);
  }
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
      if (mock::chain.IsLit(wiring, x, y) != ((x * x + 3 * y + x * y) % 3 == 0))
        upright = false;
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
static std::string GetModules() {
  std::string modules;

  for (Max7219Wiring const& wiring : kMax7219Wirings) {
    if (!IsUpright(wiring)) continue;
    if (!modules.empty()) modules += ", ";
    modules += wiring.name;
    for (auto const& named : kModuleNames) {
      if (named.wiring->digits_as_rows == wiring.digits_as_rows &&
          named.wiring->reversed_columns == wiring.reversed_columns &&
          named.wiring->reversed_rows == wiring.reversed_rows)
        modules += std::string(" (") + named.name + ")";
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
static int32_t GetCellX(byte x) {
  return DOCU_LEFT +
         (x / MAXMATRIX_COLUMN_NUMBER_OF_MODULE) *
             (MAXMATRIX_COLUMN_NUMBER_OF_MODULE * DOCU_CELL + DOCU_MODULE_GAP) +
         (x % MAXMATRIX_COLUMN_NUMBER_OF_MODULE) * DOCU_CELL;
}

/*!
 * \brief Returns the top edge of the cell of a row, the rows counted from the
 *        top of the panel.
 *
 * \param[in] y The row, as the viewer sees it.
 * \return The pixels from the top.
 */
static int32_t GetCellY(byte y) { return DOCU_TOP + y * DOCU_CELL; }

/*!
 * \brief Returns the width of a picture of the whole panel.
 *
 * It leaves room on the right for the arrow into the module DIN reaches first.
 *
 * \return The width in pixels.
 */
static uint16_t GetPictureWidth() {
  return static_cast<uint16_t>(GetCellX(DOCU_NUMBER_OF_COLUMNS - 1) +
                               DOCU_CELL + DOCU_RIGHT);
}

/*!
 * \brief Returns the height of a picture of the whole panel.
 *
 * It leaves room below the panel for the coordinates and two lines of legend.
 *
 * \return The height in pixels.
 */
static uint16_t GetPictureHeight() {
  return static_cast<uint16_t>(GetCellY(MAXMATRIX_ROW_NUMBER_OF_MODULE - 1) +
                               DOCU_CELL + DOCU_BOTTOM);
}

/*!
 * \brief Draws the LED of a dot: a rounded square in the middle of its cell,
 *        with the label in white on it.
 *
 * \param[in,out] svg The picture.
 * \param[in] x The column, as the viewer sees it.
 * \param[in] y The row, as the viewer sees it.
 * \param[in] colour The fill colour.
 * \param[in] label The text in the LED, or empty.
 */
static void DrawLed(DocuSvg* svg, byte x, byte y, char const* colour,
                    std::string const& label) {
  int32_t const inset = (DOCU_CELL - DOCU_LED) / 2;

  svg->AddRect(GetCellX(x) + inset, GetCellY(y) + inset, DOCU_LED, DOCU_LED,
               colour, "none", 3);
  if (!label.empty())
    svg->AddText(GetCellX(x) + DOCU_CELL / 2, GetCellY(y) + DOCU_CELL / 2 + 4,
                 label, DOCU_FONT_LED, "#ffffff", "middle", true);
}

/*!
 * \brief Draws what both pictures share.
 *
 * It draws the title, a frame and a label per module, the coordinates along
 * both edges, and an arrow into the module that DIN reaches first, which it
 * finds by asking the model.
 *
 * \param[in,out] svg The picture.
 * \param[in] title The line above the panel, or empty for none.
 */
static void DrawPanel(DocuSvg* svg, std::string const& title) {
  int32_t const module_width = MAXMATRIX_COLUMN_NUMBER_OF_MODULE * DOCU_CELL;
  int32_t const module_height = MAXMATRIX_ROW_NUMBER_OF_MODULE * DOCU_CELL;
  DocuLed led;

  if (!title.empty())
    svg->AddText(DOCU_LEFT, 30, title, DOCU_FONT_TITLE, DOCU_COLOUR_TEXT,
                 "start", true);
  for (byte module = 0; module < DOCU_NUMBER_OF_MODULES; module++) {
    int32_t const left = GetCellX(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    svg->AddRect(left - 4, DOCU_TOP - 4, module_width + 8, module_height + 8,
                 "none", DOCU_COLOUR_FRAME, 4);
    svg->AddText(left + module_width / 2, DOCU_TOP - 14,
                 "Module " + std::to_string(module), DOCU_FONT_MODULE,
                 DOCU_COLOUR_TEXT, "middle", true);

    if (FindLedOfDot(module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, 0, &led) &&
        led.chip == 0) {
      bool const right = module == DOCU_NUMBER_OF_MODULES - 1;
      int32_t const edge = right ? left + module_width + 8 : left - 8;
      int32_t const outside = right ? edge + 70 : edge - 70;
      svg->AddArrow(outside, edge, DOCU_TOP + module_height / 2, 3,
                    DOCU_COLOUR_FRAME);
      svg->AddText((outside + edge) / 2, DOCU_TOP + module_height / 2 - 10,
                   "DIN", DOCU_FONT_MODULE, DOCU_COLOUR_TEXT);
    }
  }
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    svg->AddText(GetCellX(x) + DOCU_CELL / 2, DOCU_TOP + module_height + 22,
                 std::to_string(x), DOCU_FONT_AXIS, DOCU_COLOUR_TEXT);
  }
  for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
    svg->AddText(DOCU_LEFT - 14, GetCellY(y) + DOCU_CELL / 2 + 4,
                 std::to_string(y), DOCU_FONT_AXIS, DOCU_COLOUR_TEXT, "end");
  }
  svg->AddText(DOCU_LEFT - 40, DOCU_TOP + module_height / 2 + 5, "Y",
               DOCU_FONT_MODULE, DOCU_COLOUR_TEXT, "middle", true);
  svg->AddText(DOCU_LEFT - 14, DOCU_TOP + module_height + 22, "X",
               DOCU_FONT_MODULE, DOCU_COLOUR_TEXT, "end", true);
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
static std::string DrawWiring(max_matrix::ModuleType module_type) {
  DocuSvg svg(GetPictureWidth(), GetPictureHeight());
  DocuLed leds[DOCU_NUMBER_OF_COLUMNS][MAXMATRIX_ROW_NUMBER_OF_MODULE];
  int32_t const inset = (DOCU_CELL - DOCU_LED) / 2 - 2;

  matrix.SetModuleType(module_type);
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
      if (!FindLedOfDot(x, y, &leds[x][y])) return "";
    }
  }

  DrawPanel(&svg, "Module type " + GetModules());
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
      /* a band from the LED to its neighbour of the same register, right or
       * below */
      if (x % MAXMATRIX_COLUMN_NUMBER_OF_MODULE <
              MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 &&
          leds[x + 1][y].digit == leds[x][y].digit) {
        svg.AddRect(GetCellX(x) + inset, GetCellY(y) + inset,
                    DOCU_CELL + DOCU_LED + 4, DOCU_LED + 4, DOCU_COLOUR_BAND);
      }
      if (y < MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 &&
          leds[x][y + 1].digit == leds[x][y].digit) {
        svg.AddRect(GetCellX(x) + inset, GetCellY(y) + inset, DOCU_LED + 4,
                    DOCU_CELL + DOCU_LED + 4, DOCU_COLOUR_BAND);
      }
    }
  }
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
      char const* colour =
          leds[x][y].bit == 0 ? DOCU_COLOUR_LED_BIT_0 : DOCU_COLOUR_LED;
      DrawLed(&svg, x, y, colour,
              std::to_string(leds[x][y].digit) + "." +
                  std::to_string(leds[x][y].bit));
    }
  }

  int32_t const legend =
      GetCellY(MAXMATRIX_ROW_NUMBER_OF_MODULE - 1) + DOCU_CELL + 56;
  svg.AddText(DOCU_LEFT, legend,
              "Each LED is labelled byte.bit: byte n is digit register n of "
              "the module's MAX7219 and "
              "matrix_buffer_[8 * module + n], bit b is bit b of it.",
              DOCU_FONT_LEGEND, DOCU_COLOUR_TEXT, "start");
  svg.AddText(
      DOCU_LEFT, legend + 22,
      "A blue band joins the LEDs of one byte, the darker one is its bit 0. "
      "Matrix orientation 0. "
      "Drawn by docu/DocuPictures.cpp from SetDot() and the MAX7219 model.",
      DOCU_FONT_LEGEND, DOCU_COLOUR_TEXT, "start");
  return svg.GetDocument();
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
static std::string DrawCoordinates() {
  int32_t const panel_height = MAXMATRIX_ROW_NUMBER_OF_MODULE * DOCU_CELL;
  DocuSvg svg(GetPictureWidth(),
              DOCU_COORDINATES_ABOVE + panel_height + DOCU_COORDINATES_BELOW,
              DOCU_TOP - DOCU_COORDINATES_ABOVE);
  std::string column_labels[DOCU_NUMBER_OF_COLUMNS]
                           [MAXMATRIX_ROW_NUMBER_OF_MODULE];
  std::string row_labels[DOCU_NUMBER_OF_COLUMNS]
                        [MAXMATRIX_ROW_NUMBER_OF_MODULE];

  // The coordinates are the same for every module once the module type fits it,
  // so the picture is drawn for FC-16 boards.
  matrix.SetModuleType(max_matrix::ModuleType::kFc16);
  for (byte bit = 0; bit < 8; bit++) {
    matrix.Clear();
    matrix.SetColumn(DOCU_EXAMPLE_COLUMN, static_cast<byte>(1 << bit));
    for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
      for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
        if (matrix.GetDot(x, y).GetValue())
          column_labels[x][y] = std::to_string(bit);
      }
    }
    matrix.Clear();
    matrix.SetRow(DOCU_EXAMPLE_ROW_MODULE, DOCU_EXAMPLE_ROW,
                  static_cast<byte>(1 << bit));
    for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
      for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
        if (matrix.GetDot(x, y).GetValue())
          row_labels[x][y] = std::to_string(bit);
      }
    }
  }

  DrawPanel(&svg, "");
  for (byte x = 0; x < DOCU_NUMBER_OF_COLUMNS; x++) {
    for (byte y = 0; y < MAXMATRIX_ROW_NUMBER_OF_MODULE; y++) {
      if (!column_labels[x][y].empty())
        DrawLed(&svg, x, y, DOCU_COLOUR_LED, column_labels[x][y]);
      else if (!row_labels[x][y].empty())
        DrawLed(&svg, x, y, DOCU_COLOUR_ROW, row_labels[x][y]);
      else
        DrawLed(&svg, x, y, DOCU_COLOUR_LED_OFF, "");
    }
  }

  int32_t const names = DOCU_TOP + panel_height + 46;
  int32_t const row_module_left =
      GetCellX(DOCU_EXAMPLE_ROW_MODULE * MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
  svg.AddText(GetCellX(DOCU_EXAMPLE_COLUMN) + DOCU_CELL / 2, names,
              "SetColumn(" + std::to_string(DOCU_EXAMPLE_COLUMN) + ", value)",
              DOCU_FONT_MODULE, DOCU_COLOUR_LED, "middle", true);
  svg.AddText(
      row_module_left + MAXMATRIX_COLUMN_NUMBER_OF_MODULE * DOCU_CELL / 2,
      names,
      "SetRow(" + std::to_string(DOCU_EXAMPLE_ROW_MODULE) + ", " +
          std::to_string(DOCU_EXAMPLE_ROW) + ", value)",
      DOCU_FONT_MODULE, DOCU_COLOUR_ROW, "middle", true);
  return svg.GetDocument();
}

/*!
 * \brief Reads a whole file.
 *
 * \param[in] path The file.
 * \return What it holds, or an empty text if it cannot be read, which no
 *         picture equals.
 */
static std::string ReadFile(std::string const& path) {
  std::ifstream file(path);
  std::stringstream content;

  content << file.rdbuf();
  return content.str();
}

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
int main(int argc, char** argv) {
  if (argc != 3 ||
      (strcmp(argv[1], "--write") != 0 && strcmp(argv[1], "--check") != 0)) {
    fprintf(stderr, "usage: %s --write|--check FOLDER\n", argv[0]);
    return 2;
  }
  bool const write = strcmp(argv[1], "--write") == 0;
  std::string const folder = argv[2];
  byte failed = 0;

  mock::Reset(DOCU_DATA_IN_PIN, DOCU_CHIP_SELECT_PIN, DOCU_CLOCK_PIN,
              DOCU_NUMBER_OF_MODULES);
  matrix.Init();

  std::vector<std::pair<std::string, std::string>> pictures;
  pictures.push_back(std::make_pair("Coordinates.svg", DrawCoordinates()));
  // The module types in the order of the model's wirings, whose names the files
  // take.
  for (byte type = 0;
       type < sizeof(kMax7219Wirings) / sizeof(kMax7219Wirings[0]); type++) {
    pictures.push_back(std::make_pair(
        std::string("Wiring_") + kMax7219Wirings[type].name + ".svg",
        DrawWiring(static_cast<max_matrix::ModuleType>(type))));
  }

  for (auto const& picture : pictures) {
    std::string const path = folder + "/" + picture.first;
    if (picture.second.empty()) {
      fprintf(stderr, "%s: a dot did not light exactly one LED\n",
              picture.first.c_str());
      failed++;
    } else if (write) {
      std::ofstream(path) << picture.second;
    } else if (ReadFile(path) != picture.second) {
      fprintf(stderr,
              "%s is not what the library draws; run DocuPictures --write\n",
              path.c_str());
      failed++;
    }
  }
  return failed == 0 ? 0 : 1;
}
