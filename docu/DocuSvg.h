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

/*! \brief A point of a picture, in pixels from its left and top. */
struct DocuPoint {
  int32_t x; /*!< From the left. */
  int32_t y; /*!< From the top. */
};

/*! \brief The size of a rectangle, in pixels. */
struct DocuSize {
  uint16_t width;  /*!< Across. */
  uint16_t height; /*!< Down. */
};

/*! \brief How a rectangle is painted. */
struct DocuPaint {
  char const* fill;   /*!< The fill colour, or `"none"`. */
  char const* stroke; /*!< The outline colour, or `"none"` for no outline. */
  uint8_t radius;     /*!< The radius of the corners. */
};

/*! \brief How a text is set. */
struct DocuFont {
  uint8_t size;     /*!< The font size in pixels. */
  char const* fill; /*!< The colour. */
  /*! `"start"`, `"middle"` or `"end"`: which part lies at its point. */
  char const* anchor;
  bool bold; /*!< True for bold. */
};

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
   * \param[in] corner The top left corner.
   * \param[in] size The width and the height.
   * \param[in] paint Its fill, its outline and its corners.
   */
  void AddRect(DocuPoint corner, DocuSize size, DocuPaint paint);

  /*!
   * \brief Adds a line of text, with its baseline at the point given.
   *
   * \param[in] at Where the text is anchored, on its baseline.
   * \param[in] text The text.
   * \param[in] font How it is set.
   * \pre `text` holds no `&`, `<` or `>`.
   */
  void AddText(DocuPoint at, std::string const& text, DocuFont font);

  /*!
   * \brief Adds a horizontal arrow: a line from `from` to `to_x` at the height
   *        of `from`, and a head at `to_x`.
   *
   * \param[in] from Where the arrow starts.
   * \param[in] to_x Where its head points to.
   * \param[in] thickness The width of the line.
   * \param[in] colour The colour of line and head.
   */
  void AddArrow(DocuPoint from, int32_t to_x, int32_t thickness,
                char const* colour);
};

#endif  // MAXMATRIX_DOCU_DOCUSVG_H_
