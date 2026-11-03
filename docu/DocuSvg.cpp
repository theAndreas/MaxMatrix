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
/**     \file       DocuSvg.cpp
 *      \brief      A picture as SVG text, for the pictures in docu/
 *
 *      \details    Written by hand rather than with a library: the pictures need three shapes, and the text is checked
 *                  into the repository, so it has to come out the same on every host.
 *
 *****************************************************************************************************************************************************/
/******************************************************************************************************************************************************
 * INCLUDES
 *****************************************************************************************************************************************************/
#include "DocuSvg.h"


/******************************************************************************************************************************************************
 *  LOCAL CONSTANT MACROS
 *****************************************************************************************************************************************************/
/* DocuSvg Parameter */
#define DOCU_SVG_FONT                                               "Helvetica, Arial, sans-serif"
#define DOCU_SVG_ARROW_HEAD_LENGTH                                  10
#define DOCU_SVG_ARROW_HEAD_HALF_WIDTH                              6


/******************************************************************************************************************************************************
 * P U B L I C   F U N C T I O N S
 *****************************************************************************************************************************************************/

/******************************************************************************************************************************************************
  CONSTRUCTOR OF DocuSvg
******************************************************************************************************************************************************/
/*! \brief          DocuSvg constructor
 *  \details        Instantiation of an empty picture with a white background. A picture that leaves out what other
 *                  pictures have at the top shows its shapes from top on, so that both can share one layout.
 *
 *  \param[in]      width           width in pixels
 *  \param[in]      height          height in pixels
 *  \param[in]      top             the coordinate shown at the top edge
 *****************************************************************************************************************************************************/
DocuSvg::DocuSvg(uint16_t width, uint16_t height, int16_t top)
{
    width_ = width;
    height_ = height;
    top_ = top;
    AddRect(0, top_, width_, height_, "#ffffff");
} /* DocuSvg */


/******************************************************************************************************************************************************
  GetDocument()
******************************************************************************************************************************************************/
/*! \brief          the picture as an SVG document
 *  \details        this function wraps what was added into the svg element, with a line break at the end
 *
 *  \return         the SVG text
 *****************************************************************************************************************************************************/
std::string DocuSvg::GetDocument() const
{
    std::ostringstream document;

    document << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width_ << "\" height=\"" << height_
             << "\" viewBox=\"0 " << top_ << " " << width_ << " " << height_ << "\" font-family=\"" << DOCU_SVG_FONT << "\">\n"
             << body_.str() << "</svg>\n";
    return document.str();
} /* GetDocument */


/******************************************************************************************************************************************************
  AddRect()
******************************************************************************************************************************************************/
/*! \brief          adds a rectangle
 *  \details        this function draws a rectangle with an outline 2 pixels wide where it has one
 *
 *  \param[in]      x               left edge
 *  \param[in]      y               top edge
 *  \param[in]      rect_width      width
 *  \param[in]      rect_height     height
 *  \param[in]      fill            fill colour, or "none"
 *  \param[in]      stroke          outline colour, or "none" for no outline
 *  \param[in]      radius          radius of the corners
 *****************************************************************************************************************************************************/
void DocuSvg::AddRect(int16_t x, int16_t y, uint16_t rect_width, uint16_t rect_height, char const* fill, char const* stroke, uint8_t radius)
{
    body_ << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << rect_width << "\" height=\"" << rect_height
         << "\" rx=\"" << static_cast<uint16_t>(radius) << "\" fill=\"" << fill << "\" stroke=\"" << stroke << "\" stroke-width=\"2\"/>\n";
} /* AddRect */


/******************************************************************************************************************************************************
  AddText()
******************************************************************************************************************************************************/
/*! \brief          adds a line of text
 *  \details        this function places the text with its baseline at Y; the text must not hold &, < or >
 *
 *  \param[in]      x               where the text is anchored
 *  \param[in]      y               baseline
 *  \param[in]      text            the text
 *  \param[in]      size            font size in pixels
 *  \param[in]      fill            colour
 *  \param[in]      anchor          "start", "middle" or "end": which part of the text lies at X
 *  \param[in]      bold            true for bold
 *****************************************************************************************************************************************************/
void DocuSvg::AddText(int16_t x, int16_t y, std::string const& text, uint8_t size, char const* fill, char const* anchor, bool bold)
{
    body_ << "  <text x=\"" << x << "\" y=\"" << y << "\" font-size=\"" << static_cast<uint16_t>(size) << "\" fill=\"" << fill
         << "\" text-anchor=\"" << anchor << "\"" << (bold ? " font-weight=\"bold\"" : "") << ">" << text << "</text>\n";
} /* AddText */


/******************************************************************************************************************************************************
  AddArrow()
******************************************************************************************************************************************************/
/*! \brief          adds a horizontal arrow
 *  \details        this function draws a line from from_x to to_x at height Y and a head at to_x
 *
 *  \param[in]      from_x          where the arrow starts
 *  \param[in]      to_x            where its head points to
 *  \param[in]      y               height of the arrow
 *  \param[in]      thickness       width of the line
 *  \param[in]      colour          colour of line and head
 *****************************************************************************************************************************************************/
void DocuSvg::AddArrow(int16_t from_x, int16_t to_x, int16_t y, int16_t thickness, char const* colour)
{
    int16_t const direction = to_x < from_x ? 1 : -1;
    int16_t const head_base = to_x + direction * DOCU_SVG_ARROW_HEAD_LENGTH;

    body_ << "  <line x1=\"" << from_x << "\" y1=\"" << y << "\" x2=\"" << head_base << "\" y2=\"" << y << "\" stroke=\"" << colour
         << "\" stroke-width=\"" << thickness << "\"/>\n";
    body_ << "  <polygon points=\"" << to_x << "," << y << " " << head_base << "," << y - DOCU_SVG_ARROW_HEAD_HALF_WIDTH << " " << head_base << ","
         << y + DOCU_SVG_ARROW_HEAD_HALF_WIDTH << "\" fill=\"" << colour << "\"/>\n";
} /* AddArrow */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
