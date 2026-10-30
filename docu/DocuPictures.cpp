/******************************************************************************************************************************************************
 *  COPYRIGHT
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  \verbatim
 *  Copyright (c) Andreas Burnickl                                                                                                 All rights reserved.
 *
 *  \endverbatim
 *  ---------------------------------------------------------------------------------------------------------------------------------------------------
 *  FILE DESCRIPTION
 *  -------------------------------------------------------------------------------------------------------------------------------------------------*/
/**     \file       DocuPictures.cpp
 *      \brief      Draws the pictures in docu/ from what the library does
 *
 *      \details    build/DocuPictures --write docu     writes them
 *                  build/DocuPictures --check docu     fails if one in docu/ is not what it would write
 *
 *                  Nothing in the pictures is drawn by hand. Every LED is placed by calling the library, with setDot(),
 *                  setColumn() or setRow(), and reading back which register bit of which MAX7219 the model in
 *                  test/mock/ latched, or which dot getDot() reports. The module types in the titles are the wirings
 *                  of the model on which a pattern set in that orientation appears upright. So a picture cannot say
 *                  something the library does not do, and the test docu/pictures fails when the library changes and
 *                  the pictures were not written again.
 *
 *****************************************************************************************************************************************************/
#define _DOCU_PICTURES_SOURCE_

/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include "Arduino.h"
#include "MaxMatrix.h"
#include "DocuSvg.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* layout, in pixels */
#define DOCU_CELL                                                   28
#define DOCU_LED                                                    22
#define DOCU_MODULE_GAP                                             28
#define DOCU_LEFT                                                   60
#define DOCU_TOP                                                    86
#define DOCU_RIGHT                                                  120
#define DOCU_BOTTOM                                                 104

/* the coordinate picture shows the panel only, from just above the module names to just below the function names */
#define DOCU_COORDINATES_ABOVE                                      40
#define DOCU_COORDINATES_BELOW                                      60

/* font sizes */
#define DOCU_FONT_TITLE                                             20
#define DOCU_FONT_MODULE                                            15
#define DOCU_FONT_AXIS                                              13
#define DOCU_FONT_LED                                               12
#define DOCU_FONT_LEGEND                                            15

/* colours */
#define DOCU_COLOUR_TEXT                                            "#222222"
#define DOCU_COLOUR_FRAME                                           "#666666"
#define DOCU_COLOUR_LED                                             "#e53935"
#define DOCU_COLOUR_LED_BIT_0                                       "#a31515"
#define DOCU_COLOUR_LED_OFF                                         "#d6d6d6"
#define DOCU_COLOUR_BAND                                            "#bbdefb"
#define DOCU_COLOUR_ROW                                             "#1e88e5"

/* the pins the examples use; the mock needs chip select to be the one the library drives */
#define DOCU_DATA_IN_PIN                                            11
#define DOCU_CHIP_SELECT_PIN                                        10
#define DOCU_CLOCK_PIN                                              13

/* what the coordinate picture shows setColumn() and setRow() with */
#define DOCU_EXAMPLE_COLUMN                                         5
#define DOCU_EXAMPLE_ROW_MODULE                                     2
#define DOCU_EXAMPLE_ROW                                            2


/******************************************************************************************************************************************************
 *  LOCAL DATA TYPES AND STRUCTURES
 *****************************************************************************************************************************************************/
/* where the LED of one dot is driven: the chip in the chain, 0 at DIN, its digit register and the bit in it */
typedef struct {
    byte Chip;
    byte Digit;
    byte Bit;
} DocuLedType;


/******************************************************************************************************************************************************
 *  LOCAL DATA
 *****************************************************************************************************************************************************/
static MaxMatrix Matrix(DOCU_DATA_IN_PIN, DOCU_CHIP_SELECT_PIN, DOCU_CLOCK_PIN);

static const struct {
    MaxMatrixModuleOrientationType Orientation;
    uint16_t Degrees;
} ModuleOrientations[] = {
    { MAXMATRIX_MODULE_ORIENTATION_0, 0 },
    { MAXMATRIX_MODULE_ORIENTATION_90, 90 },
    { MAXMATRIX_MODULE_ORIENTATION_180, 180 },
    { MAXMATRIX_MODULE_ORIENTATION_270, 270 },
};

