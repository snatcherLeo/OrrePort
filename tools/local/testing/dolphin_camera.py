"""Read the active field camera from a Dolphin save state (testing lane). Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set CAM_STATE=<state file in that folder>
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_camera.py

Loads the state, waits 60 frames with no input, and writes <state>.camera.json + <state>.camera.png (the same frame):
the render camera is resource (group 0, id 0) in the GSres table (*lbl_8047AC5C, count lbl_8047AC60, 0x14-byte entries:
+4 data, +8 group, +0xC id; gs_res.c); from it (GSRenderCamera, gs_render_util.h) eye +0x70 and interest +0x100, and its
HSD camera object (+0x0C, HSD_CObj, hsd_cobj.h): near +0x38, far +0x3C, fov +0x40, aspect +0x44, projection type +0x50,
and the eye / interest WObj positions (+0x24 / +0x28 -> WObj +0x0C) as a cross-check; plus every animated map model set's
frame (e.g. S1_out: set 4, the sky ring, 452 of 29,999 at the arrival moment).
"""
import json, os, struct, zlib
from dolphin import event, memory, savestate

OUT = os.environ.get("POS_OUT", "."); STATE = os.environ["CAM_STATE"]
frame = 0; shot = []

def vec(a): return [memory.read_f32(a + 4 * i) for i in range(3)]

def find(group, ident):
    tab, n = memory.read_u32(0x8047AC5C), memory.read_u32(0x8047AC60)
    for i in range(min(n, 4096)):
        e = tab + 0x14 * i
        d = memory.read_u32(e + 4)
        if d and memory.read_u32(e + 8) == group and memory.read_u32(e + 0xC) == ident:
            return d
    return None

def on_frame():
    global frame
    frame += 1
    if frame == 1:
        savestate.load_from_file(os.path.join(OUT, STATE)); return
    if frame != 60:
        return
    cam = find(0, 0); res = {"state": STATE, "camera": hex(cam) if cam else None}
    if cam:
        res.update(eye=vec(cam + 0x70), interest=vec(cam + 0x100), up=vec(cam + 0xF4))
        cobj = memory.read_u32(cam + 0x0C)
        if 0x80000000 <= cobj < 0x81800000:
            eyew, intw = memory.read_u32(cobj + 0x24), memory.read_u32(cobj + 0x28)
            res.update(cobj=hex(cobj), near=memory.read_f32(cobj + 0x38), far=memory.read_f32(cobj + 0x3C),
                       fov=memory.read_f32(cobj + 0x40), aspect=memory.read_f32(cobj + 0x44),
                       projection_type=memory.read_u8(cobj + 0x50),
                       cobj_eye=vec(eyew + 0xC) if eyew else None, cobj_interest=vec(intw + 0xC) if intw else None)
    # animation state of every map model set (GSres ids ...1000 + set; GSmodel +0x94 rate, +0x98 frame, +0xA0 end frame,
    # gs_model_struct.h), so a PC render can put animated sets (sky rings, props) at the same frame
    sets = []
    tab, n = memory.read_u32(0x8047AC5C), memory.read_u32(0x8047AC60)
    for i in range(min(n, 4096)):
        e = tab + 0x14 * i; d = memory.read_u32(e + 4); ident = memory.read_u32(e + 0xC)
        if d and (ident & 0xFF00) == 0x1000:
            end = memory.read_f32(d + 0xA0)
            if 0 < end < 1e6:   # GSmodels only (raw model descriptors read as tiny or huge numbers)
                sets.append({"id": hex(ident), "set": ident & 0xFF, "rate": memory.read_f32(d + 0x94),
                             "frame": memory.read_f32(d + 0x98), "end": end})
    res["animated_model_sets"] = sets
    json.dump(res, open(os.path.join(OUT, STATE + ".camera.json"), "w"), indent=1)
    shot.append(1)
event.on_frameadvance(on_frame)

def on_drawn(w, h, d):
    if shot:
        raw = b"".join(b"\x00" + d[y * w * 3:(y + 1) * w * 3] for y in range(h))
        ch = lambda t, x: struct.pack(">I", len(x)) + t + x + struct.pack(">I", zlib.crc32(t + x) & 0xFFFFFFFF)
        open(os.path.join(OUT, STATE + ".camera.png"), "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(
            ">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw, 6)) + ch(b"IEND", b""))
        os._exit(0)
event.on_framedrawn(on_drawn)
