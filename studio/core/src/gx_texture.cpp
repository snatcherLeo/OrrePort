// gx_texture.cpp: GameCube texture formats to ordinary RGBA8 pictures, and the game's type-9 texture file.
//
// Why lifted, not written anew (the review's decision 4, the project's read 2026-11-17): the port draws every texture through
// Aurora's converter (extern/aurora/lib/gfx/texture_convert.cpp), so Studio decodes with the same rules (the same bit
// expansion, the same CMPR blends, I4 / I8 alpha = intensity as the GX hardware does) and a picture looks the same in
// Studio as in the game. The per-texel rules below are Aurora's, rewritten without Aurora's internal types (ByteBuffer,
// Tracy, its GX enums) so the core needs no Aurora; mipmaps are left out (Studio shows level 0).
// When the port later moves onto the shared core, it can use this file in place of its copy.
//
// Credit: Aurora, Copyright (c) 2022 Luke Street, MIT License (extern/aurora/LICENSE); the bit-expansion trick from
// http://www.mindcontrol.org/~hplus/graphics/expand-bits.html, as cited there.
//
// Differences from tools/local/colo/gx.py (the Python reader the golden tests compare with), all deliberate, from
// Aurora: 5- and 4-bit channels expand by bit replication (Python: v * 255 // 31, at most 1 apart); CMPR's two middle
// colours are (3a + 5b) / 8 blends (Python: thirds, a few apart) and its transparent entry keeps the midpoint colour
// (Python: black); I4 / I8 / IA* follow GX (alpha = intensity for I4 and I8; Python: opaque). The golden test allows
// exactly these. Platforms: plain C++, every OS.
#include "internal.hpp"

