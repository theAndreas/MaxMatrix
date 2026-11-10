// Copyright 2026 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A picture as SVG text, for the pictures in docu/.
 *
 * Written by hand rather than with a library: the pictures need three shapes,
 * and the text is checked into the repository, so it has to come out the same
 * on every host.
 */

#include "DocuSvg.h"

#include <string>

/* DocuSvg Parameter */
#define DOCU_SVG_FONT "Helvetica, Arial, sans-serif"
#define DOCU_SVG_ARROW_HEAD_LENGTH 10
#define DOCU_SVG_ARROW_HEAD_HALF_WIDTH 6

DocuSvg::DocuSvg(uint16_t width, uint16_t height, int16_t top) {
  width_ = width;
  height_ = height;
  top_ = top;
  AddRect(0, top_, width_, height_, "#ffffff");
}

std::string DocuSvg::GetDocument() const {
  std::ostringstream document;

  document << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width_
           << "\" height=\"" << height_ << "\" viewBox=\"0 " << top_ << " "
           << width_ << " " << height_ << "\" font-family=\"" << DOCU_SVG_FONT
           << "\">\n"
           << body_.str() << "</svg>\n";
  return document.str();
}

void DocuSvg::AddRect(int16_t x, int16_t y, uint16_t rect_width,
                      uint16_t rect_height, char const* fill,
                      char const* stroke, uint8_t radius) {
  body_ << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << rect_width
        << "\" height=\"" << rect_height << "\" rx=\""
        << static_cast<uint16_t>(radius) << "\" fill=\"" << fill
        << "\" stroke=\"" << stroke << "\" stroke-width=\"2\"/>\n";
}

void DocuSvg::AddText(int16_t x, int16_t y, std::string const& text,
                      uint8_t size, char const* fill, char const* anchor,
                      bool bold) {
  body_ << "  <text x=\"" << x << "\" y=\"" << y << "\" font-size=\""
        << static_cast<uint16_t>(size) << "\" fill=\"" << fill
        << "\" text-anchor=\"" << anchor << "\""
        << (bold ? " font-weight=\"bold\"" : "") << ">" << text << "</text>\n";
}

void DocuSvg::AddArrow(int16_t from_x, int16_t to_x, int16_t y,
                       int16_t thickness, char const* colour) {
  int16_t const direction = to_x < from_x ? 1 : -1;
  int16_t const head_base{
      static_cast<int16_t>(to_x + direction * DOCU_SVG_ARROW_HEAD_LENGTH)};

  body_ << "  <line x1=\"" << from_x << "\" y1=\"" << y << "\" x2=\""
        << head_base << "\" y2=\"" << y << "\" stroke=\"" << colour
        << "\" stroke-width=\"" << thickness << "\"/>\n";
  body_ << "  <polygon points=\"" << to_x << "," << y << " " << head_base << ","
        << y - DOCU_SVG_ARROW_HEAD_HALF_WIDTH << " " << head_base << ","
        << y + DOCU_SVG_ARROW_HEAD_HALF_WIDTH << "\" fill=\"" << colour
        << "\"/>\n";
}
