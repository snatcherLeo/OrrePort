#include "gx.hpp"
#include "__gx.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>

// OrrePort (observe only; 2026-11-19, Studio's Fire Punch depth question): ORRE_GX_ZLOG=<first>-<last> game frames
// prints one "[gxz]" line per depth-affecting GX call (GXSetZMode, colour / alpha update, GXSetZTexture, projection,
// viewport, EFB copies with clear) and per draw (GXBegin, GXCallDisplayList), each with the caller's return addresses,
// so a GPU capture's draws can be tied to the game function that issued them (addr2line on the exe). Why: a RenderDoc
// capture showed a full-screen depth-only quad before the move's particles, but not which game code drew it; one log
// run names it without another RenderDoc run (RenderDoc's target control opens a network port). Nothing drawn changes:
// it only reads the arguments. Off unless the variable is set. The port sets the current game frame each retrace
// (orre_gx_zlog_frame, port/src/pc_vi.c). The return addresses come from RtlCaptureStackBackTrace (Windows only; it
// walks the x64 unwind tables MinGW emits); Linux / macOS would use backtrace() from <execinfo.h> (likely the same
// output), for now they print the line without addresses.
#ifdef _WIN32
extern "C" __declspec(dllimport) unsigned short __stdcall RtlCaptureStackBackTrace(unsigned long framesToSkip,
                                                                                  unsigned long framesToCapture,
                                                                                  void** backTrace,
                                                                                  unsigned long* backTraceHash);
extern "C" __declspec(dllimport) void* __stdcall GetModuleHandleA(const char* name);
#endif
extern "C" {
unsigned orre_gx_zlog_frame = 0;
}
extern "C" int orre_gx_zlog_on(void) {
  static int s_state = -1; // -1 not read yet, 0 off, 1 on
  static unsigned s_first, s_last;
  if (s_state < 0) {
    const char* env = getenv("ORRE_GX_ZLOG");
    s_state = 0;
    if (env != nullptr && *env != '\0') {
      char* end = nullptr;
      s_first = static_cast<unsigned>(strtoul(env, &end, 10));
      s_last = (end != nullptr && *end == '-') ? static_cast<unsigned>(strtoul(end + 1, nullptr, 10)) : s_first;
      s_state = 1;
#ifdef _WIN32
      fprintf(stderr, "[gxz] on: game frames %u-%u; exe base %p (addr2line: address - base + the exe's ImageBase)\n",
              s_first, s_last, GetModuleHandleA(nullptr));
#else
      fprintf(stderr, "[gxz] on: game frames %u-%u\n", s_first, s_last);
#endif
    }
  }
  return s_state == 1 && orre_gx_zlog_frame >= s_first && orre_gx_zlog_frame <= s_last;
}
extern "C" void orre_gx_zlog(const char* fmt, ...) {
  static unsigned s_seq;
  char text[256];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(text, sizeof(text), fmt, ap);
  va_end(ap);
  char stack[200] = "";
#ifdef _WIN32
  void* frames[8];
  // skip this function and the GX entry point that called it: frames[0] is the game code's call site
  const unsigned short n = RtlCaptureStackBackTrace(2, 8, frames, nullptr);
  size_t used = 0;
  for (unsigned short i = 0; i < n && used + 20 < sizeof(stack); ++i) {
    used += static_cast<size_t>(snprintf(stack + used, sizeof(stack) - used, "%s%p", i ? "," : "", frames[i]));
  }
#endif
  fprintf(stderr, "[gxz] f=%u n=%u %s st=%s\n", orre_gx_zlog_frame, s_seq++, text, stack);
}

