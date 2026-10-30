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
#define _DOCU_SVG_SOURCE_

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
 *                  pictures have at the top shows its shapes from sTop on, so that both can share one layout.
 *
 *  \param[in]      sWidth          width in pixels
 *  \param[in]      sHeight         height in pixels
 *  \param[in]      sTop            the coordinate shown at the top edge
 *  \return         -
 *****************************************************************************************************************************************************/
DocuSvg::DocuSvg(uint16_t sWidth, uint16_t sHeight, int16_t sTop)
{
    Width = sWidth;
    Height = sHeight;
    Top = sTop;
    addRect(0, Top, Width, Height, "#ffffff");
} /* DocuSvg */


/******************************************************************************************************************************************************
  getDocument()
******************************************************************************************************************************************************/
/*! \brief          the picture as an SVG document
 *  \details        this function wraps what was added into the svg element, with a line break at the end
 *
 *  \return         the SVG text
 *****************************************************************************************************************************************************/
std::string DocuSvg::getDocument() const
{
    std::ostringstream Document;

    Document << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << Width << "\" height=\"" << Height
             << "\" viewBox=\"0 " << Top << " " << Width << " " << Height << "\" font-family=\"" << DOCU_SVG_FONT << "\">\n"
             << Body.str() << "</svg>\n";
    return Document.str();
} /* getDocument */


/******************************************************************************************************************************************************
  addRect()
******************************************************************************************************************************************************/
/*! \brief          adds a rectangle
 *  \details        this function draws a rectangle with an outline 2 pixels wide where it has one
 *
 *  \param[in]      X               left edge
 *  \param[in]      Y               top edge
 *  \param[in]      RectWidth       width
 *  \param[in]      RectHeight      height
 *  \param[in]      Fill            fill colour, or "none"
 *  \param[in]      Stroke          outline colour, or "none" for no outline
 *  \param[in]      Radius          radius of the corners
 *  \return         -
 *****************************************************************************************************************************************************/
void DocuSvg::addRect(int16_t X, int16_t Y, uint16_t RectWidth, uint16_t RectHeight, const char* Fill, const char* Stroke, uint8_t Radius)
{
    Body << "  <rect x=\"" << X << "\" y=\"" << Y << "\" width=\"" << RectWidth << "\" height=\"" << RectHeight
         << "\" rx=\"" << static_cast<uint16_t>(Radius) << "\" fill=\"" << Fill << "\" stroke=\"" << Stroke << "\" stroke-width=\"2\"/>\n";
} /* addRect */


/******************************************************************************************************************************************************
  addText()
******************************************************************************************************************************************************/
/*! \brief          adds a line of text
 *  \details        this function places the text with its baseline at Y; the text must not hold &, < or >
 *
 *  \param[in]      X               where the text is anchored
 *  \param[in]      Y               baseline
 *  \param[in]      Text            the text
 *  \param[in]      Size            font size in pixels
 *  \param[in]      Fill            colour
 *  \param[in]      Anchor          "start", "middle" or "end": which part of the text lies at X
 *  \param[in]      Bold            true for bold
 *  \return         -
 *****************************************************************************************************************************************************/
void DocuSvg::addText(int16_t X, int16_t Y, const std::string& Text, uint8_t Size, const char* Fill, const char* Anchor, bool Bold)
{
    Body << "  <text x=\"" << X << "\" y=\"" << Y << "\" font-size=\"" << static_cast<uint16_t>(Size) << "\" fill=\"" << Fill
         << "\" text-anchor=\"" << Anchor << "\"" << (Bold ? " font-weight=\"bold\"" : "") << ">" << Text << "</text>\n";
} /* addText */


/******************************************************************************************************************************************************
  addArrow()
******************************************************************************************************************************************************/
/*! \brief          adds a horizontal arrow
 *  \details        this function draws a line from FromX to ToX at height Y and a head at ToX
 *
 *  \param[in]      FromX           where the arrow starts
 *  \param[in]      ToX             where its head points to
 *  \param[in]      Y               height of the arrow
 *  \param[in]      Thickness       width of the line
 *  \param[in]      Colour          colour of line and head
 *  \return         -
 *****************************************************************************************************************************************************/
void DocuSvg::addArrow(int16_t FromX, int16_t ToX, int16_t Y, int16_t Thickness, const char* Colour)
{
    const int16_t Direction = ToX < FromX ? 1 : -1;
    const int16_t HeadBase = ToX + Direction * DOCU_SVG_ARROW_HEAD_LENGTH;

    Body << "  <line x1=\"" << FromX << "\" y1=\"" << Y << "\" x2=\"" << HeadBase << "\" y2=\"" << Y << "\" stroke=\"" << Colour
         << "\" stroke-width=\"" << Thickness << "\"/>\n";
    Body << "  <polygon points=\"" << ToX << "," << Y << " " << HeadBase << "," << Y - DOCU_SVG_ARROW_HEAD_HALF_WIDTH << " " << HeadBase << ","
         << Y + DOCU_SVG_ARROW_HEAD_HALF_WIDTH << "\" fill=\"" << Colour << "\"/>\n";
} /* addArrow */


/******************************************************************************************************************************************************
 *  E N D   O F   F I L E
 *****************************************************************************************************************************************************/
