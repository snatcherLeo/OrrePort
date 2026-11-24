/*
 * pc_svg.cpp: draws OrrePort's own flat SVG art (the brand files, the disc labels) into an RGBA picture, for RmlUi.
 * No library: the brand files are plain filled paths on purpose (docs/design/brand: "no fonts, masks or strokes"), so
 * this reads only that subset: <path> with fill="#RRGGBB", optional fill-opacity and fill-rule (nonzero by default,
 * or evenodd), and the absolute path commands M, L, H, V, Q (curves split into short lines) and Z; the canvas size
 * comes from the viewBox. Anything else (metadata, other elements, relative commands) is skipped. Checked on the
 * Colosseum disc label, 2026-11-09: 132 paths, M / L / H / V / Q / Z only.
 *
 * Drawn by scanlines with 4 x 4 samples per pixel (each shape's crossings per sample row, filled by its rule), then
 * painted in file order with its opacity. Platform layer only (the RmlUi trial's hub).
 */
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

#include "pc_parallel.h" /* pc_svg_render's bands */

namespace {

struct Edge {
    float x0, y0, x1, y1;
};

struct Shape {
    float r = 0, g = 0, b = 0, opacity = 1;
    bool evenodd = false;
    std::vector<Edge> edges;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
};

std::string attr(const std::string& tag, const char* name) {
    const std::string key = std::string(" ") + name + "=\"";
    const size_t a = tag.find(key);
    if (a == std::string::npos) {
        return "";
    }
    const size_t b = tag.find('"', a + key.size());
    return b == std::string::npos ? "" : tag.substr(a + key.size(), b - a - key.size());
}

void add_edge(Shape& s, float x0, float y0, float x1, float y1) {
    if (y0 != y1) {
        s.edges.push_back({x0, y0, x1, y1});
    }
    s.minx = std::min({s.minx, x0, x1});
    s.maxx = std::max({s.maxx, x0, x1});
    s.miny = std::min({s.miny, y0, y1});
    s.maxy = std::max({s.maxy, y0, y1});
}

void parse_path(const std::string& d, Shape& s) {
    const char* p = d.c_str();
    float cx = 0, cy = 0, sx = 0, sy = 0; /* the pen; the sub-path's start */
    char cmd = 0;
    auto number = [&p](float& out) {
        char* q;
        out = std::strtof(p, &q);
        const bool ok = q != p;
        p = q;
        return ok;
    };
    while (*p != '\0') {
        while (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t') ++p;
        if (*p == '\0') {
            break;
        }
        if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
            cmd = *p++;
        }
        float x, y, qx, qy;
        switch (cmd) {
        case 'M':
            if (!number(x) || !number(y)) return;
            if (cx != sx || cy != sy) add_edge(s, cx, cy, sx, sy); /* an unclosed sub-path closes for filling */
            cx = sx = x;
            cy = sy = y;
            cmd = 'L'; /* more pairs after M are lines */
            break;
        case 'L':
            if (!number(x) || !number(y)) return;
            add_edge(s, cx, cy, x, y);
            cx = x;
            cy = y;
            break;
        case 'H':
            if (!number(x)) return;
            add_edge(s, cx, cy, x, cy);
            cx = x;
            break;
        case 'V':
            if (!number(y)) return;
            add_edge(s, cx, cy, cx, y);
            cy = y;
            break;
        case 'Q': {
            if (!number(qx) || !number(qy) || !number(x) || !number(y)) return;
            constexpr int N = 12;
            float px = cx, py = cy;
            for (int i = 1; i <= N; ++i) {
                const float t = i / (float)N, u = 1 - t;
                const float nx = u * u * cx + 2 * u * t * qx + t * t * x;
                const float ny = u * u * cy + 2 * u * t * qy + t * t * y;
                add_edge(s, px, py, nx, ny);
                px = nx;
                py = ny;
            }
            cx = x;
            cy = y;
            break;
        }
        case 'Z':
        case 'z':
            add_edge(s, cx, cy, sx, sy);
            cx = sx;
            cy = sy;
            break;
        default:
            return; /* a command outside the subset: the rest of this path is skipped */
        }
    }
    if (cx != sx || cy != sy) add_edge(s, cx, cy, sx, sy);
}

/* paints every shape, in file order, into rows y0 .. y1 of acc (premultiplied RGBA, width x height): scanlines with
 * 4 x 4 samples per pixel, each shape's crossings per sample row filled by its rule, then laid over what is there with
 * its opacity. Touches only those rows, so bands of rows can be painted at the same time (pc_svg_render). */
void paint_rows(const std::vector<Shape>& shapes, const float vb[4], float scale, int width, int height, int y0, int y1,
                std::vector<float>& acc) {
    constexpr int SS = 4;
    std::vector<float> cov;
    std::vector<std::pair<float, int>> xs;
    for (const Shape& s : shapes) {
        const int px0 = std::max(0, (int)std::floor((s.minx - vb[0]) * scale));
        const int px1 = std::min(width - 1, (int)std::ceil((s.maxx - vb[0]) * scale));
        const int py0 = std::max(y0, (int)std::floor((s.miny - vb[1]) * scale));
        const int py1 = std::min({y1, height - 1, (int)std::ceil((s.maxy - vb[1]) * scale)});
        if (px0 > px1 || py0 > py1) {
            continue;
        }
        const int w = px1 - px0 + 1;
        cov.assign((size_t)w * (py1 - py0 + 1), 0.0f);
        for (int sy = py0 * SS; sy < (py1 + 1) * SS; ++sy) {
            const float y = (sy + 0.5f) / SS / scale + vb[1]; /* the sample row in canvas units */
            xs.clear();
            for (const Edge& e : s.edges) {
                if ((e.y0 <= y) != (e.y1 <= y)) {
                    const float x = e.x0 + (y - e.y0) * (e.x1 - e.x0) / (e.y1 - e.y0);
                    xs.emplace_back((x - vb[0]) * scale * SS, e.y1 > e.y0 ? 1 : -1); /* in sample columns */
                }
            }
            std::sort(xs.begin(), xs.end());
            int winding = 0;
            float* row = &cov[(size_t)(sy / SS - py0) * w];
            for (size_t i = 0; i + 1 < xs.size(); ++i) {
                winding += xs[i].second;
                const bool filled = s.evenodd ? (winding & 1) != 0 : winding != 0;
                if (!filled) {
                    continue;
                }
                /* sample columns whose centres (k + 0.5) lie in [xs[i], xs[i+1]) */
                int k0 = (int)std::ceil(xs[i].first - 0.5f), k1 = (int)std::ceil(xs[i + 1].first - 0.5f);
                k0 = std::max(k0, px0 * SS);
                k1 = std::min(k1, (px1 + 1) * SS);
                for (int k = k0; k < k1; ++k) {
                    row[k / SS - px0] += 1.0f / (SS * SS);
                }
            }
        }
        for (int py = py0; py <= py1; ++py) {
            for (int px = px0; px <= px1; ++px) {
                const float a = std::min(1.0f, cov[(size_t)(py - py0) * w + (px - px0)]) * s.opacity;
                if (a <= 0) {
                    continue;
                }
                float* o = &acc[((size_t)py * width + px) * 4];
                o[0] = s.r * a + o[0] * (1 - a);
                o[1] = s.g * a + o[1] * (1 - a);
                o[2] = s.b * a + o[2] * (1 - a);
                o[3] = a + o[3] * (1 - a);
            }
        }
    }
}

} // namespace

