#include "bmp.h"

#include <cstdlib>
#include <cstring>
#include <fstream>

namespace {

uint32_t rd32(const uint8_t *p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }
uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

void put(Image &img, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (x < 0 || y < 0 || x >= img.w || y >= img.h) {
        return;
    }
    uint8_t *p = &img.rgba[((size_t)y * img.w + x) * 4];
    p[0] = r;
    p[1] = g;
    p[2] = b;
    p[3] = a;
}

/* Bresenham with LineTo's rule: the end point is not drawn. */
void line_to(Image &img, int x0, int y0, int x1, int y1) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (x0 != x1 || y0 != y1) {
        put(img, x0, y0, 0, 0, 0, 255);
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

}  // namespace

bool load_bmp(const std::string &path, Image &out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    std::vector<uint8_t> d((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return load_bmp_mem(d, out);
}

bool load_bmp_mem(const std::vector<uint8_t> &d, Image &out) {
    if (d.size() < 54 || d[0] != 'B' || d[1] != 'M') {
        return false;
    }
    uint32_t bits_off = rd32(&d[10]);
    uint32_t hsize = rd32(&d[14]);
    int w, h, bpp, entry;
    uint32_t comp = 0;
    uint32_t used = 0;
    if (hsize == 12) {
        /* BITMAPCOREHEADER: 16-bit size fields and RGBTRIPLE palette entries. */
        w = (int16_t)rd16(&d[18]);
        h = (int16_t)rd16(&d[20]);
        bpp = rd16(&d[24]);
        entry = 3;
    } else {
        w = (int)rd32(&d[18]);
        h = (int)rd32(&d[22]);
        bpp = rd16(&d[28]);
        comp = rd32(&d[30]);
        used = hsize >= 40 ? rd32(&d[46]) : 0;
        entry = 4;
    }
    if (comp != 0 || w <= 0 || h == 0 || w > 4096 || h > 4096 || h < -4096) {
        return false;
    }
    bool top_down = h < 0;
    if (top_down) {
        h = -h;
    }
    int colors = bpp <= 8 ? (used ? (int)used : 1 << bpp) : 0;
    if (14 + hsize + (size_t)colors * entry > d.size()) {
        return false;
    }
    const uint8_t *pal = &d[14 + hsize];
    size_t stride = (((size_t)w * bpp + 31) / 32) * 4;
    if (bits_off + stride * h > d.size()) {
        return false;
    }
    out.w = w;
    out.h = h;
    out.rgba.assign((size_t)w * h * 4, 255);
    for (int y = 0; y < h; y++) {
        const uint8_t *row = &d[bits_off + stride * (top_down ? y : h - 1 - y)];
        for (int x = 0; x < w; x++) {
            uint8_t r, g, b;
            if (bpp <= 8) {
                int idx;
                if (bpp == 8) {
                    idx = row[x];
                } else if (bpp == 4) {
                    idx = (row[x / 2] >> (x % 2 ? 0 : 4)) & 0xf;
                } else {
                    idx = (row[x / 8] >> (7 - x % 8)) & 1;
                }
                if (idx >= colors) {
                    idx = 0;
                }
                b = pal[idx * entry];
                g = pal[idx * entry + 1];
                r = pal[idx * entry + 2];
            } else if (bpp == 24 || bpp == 32) {
                const uint8_t *p = row + x * (bpp / 8);
                b = p[0];
                g = p[1];
                r = p[2];
            } else {
                return false;
            }
            put(out, x, y, r, g, b, 255);
        }
    }
    return true;
}

void punch_corners(Image &img) {
    int r = img.w - 1;
    int b = img.h - 1;
    const int pts[12][2] = {
        {0, 0}, {1, 0}, {0, 1}, {r, 0}, {r - 1, 0}, {r, 1},
        {0, b}, {1, b}, {0, b - 1}, {r, b}, {r - 1, b}, {r, b - 1},
    };
    for (const auto &p : pts) {
        put(img, p[0], p[1], 0, 0, 0, 0);
    }
}

void stroke_octagon(Image &img) {
    /* MoveToEx(x, y+2) then LineTo around the card (draw_card). */
    const int pts[9][2] = {
        {0, 2}, {0, 0x5d}, {2, 0x5f}, {0x44, 0x5f}, {0x46, 0x5d}, {0x46, 2}, {0x44, 0}, {2, 0}, {0, 2},
    };
    for (int i = 0; i < 8; i++) {
        line_to(img, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]);
    }
}
