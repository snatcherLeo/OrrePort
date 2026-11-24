// png_write.cpp: a minimal PNG writer for the CLI's picture dumps and the golden tests.
// Why no library: a dump only needs a valid file, not a small one, so the pixels go into stored (uncompressed) deflate
// blocks with the two checksums PNG needs; no zlib or libpng dependency in the core. Studio's interface never writes
// PNGs this way (it draws the decoded pixels directly). Platforms: plain C++ and stdio, every OS.
#include <cstdio>

#include "internal.hpp"

namespace {
uint32_t crc_table[256];
bool crc_ready = false;
uint32_t crc(const uint8_t* p, size_t n, uint32_t c = 0xFFFFFFFFu) {
    if (!crc_ready) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t v = i;
            for (int k = 0; k < 8; ++k) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1;
            crc_table[i] = v;
        }
        crc_ready = true;
    }
    for (size_t i = 0; i < n; ++i) c = crc_table[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return c;
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(v >> 24); b.push_back(v >> 16 & 0xFF); b.push_back(v >> 8 & 0xFF); b.push_back(v & 0xFF);
}
void chunk(std::FILE* f, const char* type, const std::vector<uint8_t>& data) {
    std::vector<uint8_t> b;
    put32(b, (uint32_t)data.size());
    b.insert(b.end(), type, type + 4);
    b.insert(b.end(), data.begin(), data.end());
    put32(b, crc(b.data() + 4, b.size() - 4) ^ 0xFFFFFFFFu);
    std::fwrite(b.data(), 1, b.size(), f);
}
}  // namespace

extern "C" int orre_png_write(const char* path, const OrreImage* img) {
    if (!path || !img || !img->rgba || img->width <= 0 || img->height <= 0) return 0;
    std::FILE* f = std::fopen(path, "wb");
    if (!f) return 0;
    static const uint8_t sig[8] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10};
    std::fwrite(sig, 1, 8, f);
    std::vector<uint8_t> ihdr;
    put32(ihdr, img->width);
    put32(ihdr, img->height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});  // 8-bit RGBA
    chunk(f, "IHDR", ihdr);
    // raw scanlines (filter 0), then zlib: header, stored blocks of up to 65535 bytes, Adler-32
    std::vector<uint8_t> raw;
    const size_t row = (size_t)img->width * 4;
    for (int y = 0; y < img->height; ++y) {
        raw.push_back(0);
        raw.insert(raw.end(), img->rgba + y * row, img->rgba + (y + 1) * row);
    }
    std::vector<uint8_t> z = {0x78, 0x01};
    for (size_t o = 0; o < raw.size() || o == 0; o += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - o);
        z.push_back(o + n >= raw.size() ? 1 : 0);
        z.push_back(n & 0xFF); z.push_back(n >> 8);
        z.push_back(~n & 0xFF); z.push_back((~n >> 8) & 0xFF);
        z.insert(z.end(), raw.begin() + o, raw.begin() + o + n);
        if (o + n >= raw.size()) break;
    }
    uint32_t s1 = 1, s2 = 0;
    for (uint8_t c : raw) { s1 = (s1 + c) % 65521; s2 = (s2 + s1) % 65521; }
    put32(z, s2 << 16 | s1);
    chunk(f, "IDAT", z);
    chunk(f, "IEND", {});
    const bool ok = std::fclose(f) == 0;
    return ok ? 1 : 0;
}
