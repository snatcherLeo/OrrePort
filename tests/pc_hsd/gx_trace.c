/*
 * Diagnostics: print the material set-up HSD sends for the first meshes of a frame (linked with -Wl,--wrap=<name>
 * for each function below). pc_hsd_trace > 0 enables it and counts down per display list.
 */
#include <stdio.h>

typedef unsigned char u8;
typedef unsigned int u32;
typedef struct { u8 r, g, b, a; } GXColor;
int pc_hsd_trace;
#define T(...) do { if (pc_hsd_trace > 0) { printf("  gx: " __VA_ARGS__); } } while (0)

void __real_GXSetTevColorIn(int s, int a, int b, int c, int d);
int pc_hsd_notex; /* experiment: replace the texture colour input (TEXC, 8) by C0 (2) */
void __wrap_GXSetTevColorIn(int s, int a, int b, int c, int d) {
    T("TevColorIn s%d %d %d %d %d\n", s, a, b, c, d);
    if (pc_hsd_notex) {
        a = a == 8 ? 2 : a; b = b == 8 ? 2 : b; c = c == 8 ? 2 : c; d = d == 8 ? 2 : d;
    }
    if (pc_hsd_notex == 2) { /* white: a = b = c = ZERO, d = ONE */
        a = b = c = 15; d = 12;
    }
    if (pc_hsd_notex == 3) { /* stage 0 outputs C0, later stages pass CPREV */
        a = b = c = 15; d = s == 0 ? 2 : 0;
    }
    if (pc_hsd_notex == 4) { /* stage 1 (which has colour channel 0) outputs RASC alone */
        if (s == 1) { a = b = c = 15; d = 10; }
    }
    __real_GXSetTevColorIn(s, a, b, c, d);
}
void __real_GXSetTevAlphaIn(int s, int a, int b, int c, int d);
void __wrap_GXSetTevAlphaIn(int s, int a, int b, int c, int d) { T("TevAlphaIn s%d %d %d %d %d\n", s, a, b, c, d); __real_GXSetTevAlphaIn(s, a, b, c, d); }
void __real_GXSetTevOrder(int s, int coord, int map, int chan);
void __wrap_GXSetTevOrder(int s, int coord, int map, int chan) { T("TevOrder s%d coord %d map %d chan %d\n", s, coord, map, chan); __real_GXSetTevOrder(s, coord, map, chan); }
void __real_GXSetChanCtrl(int chan, u8 en, int amb, int mat, u32 mask, int diff, int attn);
int pc_hsd_nolight; /* experiment: lighting off in every colour channel (the channel outputs its material colour) */
void __wrap_GXSetChanCtrl(int chan, u8 en, int amb, int mat, u32 mask, int diff, int attn) {
    T("ChanCtrl %d en %d amb %d mat %d mask 0x%x diff %d attn %d\n", chan, en, amb, mat, mask, diff, attn);
    __real_GXSetChanCtrl(chan, pc_hsd_nolight ? 0 : en, amb, mat, mask, diff, attn);
}
void __real_GXSetChanMatColor(int chan, GXColor c);
/* (A workaround for Aurora writing the whole channel colour register for GX_COLORn and GX_ALPHAn alike lived here
 * until B's Aurora fix 14631cfa4, 2026-10-15; the calls now go straight through.) */