/* the names MD_MAX72XX gives four of the eight wirings, as the README uses them */
static const struct {
    const Max7219WiringType* Wiring;
    const char* Name;
} ModuleNames[] = {
    { &MAX7219_WIRING_FC16, "FC-16" },
    { &MAX7219_WIRING_GENERIC, "Generic" },
    { &MAX7219_WIRING_PAROLA, "Parola" },
    { &MAX7219_WIRING_ICSTATION, "ICStation" },
};


/******************************************************************************************************************************************************
 * L O C A L   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  findLed()
******************************************************************************************************************************************************/
/*! \brief          finds the one LED the model has lit
 *  \details        this function reads the digit registers of every chip in the model
 *
 *  \param[out]     Led             the chip, digit and bit of the lit LED
 *  \return         E_OK
 *                  E_NOT_OK        no LED or more than one is lit
 *****************************************************************************************************************************************************/
static stdReturnType findLed(DocuLedType* Led)
{
    byte Lit = 0;

    for(byte Chip = 0; Chip < Mock::Chain.Chips.size(); Chip++) {
        for(byte Digit = 0; Digit < Max7219::NumberOfDigits; Digit++) {
            for(byte Bit = 0; Bit < 8; Bit++) {
                if(bitRead(Mock::Chain.Chips[Chip].Digit[Digit], Bit)) {
                    Led->Chip = Chip;
                    Led->Digit = Digit;
                    Led->Bit = Bit;
                    Lit++;
                }
            }
        }
    }
    return Lit == 1 ? E_OK : E_NOT_OK;
} /* findLed */


/******************************************************************************************************************************************************
  findLedOfDot()
******************************************************************************************************************************************************/
/*! \brief          finds the LED that drives a dot as the viewer sees it
 *  \details        this function clears the matrix, sets the one dot with setDot() and asks the model which LED it lit
 *
 *  \param[in]      X               column as the viewer sees it
 *  \param[in]      Y               row as the viewer sees it
 *  \param[out]     Led             the chip, digit and bit
 *  \return         E_OK
 *                  E_NOT_OK        the dot did not light exactly one LED
 *****************************************************************************************************************************************************/
static stdReturnType findLedOfDot(byte X, byte Y, DocuLedType* Led)
{
    Matrix.clear();
    Matrix.setDot(X, Y, true);
    return findLed(Led);
} /* findLedOfDot */


/******************************************************************************************************************************************************
  isUpright()
******************************************************************************************************************************************************/
/*! \brief          says whether a module of a wiring shows the panel upright
 *  \details        this function sets a pattern without symmetry dot by dot and compares it with what the model lights
 *                  for that wiring, as MaxMatrixModuleTest does
 *
 *  \param[in]      Wiring          the wiring of the modules
 *  \return         true if every dot is lit where it was set
 *****************************************************************************************************************************************************/
static bool isUpright(const Max7219WiringType& Wiring)
{
    bool Upright = true;

    Matrix.clear();
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) Matrix.setDot(X, Y, (X * X + 3 * Y + X * Y) % 3 == 0);
    }
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            if(Mock::Chain.isLit(Wiring, X, Y) != ((X * X + 3 * Y + X * Y) % 3 == 0)) Upright = false;
        }
    }
    return Upright;
} /* isUpright */


/******************************************************************************************************************************************************
  getModules()
******************************************************************************************************************************************************/
/*! \brief          the modules the current orientation fits
 *  \details        this function names every wiring of the model on which the panel is upright, with the name
 *                  MD_MAX72XX gives it where there is one
 *
 *  \return         the names, separated by commas, or "no module"
 *****************************************************************************************************************************************************/
static std::string getModules()
{
    std::string Modules;

    for(const Max7219WiringType& Wiring : Max7219Wirings) {
        if(!isUpright(Wiring)) continue;
        if(!Modules.empty()) Modules += ", ";
        Modules += Wiring.Name;
        for(const auto& Named : ModuleNames) {
            if(Named.Wiring->DigitsAsRows == Wiring.DigitsAsRows && Named.Wiring->ReversedColumns == Wiring.ReversedColumns &&
               Named.Wiring->ReversedRows == Wiring.ReversedRows) Modules += std::string(" (") + Named.Name + ")";
        }
    }
    return Modules.empty() ? "no module" : Modules;
} /* getModules */


