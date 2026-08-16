#pragma once

// 8x8 monochrome bitmap font, ASCII 0x00-0x7F, public domain (Daniel Hepper,
// based on Marcel Sondaar / IBM's public domain VGA fonts):
// https://github.com/dhepper/font8x8/blob/master/font8x8_basic.h
// Each glyph is 8 bytes, one per row, top row first. Within a row, bit 0
// (LSB) is the leftmost pixel.
extern const unsigned char font8x8_basic[128][8];