extern "C" {
void GXSetFog(GXFogType type, float startZ, float endZ, float nearZ, float farZ, GXColor color) {
  const bool orthographic = (type & 0x08) != 0;
  f32 a;
  f32 c;
  u32 bMantissaRaw = 0;
  u32 bScale = 0;

  if (orthographic) {
    if (farZ == nearZ || endZ == startZ) {
      a = 0.0f;
      c = 0.0f;
    } else {
      const f32 scale = 1.0f / (endZ - startZ);
      a = scale * (farZ - nearZ);
      c = scale * (startZ - nearZ);
    }
  } else {
    f32 aCoeff;
    f32 bCoeff;
    f32 cCoeff;
    if (farZ == nearZ || endZ == startZ) {
      aCoeff = 0.0f;
      bCoeff = 0.5f;
      cCoeff = 0.0f;
    } else {
      aCoeff = (farZ * nearZ) / ((farZ - nearZ) * (endZ - startZ));
      bCoeff = farZ / (farZ - nearZ);
      cCoeff = startZ / (endZ - startZ);
    }

    // Normalize B mantissa
    f32 bMantissa = bCoeff;
    u32 bExponent = 0;
    while (bMantissa > 1.0f) {
      bMantissa *= 0.5f;
      bExponent++;
    }
    while (bMantissa > 0.0f && bMantissa < 0.5f) {
      bMantissa *= 2.0f;
      bExponent--;
    }

    a = aCoeff / static_cast<f32>(1 << (bExponent + 1));
    bMantissaRaw = static_cast<u32>(8.388638e6f * bMantissa);
    bScale = bExponent + 1;
    c = cCoeff;
  }

  u32 a_hex, c_hex;
  std::memcpy(&a_hex, &a, sizeof(a_hex));
  std::memcpy(&c_hex, &c, sizeof(c_hex));

  // BP FOG0 (0xEE) - A parameter
  u32 fog0 = 0;
  SET_REG_FIELD(0, fog0, 11, 0, (a_hex >> 12) & 0x7FF);
  SET_REG_FIELD(0, fog0, 8, 11, (a_hex >> 23) & 0xFF);
  SET_REG_FIELD(0, fog0, 1, 19, (a_hex >> 31));
  SET_REG_FIELD(0, fog0, 8, 24, 0xEE);

  // BP FOG1 (0xEF) - B mantissa
  u32 fog1 = 0;
  SET_REG_FIELD(0, fog1, 24, 0, bMantissaRaw);
  SET_REG_FIELD(0, fog1, 8, 24, 0xEF);

  // BP FOG2 (0xF0) - B scale
  u32 fog2 = 0;
  SET_REG_FIELD(0, fog2, 5, 0, bScale);
  SET_REG_FIELD(0, fog2, 8, 24, 0xF0);

  // BP FOG3 (0xF1) - C parameter + type
  u32 fog3 = 0;
  SET_REG_FIELD(0, fog3, 11, 0, (c_hex >> 12) & 0x7FF);
  SET_REG_FIELD(0, fog3, 8, 11, (c_hex >> 23) & 0xFF);
  SET_REG_FIELD(0, fog3, 1, 19, (c_hex >> 31));
  SET_REG_FIELD(0, fog3, 1, 20, orthographic);
  SET_REG_FIELD(0, fog3, 3, 21, type & 0x07);
  SET_REG_FIELD(0, fog3, 8, 24, 0xF1);

  // BP FOGCLR (0xF2) - color
  u32 fogclr = 0;
  SET_REG_FIELD(0, fogclr, 8, 0, color.b);
  SET_REG_FIELD(0, fogclr, 8, 8, color.g);
  SET_REG_FIELD(0, fogclr, 8, 16, color.r);
  SET_REG_FIELD(0, fogclr, 8, 24, 0xF2);

  GX_WRITE_RAS_REG(fog0);
  GX_WRITE_RAS_REG(fog1);
  GX_WRITE_RAS_REG(fog2);
  GX_WRITE_RAS_REG(fog3);
  GX_WRITE_RAS_REG(fogclr);
  __gx->bpSent = 1;
}

void GXSetFogColor(GXColor color) {
  // BP FOGCLR (0xF2)
  u32 fogclr = 0;
  SET_REG_FIELD(0, fogclr, 8, 0, color.b);
  SET_REG_FIELD(0, fogclr, 8, 8, color.g);
  SET_REG_FIELD(0, fogclr, 8, 16, color.r);
  SET_REG_FIELD(0, fogclr, 8, 24, 0xF2);
  GX_WRITE_RAS_REG(fogclr);
  __gx->bpSent = 1;
}

void GXSetFogRangeAdj(GXBool enable, u16 center, GXFogAdjTable* table) {
  u32 i;
  u32 range_adj;
  u32 range_c;

  if (enable) {
    for (i = 0; i < 10; i += 2) {
      range_adj = 0;
      SET_REG_FIELD(0x10D, range_adj, 12, 0, table->r[i]);
      SET_REG_FIELD(0x10E, range_adj, 12, 12, table->r[i + 1]);
      SET_REG_FIELD(0x10F, range_adj, 8, 24, (i >> 1) + 0xE9);
      GX_WRITE_RAS_REG(range_adj);
    }
  }
  range_c = 0;
  SET_REG_FIELD(0x115, range_c, 10, 0, center + 342);
  SET_REG_FIELD(0x116, range_c, 1, 10, enable);
  SET_REG_FIELD(0x117, range_c, 8, 24, 0xE8);
  GX_WRITE_RAS_REG(range_c);
  __gx->bpSent = 1;
}

void GXSetBlendMode(GXBlendMode mode, GXBlendFactor src, GXBlendFactor dst, GXLogicOp op) {
  SET_REG_FIELD(0, __gx->cmode0, 1, 0, (mode == GX_BM_BLEND || mode == GX_BM_SUBTRACT));
  SET_REG_FIELD(0, __gx->cmode0, 1, 11, (mode == GX_BM_SUBTRACT));
  SET_REG_FIELD(0, __gx->cmode0, 1, 1, (mode == GX_BM_LOGIC));
  SET_REG_FIELD(0, __gx->cmode0, 4, 12, op);
  SET_REG_FIELD(0, __gx->cmode0, 3, 8, src);
  SET_REG_FIELD(0, __gx->cmode0, 3, 5, dst);
  GX_WRITE_RAS_REG(__gx->cmode0);
  __gx->bpSent = 1;
}

void GXSetColorUpdate(GXBool enabled) {
  if (orre_gx_zlog_on()) { // OrrePort observe-only log (ORRE_GX_ZLOG, above)
    orre_gx_zlog("colorupdate %d", enabled ? 1 : 0);
  }
  SET_REG_FIELD(0, __gx->cmode0, 1, 3, enabled);
  GX_WRITE_RAS_REG(__gx->cmode0);
  __gx->bpSent = 1;
}

void GXSetAlphaUpdate(bool enabled) {
  if (orre_gx_zlog_on()) { // OrrePort observe-only log (ORRE_GX_ZLOG, above)
    orre_gx_zlog("alphaupdate %d", enabled ? 1 : 0);
  }
  SET_REG_FIELD(0, __gx->cmode0, 1, 4, enabled);
  GX_WRITE_RAS_REG(__gx->cmode0);
  __gx->bpSent = 1;
}

void GXSetZMode(bool compare_enable, GXCompare func, bool update_enable) {
  if (orre_gx_zlog_on()) { // OrrePort observe-only log (ORRE_GX_ZLOG, above); func is the GXCompare number
    orre_gx_zlog("zmode compare=%d func=%d update=%d", compare_enable ? 1 : 0, static_cast<int>(func),
                 update_enable ? 1 : 0);
  }
  SET_REG_FIELD(0, __gx->zmode, 1, 0, compare_enable);
  SET_REG_FIELD(0, __gx->zmode, 3, 1, func);
  SET_REG_FIELD(0, __gx->zmode, 1, 4, update_enable);
  GX_WRITE_RAS_REG(__gx->zmode);
  __gx->bpSent = 1;
}

void GXSetZCompLoc(GXBool before_tex) {
  SET_REG_FIELD(0, __gx->peCtrl, 1, 6, before_tex);
  GX_WRITE_RAS_REG(__gx->peCtrl);
  __gx->bpSent = 1;
}

void GXSetPixelFmt(GXPixelFmt pix_fmt, GXZFmt16 z_fmt) {
  u32 oldPeCtrl = __gx->peCtrl;
  u8 hwPixelFmt = 0;

  switch (pix_fmt) {
  case GX_PF_RGB8_Z24:
    hwPixelFmt = 0;
    break;
  case GX_PF_RGBA6_Z24:
    hwPixelFmt = 1;
    break;
  case GX_PF_RGB565_Z16:
    hwPixelFmt = 2;
    break;
  case GX_PF_Z24:
    hwPixelFmt = 3;
    break;
  case GX_PF_Y8:
  case GX_PF_U8:
  case GX_PF_V8:
    hwPixelFmt = 4;
    break;
  case GX_PF_YUV420:
    hwPixelFmt = 5;
    break;
  default:
    UNLIKELY FATAL("GXSetPixelFmt: unsupported pixel format {}", static_cast<u32>(pix_fmt));
  }

  SET_REG_FIELD(0, __gx->peCtrl, 3, 0, hwPixelFmt);
  SET_REG_FIELD(0, __gx->peCtrl, 3, 3, z_fmt);
  if (oldPeCtrl != __gx->peCtrl) {
    GX_WRITE_RAS_REG(__gx->peCtrl);
    SET_REG_FIELD(0, __gx->genMode, 1, 9, pix_fmt == GX_PF_RGB565_Z16);
    __gx->dirtyState |= 4;
  }

  if (hwPixelFmt == 4) {
    SET_REG_FIELD(0, __gx->cmode1, 2, 9, (static_cast<u32>(pix_fmt) - GX_PF_Y8) & 0x3);
    GX_WRITE_RAS_REG(__gx->cmode1);
  }

  __gx->bpSent = 1;
}

void GXSetDither(GXBool dither) {
  SET_REG_FIELD(0, __gx->cmode0, 1, 2, dither);
  GX_WRITE_RAS_REG(__gx->cmode0);
  __gx->bpSent = 1;
}

void GXSetDstAlpha(bool enabled, u8 value) {
  SET_REG_FIELD(0, __gx->cmode1, 8, 0, value);
  SET_REG_FIELD(0, __gx->cmode1, 1, 8, enabled);
  GX_WRITE_RAS_REG(__gx->cmode1);
  __gx->bpSent = 1;
}

void GXSetFieldMask(GXBool odd_mask, GXBool even_mask) {
  u32 reg;

  reg = 0;
  SET_REG_FIELD(0x1FB, reg, 1, 0, even_mask);
  SET_REG_FIELD(0x1FC, reg, 1, 1, odd_mask);
  SET_REG_FIELD(0x1FD, reg, 8, 24, 0x44);
  GX_WRITE_RAS_REG(reg);
  __gx->bpSent = 1;
}

void GXSetFieldMode(GXBool field_mode, GXBool half_aspect_ratio) {
  u32 reg = 0;

  SET_REG_FIELD(0x21A, __gx->lpSize, 1, 22, half_aspect_ratio);
  GX_WRITE_RAS_REG(__gx->lpSize);
  __GXFlushTextureState();

  SET_REG_FIELD(0, reg, 8, 24, 0x68);
  SET_REG_FIELD(0, reg, 1, 0, field_mode);
  GX_WRITE_RAS_REG(reg);
  __GXFlushTextureState();
}
}