/******************************************************************************************************************************************************
  getCellX()
******************************************************************************************************************************************************/
/*! \brief          the left edge of the cell of a column
 *  \details        this function leaves a gap between the modules
 *
 *  \param[in]      X               column as the viewer sees it
 *  \return         pixels from the left
 *****************************************************************************************************************************************************/
static int16_t getCellX(byte X)
{
    return DOCU_LEFT + (X / MAXMATRIX_COLUMN_NUMBER_OF_MODULE) * (MAXMATRIX_COLUMN_NUMBER_OF_MODULE * DOCU_CELL + DOCU_MODULE_GAP)
           + (X % MAXMATRIX_COLUMN_NUMBER_OF_MODULE) * DOCU_CELL;
} /* getCellX */


/******************************************************************************************************************************************************
  getCellY()
******************************************************************************************************************************************************/
/*! \brief          the top edge of the cell of a row
 *  \details        this function counts the rows from the top of the panel
 *
 *  \param[in]      Y               row as the viewer sees it
 *  \return         pixels from the top
 *****************************************************************************************************************************************************/
static int16_t getCellY(byte Y)
{
    return DOCU_TOP + Y * DOCU_CELL;
} /* getCellY */


/******************************************************************************************************************************************************
  getPictureWidth()
******************************************************************************************************************************************************/
/*! \brief          the width of a picture of the whole panel
 *  \details        this function leaves room on the right for the arrow into the module DIN reaches first
 *
 *  \return         pixels
 *****************************************************************************************************************************************************/
static uint16_t getPictureWidth()
{
    return getCellX(MAXMATRIX_NUMBER_OF_COLUMNS - 1) + DOCU_CELL + DOCU_RIGHT;
} /* getPictureWidth */


/******************************************************************************************************************************************************
  getPictureHeight()
******************************************************************************************************************************************************/
/*! \brief          the height of a picture of the whole panel
 *  \details        this function leaves room below the panel for the coordinates and two lines of legend
 *
 *  \return         pixels
 *****************************************************************************************************************************************************/
static uint16_t getPictureHeight()
{
    return getCellY(MAXMATRIX_ROW_NUMBER_OF_MODULE - 1) + DOCU_CELL + DOCU_BOTTOM;
} /* getPictureHeight */


/******************************************************************************************************************************************************
  drawLed()
******************************************************************************************************************************************************/
/*! \brief          draws the LED of a dot, with a label
 *  \details        this function draws a rounded square in the middle of the cell, with the label in white on it
 *
 *  \param[out]     Svg             the picture
 *  \param[in]      X               column as the viewer sees it
 *  \param[in]      Y               row as the viewer sees it
 *  \param[in]      Colour          fill colour
 *  \param[in]      Label           text in the LED, or empty
 *  \return         -
 *****************************************************************************************************************************************************/
static void drawLed(DocuSvg* Svg, byte X, byte Y, const char* Colour, const std::string& Label)
{
    const int16_t Inset = (DOCU_CELL - DOCU_LED) / 2;

    Svg->addRect(getCellX(X) + Inset, getCellY(Y) + Inset, DOCU_LED, DOCU_LED, Colour, "none", 3);
    if(!Label.empty()) Svg->addText(getCellX(X) + DOCU_CELL / 2, getCellY(Y) + DOCU_CELL / 2 + 4, Label, DOCU_FONT_LED, "#ffffff", "middle", true);
} /* drawLed */


/******************************************************************************************************************************************************
  drawPanel()
******************************************************************************************************************************************************/
/*! \brief          draws what both pictures share
 *  \details        this function draws the title, a frame and a label per module, the coordinates along both edges
 *                  and an arrow into the module that DIN reaches first, which it finds by asking the model
 *
 *  \param[out]     Svg             the picture
 *  \param[in]      Title           the line above the panel, or empty for none
 *  \return         -
 *****************************************************************************************************************************************************/
