// Copyright 2020 Andreas Burnickl
// SPDX-License-Identifier: GPL-3.0-or-later

/*!
 * \file
 * \brief The font of the MaxMatrix library.
 *
 * The sprite table, which char has which sprite, and the decoding of UTF-8 into
 * latin-1 chars.
 */

#include "Font.h"

#include <stddef.h>
#include <stdint.h>

#include "MaxMatrixConfig.h"
#include "MaxMatrixError.h"
#include "MaxMatrixPgmspace.h"
#include "Utility/Bit.h"
#include "Utility/array.h"
#include "Utility/span.h"

namespace max_matrix {

namespace {

/* the bytes of a font's header */
constexpr uint8_t kFileIndicator{'F'};
constexpr uint8_t kVersion1{1};
constexpr uint8_t kVersion2{2};
constexpr uint8_t kHeaderLength1{5};
constexpr uint8_t kHeaderLength2{7};

/* where the header holds what: version 1 gives each code in a byte, version 2
 * in two */
constexpr size_t kVersionPosition{1};
constexpr size_t kFirstPosition{2};
constexpr size_t kLastPosition1{3};
constexpr size_t kHeightPosition1{4};
constexpr size_t kLastPosition2{4};
constexpr size_t kHeightPosition2{6};

/* the bits of a two byte UTF-8 sequence: the lead byte carries the two high
 * bits of the char, the continuation byte, 10xxxxxx, the six low ones */
constexpr uint8_t kUtf8ContinuationMask{0xC0u};
constexpr uint8_t kUtf8ContinuationTag{0x80u};
constexpr uint8_t kUtf8ContinuationBits{0x3Fu};
constexpr uint8_t kUtf8ContinuationWidth{6u};

// Reads a number of two bytes from flash, the high byte first, as the header
// of a font of version 2 holds it.
uint16_t ReadNumber(span<uint8_t const> font, size_t position) {
  uint16_t const high{
      Bit<uint16_t>::ShiftUp(pgm_read_byte(&font[position]), 8u)};
  return Bit<uint16_t>::Combine(high, pgm_read_byte(&font[position + 1]));
}

// Reads the byte of the font at `position` from flash.
uint8_t ReadByte(span<uint8_t const> font, size_t position) {
  return pgm_read_byte(&font[position]);
}

/* what the header of a font says: its first and last code, the height of its
 * chars, and where the width of its first char lies */
struct FontHeader {
  uint16_t first;
  uint16_t last;
  uint8_t height;
  size_t offset;
};

// Reads the header of `font`; one that is not of version 1 is read as one of
// version 2, as CheckFont() takes no other.
FontHeader ReadHeader(span<uint8_t const> font) {
  if (ReadByte(font, kVersionPosition) == kVersion1) {
    return FontHeader{ReadByte(font, kFirstPosition),
                      ReadByte(font, kLastPosition1),
                      ReadByte(font, kHeightPosition1), kHeaderLength1};
  }
  return FontHeader{ReadNumber(font, kFirstPosition),
                    ReadNumber(font, kLastPosition2),
                    ReadByte(font, kHeightPosition2), kHeaderLength2};
}

}  // namespace

/*
 * The sprites of the font MaxMatrix had from the start, converted once: the
 * ASCII chars at their codes, the chars beyond it at their latin-1 codes, and a
 * width of 0 for every code between them that has no char. As a table of 7
 * bytes per char it took 721 bytes. The header gives its length, so that
 * SetFont() can check it as any other font. make_array() counts the bytes, and
 * an array of another count than kDefaultFontSize does not compile.
 */
array<uint8_t, kDefaultFontSize> const kDefaultFont PROGMEM{make_array<uint8_t>(
    // A glyph a line, with its char beside it: laid out by hand, not by
    // clang-format.
    // clang-format off
    'F', 1, 32, 252, 8,  // version 1, chars 32 to 252, 8 rows high
    3, 0b00000000, 0b00000000, 0b00000000,  // 32 space
    1, 0b01011111,  // 33 !
    3, 0b00000011, 0b00000000, 0b00000011,  // 34 "
    5, 0b00010100, 0b00111110, 0b00010100, 0b00111110, 0b00010100,  // 35 #
    4, 0b00100100, 0b01101010, 0b00101011, 0b00010010,  // 36 $
    5, 0b01100011, 0b00010011, 0b00001000, 0b01100100, 0b01100011,  // 37 %
    5, 0b00110110, 0b01001001, 0b01010110, 0b00100000, 0b01010000,  // 38 &
    1, 0b00000011,  // 39 '
    3, 0b00011100, 0b00100010, 0b01000001,  // 40 (
    3, 0b01000001, 0b00100010, 0b00011100,  // 41 )
    5, 0b00101000, 0b00011000, 0b00001110, 0b00011000, 0b00101000,  // 42 *
    5, 0b00001000, 0b00001000, 0b00111110, 0b00001000, 0b00001000,  // 43 +
    2, 0b10110000, 0b01110000,  // 44 ,
    4, 0b00001000, 0b00001000, 0b00001000, 0b00001000,  // 45 -
    2, 0b01100000, 0b01100000,  // 46 .
    4, 0b01100000, 0b00011000, 0b00000110, 0b00000001,  // 47 /
    4, 0b00111110, 0b01000001, 0b01000001, 0b00111110,  // 48 0
    3, 0b01000010, 0b01111111, 0b01000000,  // 49 1
    4, 0b01100010, 0b01010001, 0b01001001, 0b01000110,  // 50 2
    4, 0b00100010, 0b01000001, 0b01001001, 0b00110110,  // 51 3
    4, 0b00011000, 0b00010100, 0b00010010, 0b01111111,  // 52 4
    4, 0b00100111, 0b01000101, 0b01000101, 0b00111001,  // 53 5
    4, 0b00111110, 0b01001001, 0b01001001, 0b00110000,  // 54 6
    4, 0b01100001, 0b00010001, 0b00001001, 0b00000111,  // 55 7
    4, 0b00110110, 0b01001001, 0b01001001, 0b00110110,  // 56 8
    4, 0b00000110, 0b01001001, 0b01001001, 0b00111110,  // 57 9
    2, 0b01010000, 0b00000000,  // 58 :
    2, 0b10000000, 0b01010000,  // 59 ;
    3, 0b00010000, 0b00101000, 0b01000100,  // 60 <
    3, 0b00010100, 0b00010100, 0b00010100,  // 61 =
    3, 0b01000100, 0b00101000, 0b00010000,  // 62 >
    4, 0b00000010, 0b01011001, 0b00001001, 0b00000110,  // 63 ?
    5, 0b00111110, 0b01001001, 0b01010101, 0b01011101, 0b00001110,  // 64 @
    4, 0b01111110, 0b00010001, 0b00010001, 0b01111110,  // 65 A
    4, 0b01111111, 0b01001001, 0b01001001, 0b00110110,  // 66 B
    4, 0b00111110, 0b01000001, 0b01000001, 0b00100010,  // 67 C
    4, 0b01111111, 0b01000001, 0b01000001, 0b00111110,  // 68 D
    4, 0b01111111, 0b01001001, 0b01001001, 0b01000001,  // 69 E
    4, 0b01111111, 0b00001001, 0b00001001, 0b00000001,  // 70 F
    4, 0b00111110, 0b01000001, 0b01001001, 0b01111010,  // 71 G
    4, 0b01111111, 0b00001000, 0b00001000, 0b01111111,  // 72 H
    3, 0b01000001, 0b01111111, 0b01000001,  // 73 I
    4, 0b00110000, 0b01000000, 0b01000001, 0b00111111,  // 74 J
    4, 0b01111111, 0b00001000, 0b00010100, 0b01100011,  // 75 K
    4, 0b01111111, 0b01000000, 0b01000000, 0b01000000,  // 76 L
    5, 0b01111111, 0b00000010, 0b00001100, 0b00000010, 0b01111111,  // 77 M
    5, 0b01111111, 0b00000100, 0b00001000, 0b00010000, 0b01111111,  // 78 N
    4, 0b00111110, 0b01000001, 0b01000001, 0b00111110,  // 79 O
    4, 0b01111111, 0b00001001, 0b00001001, 0b00000110,  // 80 P
    4, 0b00111110, 0b01000001, 0b01000001, 0b10111110,  // 81 Q
    4, 0b01111111, 0b00001001, 0b00001001, 0b01110110,  // 82 R
    4, 0b01000110, 0b01001001, 0b01001001, 0b00110010,  // 83 S
    5, 0b00000001, 0b00000001, 0b01111111, 0b00000001, 0b00000001,  // 84 T
    4, 0b00111111, 0b01000000, 0b01000000, 0b00111111,  // 85 U
    5, 0b00001111, 0b00110000, 0b01000000, 0b00110000, 0b00001111,  // 86 V
    5, 0b00111111, 0b01000000, 0b00111000, 0b01000000, 0b00111111,  // 87 W
    5, 0b01100011, 0b00010100, 0b00001000, 0b00010100, 0b01100011,  // 88 X
    5, 0b00000111, 0b00001000, 0b01110000, 0b00001000, 0b00000111,  // 89 Y
    4, 0b01100001, 0b01010001, 0b01001001, 0b01000111,  // 90 Z
    2, 0b01111111, 0b01000001,  // 91 [
    4, 0b00000001, 0b00000110, 0b00011000, 0b01100000,  // 92 \ backslash
    2, 0b01000001, 0b01111111,  // 93 ]
    3, 0b00000010, 0b00000001, 0b00000010,  // 94 ^
    4, 0b01000000, 0b01000000, 0b01000000, 0b01000000,  // 95 _
    2, 0b00000001, 0b00000010,  // 96 `
    4, 0b00100000, 0b01010100, 0b01010100, 0b01111000,  // 97 a
    4, 0b01111111, 0b01000100, 0b01000100, 0b00111000,  // 98 b
    4, 0b00111000, 0b01000100, 0b01000100, 0b00101000,  // 99 c
    4, 0b00111000, 0b01000100, 0b01000100, 0b01111111,  // 100 d
    4, 0b00111000, 0b01010100, 0b01010100, 0b00011000,  // 101 e
    3, 0b00000100, 0b01111110, 0b00000101,  // 102 f
    4, 0b10011000, 0b10100100, 0b10100100, 0b01111000,  // 103 g
    4, 0b01111111, 0b00000100, 0b00000100, 0b01111000,  // 104 h
    3, 0b01000100, 0b01111101, 0b01000000,  // 105 i
    4, 0b01000000, 0b10000000, 0b10000100, 0b01111101,  // 106 j
    4, 0b01111111, 0b00010000, 0b00101000, 0b01000100,  // 107 k
    3, 0b01000001, 0b01111111, 0b01000000,  // 108 l
    5, 0b01111100, 0b00000100, 0b01111100, 0b00000100, 0b01111000,  // 109 m
    4, 0b01111100, 0b00000100, 0b00000100, 0b01111000,  // 110 n
    4, 0b00111000, 0b01000100, 0b01000100, 0b00111000,  // 111 o
    4, 0b11111100, 0b00100100, 0b00100100, 0b00011000,  // 112 p
    4, 0b00011000, 0b00100100, 0b00100100, 0b11111100,  // 113 q
    4, 0b01111100, 0b00001000, 0b00000100, 0b00000100,  // 114 r
    4, 0b01001000, 0b01010100, 0b01010100, 0b00100100,  // 115 s
    3, 0b00000100, 0b00111111, 0b01000100,  // 116 t
    4, 0b00111100, 0b01000000, 0b01000000, 0b01111100,  // 117 u
    5, 0b00011100, 0b00100000, 0b01000000, 0b00100000, 0b00011100,  // 118 v
    5, 0b00111100, 0b01000000, 0b00111100, 0b01000000, 0b00111100,  // 119 w
    5, 0b01000100, 0b00101000, 0b00010000, 0b00101000, 0b01000100,  // 120 x
    4, 0b10011100, 0b10100000, 0b10100000, 0b01111100,  // 121 y
    3, 0b01100100, 0b01010100, 0b01001100,  // 122 z
    3, 0b00001000, 0b00110110, 0b01000001,  // 123 {
    1, 0b01111111,  // 124 |
    3, 0b01000001, 0b00110110, 0b00001000,  // 125 }
    4, 0b00001000, 0b00000100, 0b00001000, 0b00000100,  // 126 ~
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 127 to 175, none
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0,
    3, 0b00000010, 0b00000101, 0b00000010,  // 176 degree sign
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 177 to 195, none
    0, 0, 0,
    4, 0b01111100, 0b00010011, 0b00010011, 0b01111100,  // 196 A umlaut
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 197 to 213, none
    0,
    4, 0b00111100, 0b01000011, 0b01000011, 0b00111100,  // 214 O umlaut
    0, 0, 0, 0, 0,  // 215 to 219, none
    4, 0b00111101, 0b01000000, 0b01000000, 0b00111101,  // 220 U umlaut
    0, 0,  // 221 to 222, none
    4, 0b11111110, 0b00000001, 0b01001001, 0b00110110,  // 223 sharp s
    0, 0, 0, 0,  // 224 to 227, none
    4, 0b00100000, 0b01010110, 0b01010110, 0b01111000,  // 228 a umlaut
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // 229 to 245, none
    0,
    4, 0b00111000, 0b01000110, 0b01000110, 0b00111000,  // 246 o umlaut
    0, 0, 0, 0, 0,  // 247 to 251, none
    4, 0b00111100, 0b01000010, 0b01000010, 0b01111100)};  // 252 u umlaut
// clang-format on

/*
 * The chars are walked by their offset into the span, which is checked against
 * its size before each read.
 */
Status Font::CheckFont(span<uint8_t const> font) {
  if ((font.size() < kHeaderLength1) || (ReadByte(font, 0) != kFileIndicator)) {
    return Error::kNotAFont;
  }

  uint8_t const version{ReadByte(font, kVersionPosition)};
  bool const header_whole{
      (version == kVersion1) ||
      ((version == kVersion2) && (font.size() >= kHeaderLength2))};
  if (!header_whole) {
    return Error::kNotAFont;
  }
  FontHeader const header{ReadHeader(font)};
  bool const has_chars{header.first <= header.last};
  bool const height_fits{(header.height >= 1) &&
                         (header.height <= kRowNumberOfModule)};
  if (!has_chars || !height_fits) {
    return Error::kNotAFont;
  }
  size_t offset{header.offset};
  /* the loop ends at the last char rather than past it, as a last char of
   * 0xFFFF has no next; a count would wrap there */
  uint16_t code{header.first};
  bool walked{false};
  while (!walked) {
    if (offset >= font.size()) {
      return Error::kNotAFont;
    }
    offset += ReadByte(font, offset) + 1u;
    walked = (code == header.last);
    code++;
  }
  return (offset == font.size()) ? Error::kOk : Error::kNotAFont;
}

/*
 * Walks from the first char of the font to `c`, each char its width and that
 * many columns on. The header is read again on every call rather than kept:
 * five bytes from flash cost less than the RAM to keep them.
 */
Result<Glyph> Font::GetGlyph(span<uint8_t const> font, char c) {
  FontHeader const header{ReadHeader(font)};
  uint16_t const code{static_cast<uint8_t>(c)};

  if ((code < header.first) || (code > header.last)) {
    return Error::kUnknownChar;
  }
  size_t offset{header.offset};
  for (uint16_t skipped{header.first}; skipped < code; skipped++) {
    offset += ReadByte(font, offset) + 1u;
  }
  uint8_t const width{ReadByte(font, offset)};
  if (width == 0) {
    return Error::kUnknownChar;
  }
  return Glyph{font.subspan(offset + 1).first(width), header.height};
}

DecodedChar Font::DecodeChar(span<char const> text) {
  uint8_t const lead_byte{static_cast<uint8_t>(text[0])};
  uint8_t const next_byte{internal::IsEndOfText(text, 1)
                              ? uint8_t{0}
                              : static_cast<uint8_t>(text[1])};

  /* two byte UTF-8 sequence of a char from U+0080 to U+00FF */
  bool const is_latin_1_lead{(0xC2 == lead_byte) || (0xC3 == lead_byte)};
  bool const is_continuation{
      kUtf8ContinuationTag ==
      Bit<uint8_t>::ReadBitGroup(next_byte, kUtf8ContinuationMask)};
  if (is_latin_1_lead && is_continuation) {
    /*
     * The lead's two low bits come out on top, its others fall out of the byte.
     * One expression, the lead not masked first, measured on the Uno against
     * the operators it replaces: no example larger, SerialControl 2 bytes
     * smaller; masked first, every text drawn took 2 bytes more, and with the
     * two parts named, SerialControl 2 more.
     */
    uint8_t const latin_1{Bit<uint8_t>::Combine(
        Bit<uint8_t>::ShiftUp(lead_byte, kUtf8ContinuationWidth),
        Bit<uint8_t>::ReadBitGroup(next_byte, kUtf8ContinuationBits))};
    return {static_cast<char>(latin_1), 2};
  }
  return {static_cast<char>(lead_byte), 1};
}

}  // namespace max_matrix
