// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A picture as SVG text, for the pictures in docu/.
 *
 * Rectangles, text and arrows, and nothing else the pictures need. On a white background, so that
 * the dark text stays readable where GitHub shows the page dark.
 */

#ifndef MAXMATRIX_DOCU_DOCUSVG_H_
#define MAXMATRIX_DOCU_DOCUSVG_H_

#include <stdint.h>
#include <sstream>
#include <string>

class DocuSvg
{
  private:
    uint16_t width_;
    uint16_t height_;
    int16_t top_;
    std::ostringstream body_;

  public:
    DocuSvg(uint16_t, uint16_t, int16_t = 0);

    // get methods
    std::string GetDocument() const;

    // methods
    void AddRect(int16_t, int16_t, uint16_t, uint16_t, char const*, char const* = "none", uint8_t = 0);
    void AddText(int16_t, int16_t, std::string const&, uint8_t, char const* = "#222222", char const* = "middle", bool = false);
    void AddArrow(int16_t, int16_t, int16_t, int16_t, char const*);
};

#endif