static void drawPanel(DocuSvg* Svg, const std::string& Title)
{
    const int16_t ModuleWidth = MAXMATRIX_COLUMN_NUMBER_OF_MODULE * DOCU_CELL;
    const int16_t ModuleHeight = MAXMATRIX_ROW_NUMBER_OF_MODULE * DOCU_CELL;
    DocuLedType Led;

    if(!Title.empty()) Svg->addText(DOCU_LEFT, 30, Title, DOCU_FONT_TITLE, DOCU_COLOUR_TEXT, "start", true);
    for(byte Module = 0; Module < MAXMATRIX_NUMBER_OF_MODULES; Module++) {
        const int16_t Left = getCellX(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
        Svg->addRect(Left - 4, DOCU_TOP - 4, ModuleWidth + 8, ModuleHeight + 8, "none", DOCU_COLOUR_FRAME, 4);
        Svg->addText(Left + ModuleWidth / 2, DOCU_TOP - 14, "Module " + std::to_string(Module), DOCU_FONT_MODULE, DOCU_COLOUR_TEXT, "middle", true);

        if(E_OK == findLedOfDot(Module * MAXMATRIX_COLUMN_NUMBER_OF_MODULE, 0, &Led) && Led.Chip == 0) {
            const bool Right = Module == MAXMATRIX_NUMBER_OF_MODULES - 1;
            const int16_t Edge = Right ? Left + ModuleWidth + 8 : Left - 8;
            const int16_t Outside = Right ? Edge + 70 : Edge - 70;
            Svg->addArrow(Outside, Edge, DOCU_TOP + ModuleHeight / 2, 3, DOCU_COLOUR_FRAME);
            Svg->addText((Outside + Edge) / 2, DOCU_TOP + ModuleHeight / 2 - 10, "DIN", DOCU_FONT_MODULE, DOCU_COLOUR_TEXT);
        }
    }
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        Svg->addText(getCellX(X) + DOCU_CELL / 2, DOCU_TOP + ModuleHeight + 22, std::to_string(X), DOCU_FONT_AXIS, DOCU_COLOUR_TEXT);
    }
    for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
        Svg->addText(DOCU_LEFT - 14, getCellY(Y) + DOCU_CELL / 2 + 4, std::to_string(Y), DOCU_FONT_AXIS, DOCU_COLOUR_TEXT, "end");
    }
    Svg->addText(DOCU_LEFT - 40, DOCU_TOP + ModuleHeight / 2 + 5, "Y", DOCU_FONT_MODULE, DOCU_COLOUR_TEXT, "middle", true);
    Svg->addText(DOCU_LEFT - 14, DOCU_TOP + ModuleHeight + 22, "X", DOCU_FONT_MODULE, DOCU_COLOUR_TEXT, "end", true);
} /* drawPanel */


/******************************************************************************************************************************************************
  drawWiring()
******************************************************************************************************************************************************/
/*! \brief          the picture of how the bytes lie on the modules in one module orientation
 *  \details        this function labels every LED with the digit register and the bit that drive it, and joins the LEDs
 *                  of one register in a band: a register is one byte of MatrixBuffer
 *
 *  \param[in]      Orientation     the module orientation
 *  \param[in]      Degrees         the same in degrees, for the title
 *  \return         the SVG text
 *****************************************************************************************************************************************************/
