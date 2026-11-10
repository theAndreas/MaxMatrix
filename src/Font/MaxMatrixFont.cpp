// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The font of the MaxMatrix library.
 *
 * The sprite table, which char has which sprite, and the decoding of UTF-8 into
 * latin-1 chars.
 */

#include "MaxMatrixFont.h"

#include "MaxMatrixConfig.h"

/* MaxMatrixFont Parameter, the bytes of a font's header */
#define MAXMATRIX_FONT_FILE_INDICATOR 'F'
#define MAXMATRIX_FONT_VERSION_1 1
#define MAXMATRIX_FONT_VERSION_2 2
#define MAXMATRIX_FONT_HEADER_LENGTH_1 5
#define MAXMATRIX_FONT_HEADER_LENGTH_2 7

/*
 * The sprites of the font MaxMatrix had from the start, converted once: the
 * ASCII chars at their codes, the chars beyond it at their latin-1 codes, and a
 * width of 0 for every code between them that has no char. As a table of 7
 * bytes per char it took 721 bytes. The header gives its length, so that
 * SetFont() can check it as any other font; a byte too few would be padded with
 * a 0, which the test DefaultFontHasAsciiAndNothingBetween finds, as the chars
 * then no longer end the array.
 */
uint8_t const kMaxMatrixDefaultFont[MAXMATRIX_DEFAULT_FONT_SIZE] PROGMEM = {
    // A glyph a line, with its char beside it: laid out by hand, not by
    // clang-format.
    // clang-format off
    'F', 1, 32, 252, 8, // version 1, chars 32 to 252, 8 rows high
    3, B00000000, B00000000, B00000000, // 32 space
    1, B01011111, // 33 !
    3, B00000011, B00000000, B00000011, // 34 "
    5, B00010100, B00111110, B00010100, B00111110, B00010100, // 35 #
    4, B00100100, B01101010, B00101011, B00010010, // 36 $
    5, B01100011, B00010011, B00001000, B01100100, B01100011, // 37 %
    5, B00110110, B01001001, B01010110, B00100000, B01010000, // 38 &
    1, B00000011, // 39 '
    3, B00011100, B00100010, B01000001, // 40 (
    3, B01000001, B00100010, B00011100, // 41 )
    5, B00101000, B00011000, B00001110, B00011000, B00101000, // 42 *
    5, B00001000, B00001000, B00111110, B00001000, B00001000, // 43 +
    2, B10110000, B01110000, // 44 ,
    4, B00001000, B00001000, B00001000, B00001000, // 45 -
    2, B01100000, B01100000, // 46 .
    4, B01100000, B00011000, B00000110, B00000001, // 47 /
    4, B00111110, B01000001, B01000001, B00111110, // 48 0
    3, B01000010, B01111111, B01000000, // 49 1
    4, B01100010, B01010001, B01001001, B01000110, // 50 2
    4, B00100010, B01000001, B01001001, B00110110, // 51 3
    4, B00011000, B00010100, B00010010, B01111111, // 52 4
    4, B00100111, B01000101, B01000101, B00111001, // 53 5
    4, B00111110, B01001001, B01001001, B00110000, // 54 6
    4, B01100001, B00010001, B00001001, B00000111, // 55 7
    4, B00110110, B01001001, B01001001, B00110110, // 56 8
    4, B00000110, B01001001, B01001001, B00111110, // 57 9
    2, B01010000, B00000000, // 58 :
    2, B10000000, B01010000, // 59 ;
    3, B00010000, B00101000, B01000100, // 60 <
    3, B00010100, B00010100, B00010100, // 61 =
    3, B01000100, B00101000, B00010000, // 62 >
    4, B00000010, B01011001, B00001001, B00000110, // 63 ?
    5, B00111110, B01001001, B01010101, B01011101, B00001110, // 64 @
    4, B01111110, B00010001, B00010001, B01111110, // 65 A
    4, B01111111, B01001001, B01001001, B00110110, // 66 B
    4, B00111110, B01000001, B01000001, B00100010, // 67 C
    4, B01111111, B01000001, B01000001, B00111110, // 68 D
    4, B01111111, B01001001, B01001001, B01000001, // 69 E
    4, B01111111, B00001001, B00001001, B00000001, // 70 F
    4, B00111110, B01000001, B01001001, B01111010, // 71 G
    4, B01111111, B00001000, B00001000, B01111111, // 72 H
    3, B01000001, B01111111, B01000001, // 73 I
    4, B00110000, B01000000, B01000001, B00111111, // 74 J
    4, B01111111, B00001000, B00010100, B01100011, // 75 K
    4, B01111111, B01000000, B01000000, B01000000, // 76 L
    5, B01111111, B00000010, B00001100, B00000010, B01111111, // 77 M
    5, B01111111, B00000100, B00001000, B00010000, B01111111, // 78 N
    4, B00111110, B01000001, B01000001, B00111110, // 79 O
    4, B01111111, B00001001, B00001001, B00000110, // 80 P
    4, B00111110, B01000001, B01000001, B10111110, // 81 Q
    4, B01111111, B00001001, B00001001, B01110110, // 82 R
    4, B01000110, B01001001, B01001001, B00110010, // 83 S
    5, B00000001, B00000001, B01111111, B00000001, B00000001, // 84 T
    4, B00111111, B01000000, B01000000, B00111111, // 85 U
    5, B00001111, B00110000, B01000000, B00110000, B00001111, // 86 V
    5, B00111111, B01000000, B00111000, B01000000, B00111111, // 87 W
    5, B01100011, B00010100, B00001000, B00010100, B01100011, // 88 X
    5, B00000111, B00001000, B01110000, B00001000, B00000111, // 89 Y
    4, B01100001, B01010001, B01001001, B01000111, // 90 Z
    2, B01111111, B01000001, // 91 [
    4, B00000001, B00000110, B00011000, B01100000, // 92 \ backslash
    2, B01000001, B01111111, // 93 ]
    3, B00000010, B00000001, B00000010, // 94 ^
    4, B01000000, B01000000, B01000000, B01000000, // 95 _
    2, B00000001, B00000010, // 96 `
    4, B00100000, B01010100, B01010100, B01111000, // 97 a
    4, B01111111, B01000100, B01000100, B00111000, // 98 b
    4, B00111000, B01000100, B01000100, B00101000, // 99 c
    4, B00111000, B01000100, B01000100, B01111111, // 100 d
    4, B00111000, B01010100, B01010100, B00011000, // 101 e
    3, B00000100, B01111110, B00000101, // 102 f
    4, B10011000, B10100100, B10100100, B01111000, // 103 g
    4, B01111111, B00000100, B00000100, B01111000, // 104 h
    3, B01000100, B01111101, B01000000, // 105 i
    4, B01000000, B10000000, B10000100, B01111101, // 106 j
    4, B01111111, B00010000, B00101000, B01000100, // 107 k
    3, B01000001, B01111111, B01000000, // 108 l
    5, B01111100, B00000100, B01111100, B00000100, B01111000, // 109 m
    4, B01111100, B00000100, B00000100, B01111000, // 110 n
    4, B00111000, B01000100, B01000100, B00111000, // 111 o
    4, B11111100, B00100100, B00100100, B00011000, // 112 p
    4, B00011000, B00100100, B00100100, B11111100, // 113 q
    4, B01111100, B00001000, B00000100, B00000100, // 114 r
    4, B01001000, B01010100, B01010100, B00100100, // 115 s
    3, B00000100, B00111111, B01000100, // 116 t
    4, B00111100, B01000000, B01000000, B01111100, // 117 u
    5, B00011100, B00100000, B01000000, B00100000, B00011100, // 118 v
    5, B00111100, B01000000, B00111100, B01000000, B00111100, // 119 w
    5, B01000100, B00101000, B00010000, B00101000, B01000100, // 120 x
    4, B10011100, B10100000, B10100000, B01111100, // 121 y
    3, B01100100, B01010100, B01001100, // 122 z
    3, B00001000, B00110110, B01000001, // 123 {
    1, B01111111, // 124 |
    3, B01000001, B00110110, B00001000, // 125 }
    4, B00001000, B00000100, B00001000, B00000100, // 126 ~
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 127 to 175, none
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,
    3, B00000010, B00000101, B00000010, // 176 °
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 177 to 195, none
    0, 0, 0,
    4, B01111100, B00010011, B00010011, B01111100, // 196 Ä
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 197 to 213, none
    0,
    4, B00111100, B01000011, B01000011, B00111100, // 214 Ö
    0, 0, 0, 0, 0, // 215 to 219, none
    4, B00111101, B01000000, B01000000, B00111101, // 220 Ü
    0, 0, // 221 to 222, none
    4, B11111110, B00000001, B01001001, B00110110, // 223 ß
    0, 0, 0, 0, // 224 to 227, none
    4, B00100000, B01010110, B01010110, B01111000, // 228 ä
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // 229 to 245, none
    0,
    4, B00111000, B01000110, B01000110, B00111000, // 246 ö
    0, 0, 0, 0, 0, // 247 to 251, none
    4, B00111100, B01000010, B01000010, B01111100, // 252 ü
    // clang-format on
};

