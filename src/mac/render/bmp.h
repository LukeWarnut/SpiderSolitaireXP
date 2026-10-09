#pragma once

#include <cstdint>
#include <string>
#include <vector>

/* RGBA8, top row first. */
struct Image {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> rgba;
};

/* Uncompressed 1/4/8/24/32-bit Windows BMP. */
bool load_bmp(const std::string &path, Image &out);
/* Same, from an in-memory file (the WASM port fetches bitmaps over HTTP). */
bool load_bmp_mem(const std::vector<uint8_t> &data, Image &out);

/* Clear the six corner pixels per end that draw_card painted felt green, so
 * the card below (or the felt) shows through. */
void punch_corners(Image &img);

/* The black outline draw_card adds to codes 14-23 and 27-36. */
void stroke_octagon(Image &img);