namespace orre {

namespace {
// GX texture formats (GXTexFmt numbering)
enum { TF_I4 = 0, TF_I8 = 1, TF_IA4 = 2, TF_IA8 = 3, TF_RGB565 = 4, TF_RGB5A3 = 5, TF_RGBA8 = 6, TF_C4 = 8, TF_C8 = 9,
       TF_C14X2 = 10, TF_CMPR = 14 };

template <int v>
constexpr uint8_t expand(uint8_t n) {
    if constexpr (v == 3) return (uint8_t)((n << (8 - 3)) | (n << (8 - 6)) | (n >> (9 - 8)));
    else return (uint8_t)((n << (8 - v)) | (n >> ((v * 2) - 8)));
}
constexpr uint8_t s3tc_blend(uint32_t a, uint32_t b) { return (uint8_t)((((a << 1) + a) + ((b << 2) + b)) >> 3); }
constexpr uint8_t half_blend(uint8_t a, uint8_t b) { return (uint8_t)(((uint32_t)a + b) >> 1); }

void rgb565(uint16_t t, uint8_t* o) {
    o[0] = expand<5>(t >> 11 & 0x1f); o[1] = expand<6>(t >> 5 & 0x3f); o[2] = expand<5>(t & 0x1f); o[3] = 0xff;
}
void rgb5a3(uint16_t t, uint8_t* o) {
    if (t & 0x8000) {
        o[0] = expand<5>(t >> 10 & 0x1f); o[1] = expand<5>(t >> 5 & 0x1f); o[2] = expand<5>(t & 0x1f); o[3] = 0xff;
    } else {
        o[0] = expand<4>(t >> 8 & 0xf); o[1] = expand<4>(t >> 4 & 0xf); o[2] = expand<4>(t & 0xf); o[3] = expand<3>(t >> 12 & 0x7);
    }
}
// IA8 (t read big-endian): the first byte is alpha, the second intensity (colo/gx.py cia8; Aurora's decoder reads the
// same layout without a byte swap). *Corrected 2026-11-18:* this had the two swapped, which turned Espeon's gem sparkle
// (an IA8 picture) into a black square with the star drawn in its colour.
void ia8(uint16_t t, uint8_t* o) { o[0] = o[1] = o[2] = (uint8_t)(t & 0xff); o[3] = (uint8_t)(t >> 8); }

struct Fmt { int bw, bh, bpp; };
Fmt fmt_info(int f) {
    switch (f) {
        case TF_I4: case TF_C4: case TF_CMPR: return {8, 8, 4};
        case TF_I8: case TF_IA4: case TF_C8: return {8, 4, 8};
        case TF_IA8: case TF_RGB565: case TF_RGB5A3: case TF_C14X2: return {4, 4, 16};
        case TF_RGBA8: return {4, 4, 32};
        default: return {0, 0, 0};
    }
}
}  // namespace

// tables.py GX_TO_GS, the other way: the game's GS format codes (GStextureGetGXformat)
int gs_to_gx_format(int gs) {
    switch (gs) {
        case 0x00: return TF_C4; case 0x01: return TF_C8; case 0x30: return TF_C14X2; case 0x40: return TF_I4;
        case 0x41: return TF_IA4; case 0x42: return TF_I8; case 0x43: return TF_IA8; case 0x44: return TF_RGB565;
        case 0x45: return TF_RGBA8; case 0x90: return TF_RGB5A3; case 0xB0: return TF_CMPR; case 0xA0: return TF_I8;
        default: return -1;
    }
}

std::vector<uint32_t> decode_tlut(const uint8_t* data, size_t size, int tlut_format, int count) {
    std::vector<uint32_t> pal;
    for (int i = 0; i < count && 2 * (size_t)i + 2 <= size; ++i) {
        const uint16_t t = be16(data + 2 * i);
        uint8_t c[4];
        if (tlut_format == 0) ia8(t, c);
        else if (tlut_format == 1) rgb565(t, c);
        else rgb5a3(t, c);
        uint32_t w;
        std::memcpy(&w, c, 4);
        pal.push_back(w);
    }
    return pal;
}

bool decode_gx(const uint8_t* data, size_t size, int f, int w, int h, const std::vector<uint32_t>* palette,
               std::vector<uint8_t>& out) {
    const Fmt fi = fmt_info(f);
    if (!fi.bw || w <= 0 || h <= 0) return false;
    const int bx_n = (w + fi.bw - 1) / fi.bw, by_n = (h + fi.bh - 1) / fi.bh;
    const size_t need = (size_t)bx_n * by_n * fi.bw * fi.bh * fi.bpp / 8;
    if (need > size) return false;
    out.assign((size_t)w * h * 4, 0);
    auto put = [&](int x, int y, const uint8_t* c) {
        if (x < w && y < h) std::memcpy(&out[((size_t)y * w + x) * 4], c, 4);
    };
    auto pal = [&](uint32_t i, uint8_t* c) {
        // Aurora: an index past the palette is transparent
        if (palette && i < palette->size()) std::memcpy(c, &(*palette)[i], 4);
        else std::memset(c, 0, 4);
    };
    const uint8_t* p = data;
    for (int by = 0; by < by_n; ++by)
        for (int bx = 0; bx < bx_n; ++bx) {
            const int x0 = bx * fi.bw, y0 = by * fi.bh;
            uint8_t c[4];
            switch (f) {
                case TF_RGBA8:  // two 32-byte halves: AR pairs, then GB pairs
                    for (int i = 0; i < 16; ++i) {
                        c[3] = p[i * 2]; c[0] = p[i * 2 + 1]; c[1] = p[32 + i * 2]; c[2] = p[33 + i * 2];
                        put(x0 + i % 4, y0 + i / 4, c);
                    }
                    p += 64;
                    break;
                case TF_RGB5A3: case TF_RGB565: case TF_IA8: case TF_C14X2:
                    for (int i = 0; i < 16; ++i, p += 2) {
                        const uint16_t t = be16(p);
                        if (f == TF_RGB5A3) rgb5a3(t, c);
                        else if (f == TF_RGB565) rgb565(t, c);
                        else if (f == TF_IA8) ia8(t, c);
                        else pal(t & 0x3fff, c);
                        put(x0 + i % 4, y0 + i / 4, c);
                    }
                    break;
                case TF_I8: case TF_IA4: case TF_C8:
                    for (int i = 0; i < 32; ++i, ++p) {
                        const uint8_t v = *p;
                        if (f == TF_I8) { c[0] = c[1] = c[2] = c[3] = v; }
                        else if (f == TF_IA4) { c[0] = c[1] = c[2] = expand<4>(v & 0xf); c[3] = expand<4>(v >> 4); }
                        else pal(v, c);
                        put(x0 + i % 8, y0 + i / 8, c);
                    }
                    break;
                case TF_I4: case TF_C4:
                    for (int i = 0; i < 32; ++i, ++p)
                        for (int half = 0; half < 2; ++half) {
                            const uint8_t nib = half ? (*p & 0xf) : (*p >> 4);
                            if (f == TF_I4) { c[0] = c[1] = c[2] = c[3] = expand<4>(nib); }
                            else pal(nib, c);
                            const int idx = i * 2 + half;
                            put(x0 + idx % 8, y0 + idx / 8, c);
                        }
                    break;
                case TF_CMPR:  // four 4x4 DXT1-like sub-blocks per 8x8 tile, colours big-endian
                    for (int sub = 0; sub < 4; ++sub, p += 8) {
                        const int sx = x0 + (sub % 2) * 4, sy = y0 + (sub / 2) * 4;
                        const uint16_t c1 = be16(p), c2 = be16(p + 2);
                        uint8_t t[16];
                        rgb565(c1, t);
                        rgb565(c2, t + 4);
                        if (c1 > c2) {
                            for (int k = 0; k < 3; ++k) { t[8 + k] = s3tc_blend(t[4 + k], t[k]); t[12 + k] = s3tc_blend(t[k], t[4 + k]); }
                            t[11] = t[15] = 0xff;
                        } else {
                            for (int k = 0; k < 3; ++k) t[8 + k] = t[12 + k] = half_blend(t[k], t[4 + k]);
                            t[11] = 0xff;
                            t[15] = 0;  // GX: a transparent midpoint
                        }
                        for (int y = 0; y < 4; ++y) {
                            uint8_t bits = p[4 + y];
                            for (int x = 0; x < 4; ++x, bits <<= 2) put(sx + x, sy + y, &t[((bits >> 6) & 3) * 4]);
                        }
                    }
                    break;
            }
        }
    return true;
}

// The type-9 texture file (GStextureHandle; gx.py decode_texture_file): u16 w, u16 h at 0; s32 GS format at 8; u32
// TLUT format at 0xC; pixel data offset at 0x28 (0 = 0x80); palette offset at 0x48 (a guess in gx.py: no paletted
// type-9 file on the US disc, so untested).
bool decode_texture_file(const Bytes& d, int& w, int& h, std::vector<uint8_t>& rgba) {
    if (d.size() < 0x80) return false;
    w = be16(d, 0);
    h = be16(d, 2);
    const int gx = gs_to_gx_format((int)be32(d, 8));
    if (gx < 0 || !w || !h) return false;
    uint32_t off = be32(d, 0x28);
    if (!off) off = 0x80;
    if (off >= d.size()) return false;
    std::vector<uint32_t> palette;
    if (gx == TF_C4 || gx == TF_C8 || gx == TF_C14X2) {
        const uint32_t po = be32(d, 0x48);
        if (po && po < d.size()) palette = decode_tlut(d.data() + po, d.size() - po, (int)be32(d, 0x0C), std::min<int>(16384, (int)((d.size() - po) / 2)));
    }
    return decode_gx(d.data() + off, d.size() - off, gx, w, h, palette.empty() ? nullptr : &palette, rgba);
}

}  // namespace orre