/*
 * The chars are walked by their offset into the array rather than by a pointer,
 * as a pointer more than one past the end of the array would be undefined
 * behaviour already, compared or not.
 */
MaxMatrixStatus MaxMatrixFont::CheckFont(uint8_t const* font, size_t size) {
  if (font == NULL || size < MAXMATRIX_FONT_HEADER_LENGTH_1 ||
      pgm_read_byte(font) != MAXMATRIX_FONT_FILE_INDICATOR) {
    return MaxMatrixError::kNotAFont;
  }

  byte const version = pgm_read_byte(font + 1);
  uint16_t first;
  uint16_t last;
  byte height;
  size_t offset;

  if (version == MAXMATRIX_FONT_VERSION_1) {
    first = pgm_read_byte(font + 2);
    last = pgm_read_byte(font + 3);
    height = pgm_read_byte(font + 4);
    offset = MAXMATRIX_FONT_HEADER_LENGTH_1;
  } else if (version == MAXMATRIX_FONT_VERSION_2 &&
             size >= MAXMATRIX_FONT_HEADER_LENGTH_2) {
    first = static_cast<uint16_t>((pgm_read_byte(font + 2) << 8) |
                                  pgm_read_byte(font + 3));
    last = static_cast<uint16_t>((pgm_read_byte(font + 4) << 8) |
                                 pgm_read_byte(font + 5));
    height = pgm_read_byte(font + 6);
    offset = MAXMATRIX_FONT_HEADER_LENGTH_2;
  } else
    return MaxMatrixError::kNotAFont;
  if (first > last || height < 1 || height > MAXMATRIX_ROW_NUMBER_OF_MODULE)
    return MaxMatrixError::kNotAFont;
  /* the loop ends at the last char rather than past it, as a last char of
   * 0xFFFF has no next */
  for (uint16_t code = first;; code++) {
    if (offset >= size) return MaxMatrixError::kNotAFont;
    offset += pgm_read_byte(font + offset) + 1u;
    if (code == last) break;
  }
  return offset == size ? MaxMatrixError::kOk : MaxMatrixError::kNotAFont;
}