void __wrap_GXSetChanMatColor(int chan, GXColor c) {
    T("ChanMatColor %d %02x%02x%02x%02x\n", chan, c.r, c.g, c.b, c.a);
    __real_GXSetChanMatColor(chan, c);
}
void __real_GXSetChanAmbColor(int chan, GXColor c);
void __wrap_GXSetChanAmbColor(int chan, GXColor c) {
    T("ChanAmbColor %d %02x%02x%02x%02x\n", chan, c.r, c.g, c.b, c.a);
    __real_GXSetChanAmbColor(chan, c);
}
void __real_GXLoadTexObj(void* obj, int id);
void __wrap_GXLoadTexObj(void* obj, int id) { T("LoadTexObj map %d\n", id); __real_GXLoadTexObj(obj, id); }
void __real_GXSetNumTevStages(u8 n);
void __wrap_GXSetNumTevStages(u8 n) { T("NumTevStages %d\n", n); __real_GXSetNumTevStages(n); }
void __real_GXSetNumChans(u8 n);
void __wrap_GXSetNumChans(u8 n) { T("NumChans %d\n", n); __real_GXSetNumChans(n); }
void __real_GXSetTevOp(int s, int mode);
void __wrap_GXSetTevOp(int s, int mode) { T("TevOp s%d mode %d\n", s, mode); __real_GXSetTevOp(s, mode); }
typedef struct { short r, g, b, a; } GXColorS10;
void __real_GXSetTevColorS10(int id, GXColorS10 c);
void __wrap_GXSetTevColorS10(int id, GXColorS10 c) { T("TevColorS10 reg %d %d %d %d %d\n", id, c.r, c.g, c.b, c.a); __real_GXSetTevColorS10(id, c); }
void __real_GXSetTevColor(int id, GXColor c);
void __wrap_GXSetTevColor(int id, GXColor c) { T("TevColor reg %d %02x%02x%02x%02x\n", id, c.r, c.g, c.b, c.a); __real_GXSetTevColor(id, c); }
void __real_GXSetTevKColor(int id, GXColor c);
void __wrap_GXSetTevKColor(int id, GXColor c) { T("TevKColor %d %02x%02x%02x%02x\n", id, c.r, c.g, c.b, c.a); __real_GXSetTevKColor(id, c); }
void __real_GXSetTevKColorSel(int s, int sel);
void __wrap_GXSetTevKColorSel(int s, int sel) { T("TevKColorSel s%d sel %d\n", s, sel); __real_GXSetTevKColorSel(s, sel); }
void __real_GXSetTevColorOp(int s, int op, int bias, int scale, u8 clamp, int reg);
void __wrap_GXSetTevColorOp(int s, int op, int bias, int scale, u8 clamp, int reg) { T("TevColorOp s%d op %d bias %d scale %d clamp %d reg %d\n", s, op, bias, scale, clamp, reg); __real_GXSetTevColorOp(s, op, bias, scale, clamp, reg); }
void __real_GXSetTevAlphaOp(int s, int op, int bias, int scale, u8 clamp, int reg);
void __wrap_GXSetTevAlphaOp(int s, int op, int bias, int scale, u8 clamp, int reg) { T("TevAlphaOp s%d op %d bias %d scale %d clamp %d reg %d\n", s, op, bias, scale, clamp, reg); __real_GXSetTevAlphaOp(s, op, bias, scale, clamp, reg); }
void __real_GXSetBlendMode(int type, int src, int dst, int op);
void __wrap_GXSetBlendMode(int type, int src, int dst, int op) { T("BlendMode type %d src %d dst %d op %d\n", type, src, dst, op); __real_GXSetBlendMode(type, src, dst, op); }
void __real_GXSetTevSwapMode(int s, int ras, int tex);
void __wrap_GXSetTevSwapMode(int s, int ras, int tex) { T("TevSwapMode s%d ras %d tex %d\n", s, ras, tex); __real_GXSetTevSwapMode(s, ras, tex); }
void __real_GXSetTevSwapModeTable(int t, int r, int g, int b, int a);
void __wrap_GXSetTevSwapModeTable(int t, int r, int g, int b, int a) { T("SwapTable %d: %d %d %d %d\n", t, r, g, b, a); __real_GXSetTevSwapModeTable(t, r, g, b, a); }
void __real_GXSetAlphaCompare(int c0, u8 r0, int op, int c1, u8 r1);
void __wrap_GXSetAlphaCompare(int c0, u8 r0, int op, int c1, u8 r1) { T("AlphaCompare %d %d op %d %d %d\n", c0, r0, op, c1, r1); __real_GXSetAlphaCompare(c0, r0, op, c1, r1); }
int pc_hsd_cullswap; /* experiment: swap front / back culling (GX_CULL_FRONT 1 <-> GX_CULL_BACK 2) */
void __real_GXSetCullMode(int mode);
void __wrap_GXSetCullMode(int mode) {
    T("CullMode %d\n", mode);
    if (pc_hsd_cullswap && (mode == 1 || mode == 2)) {
        mode = 3 - mode;
    }
    __real_GXSetCullMode(mode);
}
int pc_hsd_nofog; /* experiment: GX fog off */
void __real_GXSetFog(int type, float startz, float endz, float nearz, float farz, GXColor color);
void __wrap_GXSetFog(int type, float startz, float endz, float nearz, float farz, GXColor color) {
    T("Fog type %d start %g end %g near %g far %g color %02x%02x%02x\n", type, startz, endz, nearz, farz, color.r, color.g, color.b);
    __real_GXSetFog(pc_hsd_nofog ? 0 : type, startz, endz, nearz, farz, color);
}
void __real_GXBegin(int prim, int fmt, unsigned short n);
void __wrap_GXBegin(int prim, int fmt, unsigned short n) { T("Begin prim 0x%x fmt %d n %u\n", prim, fmt, n); __real_GXBegin(prim, fmt, n); }
void __real_GXSetVtxDesc(int attr, int type);
void __wrap_GXSetVtxDesc(int attr, int type) { T("VtxDesc attr %d type %d\n", attr, type); __real_GXSetVtxDesc(attr, type); }
void __real_GXSetVtxAttrFmt(int fmt, int attr, int cnt, int type, u8 frac);
void __wrap_GXSetVtxAttrFmt(int fmt, int attr, int cnt, int type, u8 frac) { T("VtxAttrFmt fmt %d attr %d cnt %d type %d frac %d\n", fmt, attr, cnt, type, frac); __real_GXSetVtxAttrFmt(fmt, attr, cnt, type, frac); }
void __real_GXClearVtxDesc(void);
void __wrap_GXClearVtxDesc(void) { T("ClearVtxDesc\n"); __real_GXClearVtxDesc(); }
void __real_GXSetColorUpdate(u8 e);
void __wrap_GXSetColorUpdate(u8 e) { T("ColorUpdate %d\n", e); __real_GXSetColorUpdate(e); }
void __real_GXSetAlphaUpdate(u8 e);
void __wrap_GXSetAlphaUpdate(u8 e) { T("AlphaUpdate %d\n", e); __real_GXSetAlphaUpdate(e); }
void __real_GXSetZMode(u8 c, int f, u8 u);
void __wrap_GXSetZMode(u8 c, int f, u8 u) { T("ZMode cmp %d func %d upd %d\n", c, f, u); __real_GXSetZMode(c, f, u); }
void __real_GXSetProjection(const float m[4][4], int type);
void __wrap_GXSetProjection(const float m[4][4], int type) { T("Projection type %d [%g %g %g %g / %g %g %g %g / %g %g %g %g]\n", type, m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3]); __real_GXSetProjection(m, type); }
void __real_GXLoadPosMtxImm(const float m[3][4], u32 id);
void __wrap_GXLoadPosMtxImm(const float m[3][4], u32 id) { T("PosMtx %u [%g %g %g %g / %g %g %g %g / %g %g %g %g]\n", id, m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3]); __real_GXLoadPosMtxImm(m, id); }
void __real_GXSetTexCoordGen2(int dst, int func, int src, u32 mtx, int normalize, u32 pt);
void __wrap_GXSetTexCoordGen2(int dst, int func, int src, u32 mtx, int normalize, u32 pt) { T("TexCoordGen %d func %d src %d mtx %u norm %d pt %u\n", dst, func, src, mtx, normalize, pt); __real_GXSetTexCoordGen2(dst, func, src, mtx, normalize, pt); }
void __real_GXLoadTexMtxImm(const float m[][4], u32 id, int type);
void __wrap_GXLoadTexMtxImm(const float m[][4], u32 id, int type) { if (type == 0) T("TexMtx %u 3x4 [%g %g %g %g / %g %g %g %g / %g %g %g %g]\n", id, m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3], m[2][0], m[2][1], m[2][2], m[2][3]); else T("TexMtx %u 2x4 [%g %g %g %g / %g %g %g %g]\n", id, m[0][0], m[0][1], m[0][2], m[0][3], m[1][0], m[1][1], m[1][2], m[1][3]); __real_GXLoadTexMtxImm(m, id, type); }
void __real_GXSetCurrentMtx(u32 id);
void __wrap_GXSetCurrentMtx(u32 id) { T("CurrentMtx %u\n", id); __real_GXSetCurrentMtx(id); }
void __real_GXSetViewport(float x, float y, float w, float h, float n, float f);
void __wrap_GXSetViewport(float x, float y, float w, float h, float n, float f) { T("Viewport %g %g %g %g %g %g\n", x, y, w, h, n, f); __real_GXSetViewport(x, y, w, h, n, f); }
void __real_GXSetScissor(u32 x, u32 y, u32 w, u32 h);
void __wrap_GXSetScissor(u32 x, u32 y, u32 w, u32 h) { T("Scissor %u %u %u %u\n", x, y, w, h); __real_GXSetScissor(x, y, w, h); }
