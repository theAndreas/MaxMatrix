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

namespace {

/* DocuSvg Parameter */
constexpr char kFont[] = "Helvetica, Arial, sans-serif";
constexpr int32_t kArrowHeadLength{10};
constexpr int32_t kArrowHeadHalfWidth{6};

}  // namespace

DocuSvg::DocuSvg(uint16_t width, uint16_t height, int32_t top) {
  width_ = width;
  height_ = height;
  top_ = top;
  AddRect(0, top_, width_, height_, "#ffffff");
}

std::string DocuSvg::GetDocument() const {
  std::ostringstream document;

  document << R"(<svg xmlns="http://www.w3.org/2000/svg" width=")" << width_
           << "\" height=\"" << height_ << "\" viewBox=\"0 " << top_ << " "
           << width_ << " " << height_ << "\" font-family=\"" << kFont
           << "\">\n"
           << body_.str() << "</svg>\n";
  return document.str();
}

void DocuSvg::AddRect(int32_t x, int32_t y, uint16_t rect_width,
                      uint16_t rect_height, char const* fill,
                      char const* stroke, uint8_t radius) {
  body_ << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << rect_width
        << "\" height=\"" << rect_height << "\" rx=\""
        << static_cast<uint16_t>(radius) << "\" fill=\"" << fill
        << "\" stroke=\"" << stroke << "\" stroke-width=\"2\"/>\n";
}

void DocuSvg::AddText(int32_t x, int32_t y, std::string const& text,
                      uint8_t size, char const* fill, char const* anchor,
                      bool bold) {
  body_ << "  <text x=\"" << x << "\" y=\"" << y << "\" font-size=\""
        << static_cast<uint16_t>(size) << "\" fill=\"" << fill
        << "\" text-anchor=\"" << anchor << "\""
        << (bold ? " font-weight=\"bold\"" : "") << ">" << text << "</text>\n";
}

void DocuSvg::AddArrow(int32_t from_x, int32_t to_x, int32_t y,
                       int32_t thickness, char const* colour) {
  int32_t const direction = to_x < from_x ? 1 : -1;
  int32_t const head_base{to_x + direction * kArrowHeadLength};

  body_ << "  <line x1=\"" << from_x << "\" y1=\"" << y << "\" x2=\""
        << head_base << "\" y2=\"" << y << "\" stroke=\"" << colour
        << "\" stroke-width=\"" << thickness << "\"/>\n";
  body_ << "  <polygon points=\"" << to_x << "," << y << " " << head_base << ","
        << y - kArrowHeadHalfWidth << " " << head_base << ","
        << y + kArrowHeadHalfWidth << "\" fill=\"" << colour << "\"/>\n";
}