static std::string drawWiring(MaxMatrixModuleOrientationType Orientation, uint16_t Degrees)
{
    DocuSvg Svg(getPictureWidth(), getPictureHeight());
    DocuLedType Leds[MAXMATRIX_NUMBER_OF_COLUMNS][MAXMATRIX_ROW_NUMBER_OF_MODULE];
    const int16_t Inset = (DOCU_CELL - DOCU_LED) / 2 - 2;

    Matrix.setModuleOrientation(Orientation);
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            if(E_OK != findLedOfDot(X, Y, &Leds[X][Y])) return "";
        }
    }

    drawPanel(&Svg, "Module orientation " + std::to_string(Degrees) + ": " + getModules());
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            /* a band from the LED to its neighbour of the same register, right or below */
            if(X % MAXMATRIX_COLUMN_NUMBER_OF_MODULE < MAXMATRIX_COLUMN_NUMBER_OF_MODULE - 1 && Leds[X + 1][Y].Digit == Leds[X][Y].Digit) {
                Svg.addRect(getCellX(X) + Inset, getCellY(Y) + Inset, DOCU_CELL + DOCU_LED + 4, DOCU_LED + 4, DOCU_COLOUR_BAND);
            }
            if(Y < MAXMATRIX_ROW_NUMBER_OF_MODULE - 1 && Leds[X][Y + 1].Digit == Leds[X][Y].Digit) {
                Svg.addRect(getCellX(X) + Inset, getCellY(Y) + Inset, DOCU_LED + 4, DOCU_CELL + DOCU_LED + 4, DOCU_COLOUR_BAND);
            }
        }
    }
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            const char* Colour = Leds[X][Y].Bit == 0 ? DOCU_COLOUR_LED_BIT_0 : DOCU_COLOUR_LED;
            drawLed(&Svg, X, Y, Colour, std::to_string(Leds[X][Y].Digit) + "." + std::to_string(Leds[X][Y].Bit));
        }
    }

    const int16_t Legend = getCellY(MAXMATRIX_ROW_NUMBER_OF_MODULE - 1) + DOCU_CELL + 56;
    Svg.addText(DOCU_LEFT, Legend, "Each LED is labelled byte.bit: byte n is digit register n of the module's MAX7219 and "
                "MatrixBuffer[8 * module + n], bit b is bit b of it.", DOCU_FONT_LEGEND, DOCU_COLOUR_TEXT, "start");
    Svg.addText(DOCU_LEFT, Legend + 22, "A blue band joins the LEDs of one byte, the darker one is its bit 0. Matrix orientation 0. "
                "Drawn by docu/DocuPictures.cpp from setDot() and the MAX7219 model.", DOCU_FONT_LEGEND, DOCU_COLOUR_TEXT, "start");
    return Svg.getDocument();
} /* drawWiring */


/******************************************************************************************************************************************************
  drawCoordinates()
******************************************************************************************************************************************************/
/*! \brief          the picture of the coordinates a sketch uses
 *  \details        this function marks which dot each bit of setColumn() and setRow() sets, as getDot() reports it,
 *                  and names the function under what it set. The coordinates are the same in every orientation once it
 *                  fits the module, so the picture is drawn in the one of FC-16 boards. It has no title and no legend:
 *                  the README explains it right below, so the picture shows the panel only.
 *
 *  \return         the SVG text
 *****************************************************************************************************************************************************/
static std::string drawCoordinates()
{
    const int16_t PanelHeight = MAXMATRIX_ROW_NUMBER_OF_MODULE * DOCU_CELL;
    DocuSvg Svg(getPictureWidth(), DOCU_COORDINATES_ABOVE + PanelHeight + DOCU_COORDINATES_BELOW, DOCU_TOP - DOCU_COORDINATES_ABOVE);
    std::string ColumnLabels[MAXMATRIX_NUMBER_OF_COLUMNS][MAXMATRIX_ROW_NUMBER_OF_MODULE];
    std::string RowLabels[MAXMATRIX_NUMBER_OF_COLUMNS][MAXMATRIX_ROW_NUMBER_OF_MODULE];
    bool Dot;

    Matrix.setModuleOrientation(MAXMATRIX_MODULE_ORIENTATION_270);
    for(byte Bit = 0; Bit < 8; Bit++) {
        Matrix.clear();
        Matrix.setColumn(DOCU_EXAMPLE_COLUMN, 1 << Bit);
        for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
            for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
                if(E_OK == Matrix.getDot(X, Y, &Dot) && Dot) ColumnLabels[X][Y] = std::to_string(Bit);
            }
        }
        Matrix.clear();
        Matrix.setRow(DOCU_EXAMPLE_ROW_MODULE, DOCU_EXAMPLE_ROW, 1 << Bit);
        for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
            for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
                if(E_OK == Matrix.getDot(X, Y, &Dot) && Dot) RowLabels[X][Y] = std::to_string(Bit);
            }
        }
    }

    drawPanel(&Svg, "");
    for(byte X = 0; X < MAXMATRIX_NUMBER_OF_COLUMNS; X++) {
        for(byte Y = 0; Y < MAXMATRIX_ROW_NUMBER_OF_MODULE; Y++) {
            if(!ColumnLabels[X][Y].empty()) drawLed(&Svg, X, Y, DOCU_COLOUR_LED, ColumnLabels[X][Y]);
            else if(!RowLabels[X][Y].empty()) drawLed(&Svg, X, Y, DOCU_COLOUR_ROW, RowLabels[X][Y]);
            else drawLed(&Svg, X, Y, DOCU_COLOUR_LED_OFF, "");
        }
    }

    const int16_t Names = DOCU_TOP + PanelHeight + 46;
    const int16_t RowModuleLeft = getCellX(DOCU_EXAMPLE_ROW_MODULE * MAXMATRIX_COLUMN_NUMBER_OF_MODULE);
    Svg.addText(getCellX(DOCU_EXAMPLE_COLUMN) + DOCU_CELL / 2, Names, "setColumn(" + std::to_string(DOCU_EXAMPLE_COLUMN) + ", value)",
                DOCU_FONT_MODULE, DOCU_COLOUR_LED, "middle", true);
    Svg.addText(RowModuleLeft + MAXMATRIX_COLUMN_NUMBER_OF_MODULE * DOCU_CELL / 2, Names, "setRow(" + std::to_string(DOCU_EXAMPLE_ROW_MODULE)
                + ", " + std::to_string(DOCU_EXAMPLE_ROW) + ", value)", DOCU_FONT_MODULE, DOCU_COLOUR_ROW, "middle", true);
    return Svg.getDocument();
} /* drawCoordinates */


