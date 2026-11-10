// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief A picture as SVG text, for the pictures in docu/.
 *
 * Rectangles, text and arrows, and nothing else the pictures need. On a white
 * background, so that the dark text stays readable where GitHub shows the page
 * dark.
 */

#ifndef MAXMATRIX_DOCU_DOCUSVG_H_
#define MAXMATRIX_DOCU_DOCUSVG_H_

#include <stdint.h>

#include <sstream>
#include <string>

/*! \brief A picture as SVG text, for the pictures in docu/. */
class DocuSvg {
 private:
  uint16_t width_;
  uint16_t height_;
  int32_t top_;
  std::ostringstream body_;

 public:
  /*!
   * \brief Makes an empty picture with a white background.
   *
   * A picture that leaves out what other pictures have at the top shows its
   * shapes from `top` on, so that both can share one layout.
   *
   * \param[in] width The width in pixels.
   * \param[in] height The height in pixels.
   * \param[in] top The coordinate shown at the top edge.
   */
  DocuSvg(uint16_t width, uint16_t height, int32_t top = 0);

  /*!
   * \brief Returns the picture as an SVG document.
   *
   * What was added is wrapped into the `svg` element, with a line break at the
   * end.
   *
   * \return The SVG text.
   */
  std::string GetDocument() const;

  /*!
   * \brief Adds a rectangle, with an outline 2 pixels wide where it has one.
   *
   * \param[in] x The left edge.
   * \param[in] y The top edge.
   * \param[in] rect_width The width.
   * \param[in] rect_height The height.
   * \param[in] fill The fill colour, or `"none"`.
   * \param[in] stroke The outline colour, or `"none"` for no outline.
   * \param[in] radius The radius of the corners.
   */
  void AddRect(int32_t x, int32_t y, uint16_t rect_width, uint16_t rect_height,
               char const* fill, char const* stroke = "none",
               uint8_t radius = 0);

  /*!
   * \brief Adds a line of text, with its baseline at `y`.
   *
   * \param[in] x Where the text is anchored.
   * \param[in] y The baseline.
   * \param[in] text The text.
   * \param[in] size The font size in pixels.
   * \param[in] fill The colour.
   * \param[in] anchor `"start"`, `"middle"` or `"end"`: which part of the text
   *                   lies at `x`.
   * \param[in] bold True for bold.
   * \pre `text` holds no `&`, `<` or `>`.
   */
  void AddText(int32_t x, int32_t y, std::string const& text, uint8_t size,
               char const* fill = "#222222", char const* anchor = "middle",
               bool bold = false);

  /*!
   * \brief Adds a horizontal arrow: a line from `from_x` to `to_x` at height
   *        `y`, and a head at `to_x`.
   *
   * \param[in] from_x Where the arrow starts.
   * \param[in] to_x Where its head points to.
   * \param[in] y The height of the arrow.
   * \param[in] thickness The width of the line.
   * \param[in] colour The colour of line and head.
   */
  void AddArrow(int32_t from_x, int32_t to_x, int32_t y, int32_t thickness,
                char const* colour);
};

#endif  // MAXMATRIX_DOCU_DOCUSVG_H_
