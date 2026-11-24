"""Read the SDK heap's size and free bytes from a Dolphin save state (testing lane; for B's option-1 memory check).

Runs inside Felk's Dolphin fork:

    set POS_OUT=<folder>   set HEAP_STATE=<state in it, e.g. title_idle.sav>
    Dolphin.exe -b -e "<Colosseum ISO>" --script dolphin_heap.py

Loads the state, waits 60 frames, and writes <state>.heap.json: for each OS heap (the SDK's OSHeapDesc array: {s32 size;
Cell* free; Cell* allocated}, 12 bytes each, at HeapArray (0x8047A6E8), or behind it if that word is a pointer), its size, the
number of free cells and the sum of their sizes (Cell: {Cell* prev; Cell* next; s32 size}, big-endian), and the same for
the allocated list. Compare with the PC's "OS avail memory" line at the end of boot (main.c).
"""
import json, os
from dolphin import event, memory, savestate

OUT = os.environ.get("POS_OUT", "."); STATE = os.environ.get("HEAP_STATE", "title_idle.sav")
DESC = 0x8047A6E8
frame = 0


def ram(a):
    return 0x80000000 <= a < 0x81800000


def walk(head):
    n, total, cells, seen = 0, 0, [], set()
    c = head
    while ram(c) and c not in seen and n < 100000:
        seen.add(c)
        size = memory.read_s32(c + 8)
        total += size
        n += 1
        if len(cells) < 20:
            cells.append([hex(c), size])
        c = memory.read_u32(c + 4)
    return {"cells": n, "bytes": total, "first": cells}


def heaps(base, count=4):
    out = []
    for i in range(count):
        d = base + 12 * i
        size, free, alloc = memory.read_s32(d), memory.read_u32(d + 4), memory.read_u32(d + 8)
        out.append({"heap": i, "desc": hex(d), "size": size, "free": walk(free), "allocated": walk(alloc)})
    return out


BOOT = os.environ.get("HEAP_BOOT") == "1"   # power-on mode: read at fn_800E0DDC's first call (main's "OS avail memory")
AVAIL_FN = 0x800E0DDC                        # needs "$800e0ddc nl" in the fork's GC6E01.ini


ARENA_LO, ARENA_HI = 0x80478988, 0x8047A6F8          # __OSArenaLo (.sdata), __OSArenaHi (.sbss)
GET_HI, GET_LO = 0x8009AFB0, 0x8009AFB8              # OSGetArenaHi / OSGetArenaLo (need "$8009afb0 nl", "$8009afb8 nl")
early = {}


def lowmem():
    """The boot info words the SDK's OSInit reads (big-endian u32), the BI2 pointer and its debug flag, the arena
    globals."""
    bi2 = memory.read_u32(0x800000F4)
    return {"bootinfo_arenaLo": hex(memory.read_u32(0x80000030)), "bootinfo_arenaHi": hex(memory.read_u32(0x80000034)),
            "fst_location": hex(memory.read_u32(0x80000038)), "fst_max_length": hex(memory.read_u32(0x8000003C)),
            "bi2": hex(bi2), "bi2_debug_flag": hex(memory.read_u32(bi2 + 0x0C)) if ram(bi2) else None,
            "__OSArenaLo": hex(memory.read_u32(ARENA_LO)), "__OSArenaHi": hex(memory.read_u32(ARENA_HI))}


def on_code(addr):
    """Power-on mode. The first calls of OSGetArenaLo / OSGetArenaHi (right after OSInit; a breakpoint reports only
    its first hit in this fork, which is the one wanted): record the arena globals and low memory then.
    fn_800E0DDC's first call is main's "OS avail memory" line (end of boot): read the heap there and finish."""
    if not BOOT:
        return
    if addr in (GET_LO, GET_HI) and addr not in early:
        early[addr] = dict(lowmem(), frame=frame)
        return
    if addr != AVAIL_FN:
        return
    word = memory.read_u32(DESC)
    res = {"mode": "power-on, at fn_800E0DDC's first call (main.c, 'OS avail memory')", "frame": frame,
           "label_word": hex(word), "as_pointer": heaps(word) if ram(word) else None,
           "lowmem_at_avail": lowmem(),
           "at_first_OSGetArenaLo": early.get(GET_LO), "at_first_OSGetArenaHi": early.get(GET_HI)}
    json.dump(res, open(os.path.join(OUT, "boot.heap.json"), "w"), indent=1)
    os._exit(0)


def on_frame():
    global frame
    frame += 1
    if BOOT:
        if frame > 3600:
            json.dump({"error": "fn_800E0DDC not hit in 3600 frames"}, open(os.path.join(OUT, "boot.heap.json"), "w"))
            os._exit(1)
        return
    if frame == 1:
        savestate.load_from_file(os.path.join(OUT, STATE))
        return
    if frame < 60:
        return
    word = memory.read_u32(DESC)
    res = {"state": STATE, "frame_after_load": frame, "label": hex(DESC), "label_word": hex(word),
           "as_array": heaps(DESC)}
    if ram(word):
        res["as_pointer"] = heaps(word)  # HeapArray (0x8047A6E8) is a 4-byte .sbss variable: the SDK's HeapArray pointer
    json.dump(res, open(os.path.join(OUT, STATE + ".heap.json"), "w"), indent=1)
    os._exit(0)


event.on_frameadvance(on_frame)
event.on_codebreakpoint(on_code)