/******************************************************************************************************************************************************
  readFile()
******************************************************************************************************************************************************/
/*! \brief          reads a whole file
 *  \details        this function returns an empty text for a missing file, which no picture equals
 *
 *  \param[in]      Path            the file
 *  \return         what it holds, or an empty text if it cannot be read
 *****************************************************************************************************************************************************/
static std::string readFile(const std::string& Path)
{
    std::ifstream File(Path);
    std::stringstream Content;

    Content << File.rdbuf();
    return Content.str();
} /* readFile */


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  main()
******************************************************************************************************************************************************/
/*! \brief          writes or checks the pictures
 *  \details        this function draws every picture, then writes each one or compares it with the file of that name
 *
 *  \param[in]      argc            the number of arguments, the program's name included
 *  \param[in]      argv            --write or --check, and the folder of the pictures
 *  \return         0 when all is written or up to date, 1 when a picture differs or cannot be drawn, 2 on a wrong
 *                  command line
 *****************************************************************************************************************************************************/
int main(int argc, char** argv)
{
    if(argc != 3 || (strcmp(argv[1], "--write") != 0 && strcmp(argv[1], "--check") != 0)) {
        fprintf(stderr, "usage: %s --write|--check FOLDER\n", argv[0]);
        return 2;
    }
    const bool Write = strcmp(argv[1], "--write") == 0;
    const std::string Folder = argv[2];
    byte Failed = 0;

    Mock::reset(DOCU_CHIP_SELECT_PIN, MAXMATRIX_NUMBER_OF_MODULES);
    Matrix.init();

    std::vector<std::pair<std::string, std::string>> Pictures;
    Pictures.push_back(std::make_pair("Coordinates.svg", drawCoordinates()));
    for(const auto& Each : ModuleOrientations) {
        Pictures.push_back(std::make_pair("Wiring_" + std::to_string(Each.Degrees) + ".svg", drawWiring(Each.Orientation, Each.Degrees)));
    }

    for(const auto& Picture : Pictures) {
        const std::string Path = Folder + "/" + Picture.first;
        if(Picture.second.empty()) {
            fprintf(stderr, "%s: a dot did not light exactly one LED\n", Picture.first.c_str());
            Failed++;
        } else if(Write) {
            std::ofstream(Path) << Picture.second;
        } else if(readFile(Path) != Picture.second) {
            fprintf(stderr, "%s is not what the library draws; run DocuPictures --write\n", Path.c_str());
            Failed++;
        }
    }
    return Failed == 0 ? 0 : 1;
} /* main */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