/* svg: the file's text; width: the picture's width in pixels; height: set from the viewBox's shape; rgba:
 * width*height*4 bytes, straight (not premultiplied) alpha; skip_fills: shapes in these colours ("#RRGGBB", as written
 * in the file) are left out (the hub's blank disc: the label without its printing). False when the text has no
 * viewBox or no shapes. */
bool pc_svg_render(const std::string& svg, int width, int& height, std::vector<unsigned char>& rgba,
                   const std::vector<std::string>& skip_fills) {
    const size_t open = svg.find("<svg");
    if (open == std::string::npos || width <= 0) {
        return false;
    }
    const std::string root = svg.substr(open, svg.find('>', open) - open + 1);
    float vb[4] = {0, 0, 0, 0};
    {
        const std::string v = attr(root, "viewBox");
        const char* p = v.c_str();
        for (float& f : vb) {
            char* q;
            f = std::strtof(p, &q);
            p = q;
        }
    }
    if (vb[2] <= 0 || vb[3] <= 0) {
        return false;
    }
    std::vector<Shape> shapes;
    size_t pos = 0;
    while ((pos = svg.find('<', pos)) != std::string::npos) {
        if (svg.compare(pos, 10, "<metadata>") == 0) { /* skip the embedded manifest whole */
            const size_t end = svg.find("</metadata>", pos);
            pos = end == std::string::npos ? svg.size() : end + 11;
            continue;
        }
        const size_t end = svg.find('>', pos);
        if (end == std::string::npos) {
            break;
        }
        const std::string tag = svg.substr(pos, end - pos + 1);
        pos = end;
        if (tag.rfind("<path", 0) != 0) {
            continue;
        }
        const std::string fill = attr(tag, "fill");
        if (fill.size() != 7 || fill[0] != '#' ||
            std::find(skip_fills.begin(), skip_fills.end(), fill) != skip_fills.end()) {
            continue;
        }
        Shape s;
        const long c = std::strtol(fill.c_str() + 1, nullptr, 16);
        s.r = ((c >> 16) & 255) / 255.0f;
        s.g = ((c >> 8) & 255) / 255.0f;
        s.b = (c & 255) / 255.0f;
        const std::string op = attr(tag, "fill-opacity");
        s.opacity = op.empty() ? 1.0f : std::clamp(std::strtof(op.c_str(), nullptr), 0.0f, 1.0f);
        s.evenodd = attr(tag, "fill-rule") == "evenodd";
        parse_path(attr(tag, "d"), s);
        if (!s.edges.empty()) {
            shapes.push_back(std::move(s));
        }
    }
    if (shapes.empty()) {
        return false;
    }
    const float scale = width / vb[2]; /* canvas units to pixels */
    height = std::max(1, (int)std::lround(vb[3] * scale));
    std::vector<float> acc((size_t)width * height * 4, 0.0f); /* premultiplied RGBA */
    /* the picture in bands of rows side by side (pc_parallel.h; 2026-11-13: the hub's 1216 px disc was the slowest
     * picture of the start). Every band paints all shapes in file order, but only into its own rows, so the result is
     * the same pixel for pixel as one pass; small pictures stay one band. */
    const int bands = height >= 256 ? 8 : 1;
    std::vector<std::function<void()>> jobs;
    for (int band = 0; band < bands; ++band) {
        const int y0 = height * band / bands, y1 = height * (band + 1) / bands - 1;
        jobs.push_back([&, y0, y1] { paint_rows(shapes, vb, scale, width, height, y0, y1, acc); });
    }
    pc_run_together(jobs);
    rgba.resize((size_t)width * height * 4);
    for (size_t i = 0; i < (size_t)width * height; ++i) {
        const float a = acc[i * 4 + 3];
        for (int c = 0; c < 3; ++c) {
            rgba[i * 4 + c] = (unsigned char)std::lround(a > 0 ? std::min(1.0f, acc[i * 4 + c] / a) * 255 : 0);
        }
        rgba[i * 4 + 3] = (unsigned char)std::lround(a * 255);
    }
    return true;
}