/*
 * Walks from the first char of the font to `c`, each char its width and that
 * many columns on. The header is read again on every call rather than kept:
 * five bytes from flash cost less than the RAM to keep them.
 */
MaxMatrixResult<MaxMatrixGlyph> MaxMatrixFont::GetGlyph(uint8_t const* font,
                                                        char c) {
  bool const version_1 = pgm_read_byte(font + 1) == MAXMATRIX_FONT_VERSION_1;
  uint16_t const code = static_cast<byte>(c);
  uint16_t first;
  uint16_t last;
  MaxMatrixGlyph glyph;

  if (version_1) {
    first = pgm_read_byte(font + 2);
    last = pgm_read_byte(font + 3);
    glyph.height = pgm_read_byte(font + 4);
    font += MAXMATRIX_FONT_HEADER_LENGTH_1;
  } else {
    first = static_cast<uint16_t>((pgm_read_byte(font + 2) << 8) |
                                  pgm_read_byte(font + 3));
    last = static_cast<uint16_t>((pgm_read_byte(font + 4) << 8) |
                                 pgm_read_byte(font + 5));
    glyph.height = pgm_read_byte(font + 6);
    font += MAXMATRIX_FONT_HEADER_LENGTH_2;
  }
  if (code < first || code > last) return MaxMatrixError::kUnknownChar;
  for (uint16_t skipped = first; skipped < code; skipped++)
    font += pgm_read_byte(font) + 1;
  glyph.width = pgm_read_byte(font);
  glyph.columns = font + 1;
  if (glyph.width == 0) return MaxMatrixError::kUnknownChar;
  return glyph;
}

byte MaxMatrixFont::DecodeChar(char const* string, char* c) {
  byte lead_byte = string[0];
  byte next_byte = string[1];

  /* two byte UTF-8 sequence of a char from U+0080 to U+00FF */
  if ((0xC2 == lead_byte || 0xC3 == lead_byte) && 0x80 == (next_byte & 0xC0)) {
    *c = static_cast<char>(((lead_byte & 0x03) << 6) | (next_byte & 0x3F));
    return 2;
  } else {
    *c = static_cast<char>(lead_byte);
    return 1;
  }
}
