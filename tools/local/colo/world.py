"""World facts for the extracted/ READMEs: rooms (floor table), battle arenas, battles, treasure, and the
move-effect (WZX) lookup tables in main.dol. Docs: docs/engine/game-data-tables.md, battle-arenas.md,
models-animation.md ("Which WZX file a move uses").
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct
from .tables import table_rows, gsfsys_toc

# floor entry +0x00 class byte; meanings inferred from which rooms share a value (fsys-catalog.md)
FLOOR_CLASS = {0x20: "field room", 0x40: "Colosseum arena or battle-field room", 0x80: "move viewer",
               0xA0: "menu / special screen", 0xC0: "title / top menu"}
# floor entry +0x01 area byte (fsys-catalog.md, verified by snatcherLeo)
AREA_NAMES = {0: "none (battle-field rooms and other non-field rooms)", 1: "Outskirt Stand", 2: "Phenac City", 3: "Pyrite Town", 4: "Agate Village", 5: "The Under",
              6: "Shadow Pokémon Lab", 7: "Mt. Battle", 8: "Realgam Tower", 9: "no town (menus, system screens, T1)",
              10: "Team Snagem Hideout"}
# battle record +0x00 (battle-arenas.md; guesses from where each value occurs)
BATTLE_TYPE = {2: "trainer battle", 8: "Mt. Battle challenge", 3: "Colosseum round", 5: "Colosseum round",
               6: "Colosseum round", 7: "Colosseum round", 4: "tutorial / special", 10: "tutorial / special",
               11: "tutorial / special", 12: "tutorial / special", 17: "tutorial / special"}


def u16(b, o): return struct.unpack(">H", b[o:o + 2])[0]
def u32(b, o): return struct.unpack(">I", b[o:o + 4])[0]


def floors(rel, toc):
    """Floor table (lbl_80478FBC, 0x4C entries, floorDataBios*): one dict per room."""
    out = []
    for i, e in enumerate(table_rows(rel, 0x8bcf4, 0x14a670, 0x4C)):
        out.append(dict(index=i, cls=e[0], area=e[1], archive_id=u32(e, 4), archive=toc.get(u32(e, 4)),
                        map_res=u32(e, 8), room_id=u32(e, 0x0C)))
    return out


def arenas(rel):
    """Battle-floor table (fight_floor_data, 24 bytes, fightFloorDataBiosGetPtr)."""
    return [dict(index=i, kind=e[0], room_id=u16(e, 2), name_msg=u32(e, 4), music=u32(e, 0x0C))
            for i, e in enumerate(table_rows(rel, 0x8f974, 0x14a68c, 24))]


def battles(rel):
    """Battle table (fight_encount_data, 0x38 bytes, fightEncountDataBiosGetPtr)."""
    return [dict(index=i, type=e[0], arena=u16(e, 4), music=u32(e, 0x0C), trainers=[u16(e, o) for o in (0x18, 0x20, 0x28, 0x30)])
            for i, e in enumerate(table_rows(rel, 0x109fbc, 0x14a6b8, 0x38))]


def treasure(rel):
    """Treasure table (floor_tresure_list, 28 bytes, floorEventGetTresure*). Layout read from the data:
    +0x00 u8 container kind, +0x01 u8 quantity, +0x02 u16 facing (degrees), +0x04 u16 room id,
    +0x06 u16 story flag set when collected, +0x0C u32 item id, +0x10 f32 x, y, z."""
    out = []
    for i, e in enumerate(table_rows(rel, 0x11bfac, 0x14a6d0, 28)):
        if not u16(e, 4): continue
        out.append(dict(index=i, kind=e[0], quantity=e[1], angle=u16(e, 2), room_id=u16(e, 4), flag=u16(e, 6),
                        item=u32(e, 0x0C), pos=struct.unpack(">3f", e[0x10:0x1C])))
    return out


def dol_reader(dol):
    o = struct.unpack(">18I", dol[0:72]); a = struct.unpack(">18I", dol[72:144]); z = struct.unpack(">18I", dol[144:216])
    def rd(x, n):
        for O, A, S in zip(o, a, z):
            if A <= x < A + S: return dol[O + x - A:O + x - A + n]
        return b"\0" * n
    return rd


def move_effects(dol, toc):
    """Which WZX archive each move plays (fn_801DDD28 / fn_801DDC10, src/game/sequence.c):
    lbl_8036E150: one 0x1C record per move id {u32 key = 2000 + move id, 3 x {u32 archive id, u32 entry id}}
                  (part 1, 2, 3 = the sequence 'type' 1-3 the caller asks for);
    lbl_80373210: 0x20 records {u32 move id, u32 variant, 3 x {archive, entry}} (per-turn variants, e.g. Ice Ball);
    lbl_80373750: 0x10 records {u8 variant, u16 model id (pkxDataId), u16 move id, u32 archive, u32 entry}
                  (versions for one Pokémon model, e.g. Graveler's (golone) Self-Destruct);
    lbl_803727C8: 12-byte records {u32 archive, u32 entry, u32 name message} for sequence type 4 (ball throws...).
    Returns {archive name: [(how, move id or sequence index, detail)]}."""
    rd = dol_reader(dol); out = {}
    def add(arch_id, how, ident, detail=""):
        nm = toc.get(arch_id)
        if nm: out.setdefault(nm, []).append((how, ident, detail))
    for m in range(1, 0x26F0 // 0x1C):
        e = rd(0x8036E150 + m * 0x1C, 0x1C)
        for part in range(3):
            add(u32(e, 4 + part * 8), "move", m, f"part {part + 1} (lbl_8036E150)")
    for i in range(0x540 // 0x20):
        e = rd(0x80373210 + i * 0x20, 0x20)
        for part in range(3):
            add(u32(e, 8 + part * 8), "move", u32(e, 0), f"variant {u32(e, 4)}, part {part + 1} (lbl_80373210)")
    for i in range(0x16F0 // 0x10):
        e = rd(0x80373750 + i * 0x10, 0x10)
        add(u32(e, 8), "move", u16(e, 4), f"only for Pokémon model {u16(e, 2)} (pkxDataId), variant {e[0]} (lbl_80373750)")
    for i in range(0xA48 // 12):
        e = rd(0x803727C8 + i * 12, 12)
        add(u32(e, 0), "sequence", i, f"type-4 sequence {i}, name message {u32(e, 8):#x} (lbl_803727C8)")
    return out


def trainers(rel):
    """Trainer table (lbl_80478F24, 0x34 bytes; fightTrainerGetStatus fields 1-9 -> fn_801FCB.. setters):
    +0x00 u8 sex (1), +0x02 u16 class (4, KindDataId), +0x04 u16 first trainer-Pokémon id (5), +0x06 u16 party
    size (2), +0x08 u32 name message (3), +0x10 u32 battle-model index into lbl_80370840 (9, read by
    fn_801DE418). Fields 6-8 (+0x14 u16[], +0x0C, +0x24 u32[]) not named yet."""
    return [dict(index=i, sex=e[0], cls=u16(e, 2), first_pokemon=u16(e, 4), party=u16(e, 6), name_msg=u32(e, 8), model=u32(e, 0x10))
            for i, e in enumerate(table_rows(rel, 0x91684, 0x14a6ac, 0x34))]


def trainer_classes(rel):
    """Trainer-class table (12 bytes): +0x04 u32 class name message (others not named yet)."""
    return [dict(index=i, name_msg=u32(e, 4)) for i, e in enumerate(table_rows(rel, 0x8f724, 0x14a684, 12))]


# ---------------------------------------------------------------- REL modules: room tables (docs/engine/game-data-tables.md)
def rel_parse(d):
    """Dolphin REL: (sections [(file offset, size)], relocations [(section, offset, type, target module, target section, addend)])."""
    nsec, sio = struct.unpack(">II", d[0x0C:0x14]); impo, imps = struct.unpack(">II", d[0x28:0x30])
    secs = [(o & ~1, z) for o, z in (struct.unpack(">II", d[sio + 8 * s:sio + 8 * s + 8]) for s in range(nsec))]
    relocs = []
    for k in range(0, imps, 8):
        mid, off = struct.unpack(">II", d[impo + k:impo + k + 8]); p = off; sec = None; pos = 0
        while True:
            o, t, s, a = struct.unpack(">HBBI", d[p:p + 8]); p += 8
            if t == 203: break          # R_DOLPHIN_END
            if t == 202: sec = s; pos = 0; continue  # R_DOLPHIN_SECTION
            pos += o
            if t == 201: continue       # R_DOLPHIN_NOP
            relocs.append((sec, pos, t, mid, s, a))
    return secs, relocs


def rel_prolog_stores(d):
    """What a module's prolog stores into the executable: {main address: (section, offset) of the module data stored
    there}. Pairs each `@l` relocation of a main address (type 4) with the module `@l` relocation just before it in
    the code (lis/addi of the module address, then lis/stw to the main address). Also returns the module's own
    32-bit pointers {(section, offset): (section, offset)} (type 1)."""
    secs, rr = rel_parse(d); me = struct.unpack(">I", d[:4])[0]
    mains = [(pos, a) for sec, pos, t, mid, s, a in rr if sec == 1 and mid == 0 and t == 4]
    mods = [(pos, s, a) for sec, pos, t, mid, s, a in rr if sec == 1 and mid == me and t == 4]
    out = {}
    for pos, a in mains:
        c = [x for x in mods if pos - 0x14 <= x[0] < pos]
        if c: out.setdefault(a, (c[-1][1], c[-1][2]))
    ptrs = {(sec, pos): (s, a) for sec, pos, t, mid, s, a in rr if t == 1 and mid == me}
    return secs, out, ptrs


def floor_slots(rel):
    """Floor entry slot pointers (+0x10 lighting, +0x14 characters, +0x18 positions, +0x1C cameras): main addresses,
    from common_rel's relocations. {floor index: {0x10: addr, ...}}."""
    secs, rr = rel_parse(rel); out = {}
    for sec, pos, t, mid, s, a in rr:
        if sec == 4 and mid == 0 and t == 1 and 0x8bcf4 <= pos < 0x8bcf4 + 196 * 0x4C:
            i, f = divmod(pos - 0x8bcf4, 0x4C)
            if f in (0x10, 0x14, 0x18, 0x1C, 0x48): out.setdefault(i, {})[f] = a
    return out


def module_table(module, slot_addr):
    """A room module's table for one slot: the prolog stores a descriptor {u32* count, records*} into the slot's first
    word (floorDataBiosGetCharInfo reads it this way). Returns (count, records bytes from the module) or None."""
    secs, stores, ptrs = rel_prolog_stores(module)
    if slot_addr not in stores: return None
    dsec, doff = stores[slot_addr]
    cp, rp = ptrs.get((dsec, doff)), ptrs.get((dsec, doff + 4))
    if not cp or not rp: return None
    count = u32(module, secs[cp[0]][0] + cp[1])
    return count, module[secs[rp[0]][0] + rp[1]:]


def room_characters(module, slot_addr):
    """NPC placement records (0x24 bytes, floorCharacterBios*): flags, NPC info index (+0x06), character-name index
    (+0x08), movement script id (+0x10), talk script id (+0x14), position (+0x18)."""
    t = module_table(module, slot_addr)
    if not t: return []
    count, data = t
    return [dict(index=k, flags=data[0x24 * k], flags2=data[0x24 * k + 1], info=u16(data, 0x24 * k + 6),
                 name=u16(data, 0x24 * k + 8), move_script=u32(data, 0x24 * k + 0x10), talk_script=u32(data, 0x24 * k + 0x14),
                 pos=struct.unpack(">3f", data[0x24 * k + 0x18:0x24 * k + 0x24])) for k in range(count)]


def room_positions(module, slot_addr):
    """Position points (FloorPos, src/game/floor.c; floorDataBiosGetPosListPtr): 0x10 bytes {s16 direction in degrees,
    pad, f32 x, y, z}. The room-entry code puts the player at point N facing its direction (floor.c, the warp-in
    path). Returns [{index, x, y, z, direction}]."""
    t = module_table(module, slot_addr)
    if not t: return []
    count, d = t
    return [dict(index=k, direction=struct.unpack(">h", d[16 * k:16 * k + 2])[0],
                 x=struct.unpack(">f", d[16 * k + 4:16 * k + 8])[0], y=struct.unpack(">f", d[16 * k + 8:16 * k + 12])[0],
                 z=struct.unpack(">f", d[16 * k + 12:16 * k + 16])[0]) for k in range(count)]


def room_cameras(module, slot_addr):
    """Field-camera points (floorUpdateFieldCamera, src/game/field_camera_candidate_80117514.c): 0x18 bytes {f32 anchor
    x, y, z, f32 distance, f32 height, f32 Y rotation}. With one point the camera uses it directly; with several, the
    settings are blended by inverse squared distance from the player to each anchor (snapping to a point when the
    player is within a small radius of it). Returns [{index, anchor, distance, height, rot_y}]."""
    t = module_table(module, slot_addr)
    if not t: return []
    count, d = t
    out = []
    for k in range(count):
        v = struct.unpack(">6f", d[24 * k:24 * k + 24])
        out.append(dict(index=k, anchor=v[:3], distance=v[3], height=v[4], rot_y=v[5]))
    return out


def room_lighting(module, slot_addr):
    """Lighting record (floorDataBiosGetSunResID / GetShadowLightID / GetShadowRecive*, src/game/floor_data.c): the slot
    points straight at {u32 sun resource id, u32 shadow light id, u32 shadow receivers x 8 (0 = unused)}.
    Returns dict or None."""
    secs, stores, ptrs = rel_prolog_stores(module)
    if slot_addr not in stores: return None
    dsec, doff = stores[slot_addr]; o = secs[dsec][0] + doff
    w = struct.unpack(">10I", module[o:o + 40])
    return dict(sun=w[0], shadow_light=w[1], receivers=[x for x in w[2:] if x])


def room_tvs(module, slot_addr):
    """TV-screen table (floor entry +0x48; read by the room-entry code in src/game/field_range_80117E58.c): 0x18-byte
    records {u16 texture width, u16 height, u32 channel (the code starts the entry with 0), u32 screen model resource
    (its texture is replaced), u32 content model resource (opened and animated), u32 content animation index, u32
    camera resource}. The content is rendered into a width x height RGB565 texture shown on the screen model."""
    t = module_table(module, slot_addr)
    if not t: return []
    count, d = t
    return [dict(index=k, width=u16(d, 24 * k), height=u16(d, 24 * k + 2), channel=u32(d, 24 * k + 4), screen=u32(d, 24 * k + 8),
                 content=u32(d, 24 * k + 12), anim=u32(d, 24 * k + 16), camera=u32(d, 24 * k + 20)) for k in range(count)]


def room_gift_table(module):
    """A weighted random item table a room module registers into lbl_80478EB0 (count pointer) / lbl_80478EB4 (records):
    u16 item id, u16 weight each; drawn by the script native std087_sequenceUnk87 (src/game/etctool.c), which gives one
    item with probability weight / total (Agate Village's berry gift: first visit, then every 500 steps of flag 0xD1).
    Returns [(item id, weight)] or []."""
    secs, stores, ptrs = rel_prolog_stores(module)
    if 0x80478EB4 not in stores or 0x80478EB0 not in stores: return []
    rs, ro = stores[0x80478EB4]; cs, co = stores[0x80478EB0]
    n = u32(module, secs[cs][0] + co); b = secs[rs][0] + ro
    return [(u16(module, b + 4 * k), u16(module, b + 4 * k + 2)) for k in range(n)]


def shops(module):
    """Shop tables of the pocket_menu module (pocket_menu.fsys, type 14), read from the pointers its prolog registers
    (src/rel/pocket_menu/pocket_menu.c; readers in src/game/menuShop.c):
      lbl_80478E54 shop records, 4 bytes each, indexed by the shop number scripts pass to menuShopOpen:
        +0 u8 clerk-dialogue set, +1 u8 shop kind (std026_unkMenuSequence26 dispatches on it: 0 = mart, menu 0x62;
        1 = fn_8002CE6C, the vending machine; other = menu 0x83, the Poke Coupon exchanges, kinds 2-4), +2 u16 start of
        the stock list, in u16 units
        into lbl_80478E44 (shopOpenItemList walks it to the 0 terminator);
      lbl_80478E44 stock lists (u16 item ids, each ending in 0);
      lbl_80478E4C dialogue sets of 0x4C bytes (u8, then 18 u32 message ids; fn_80029FAC): coupon-exchange text;
      lbl_80478E3C dialogue sets of 0x3C bytes (u8, then 14 u32 message ids; fn_8002A0B8): mart / vending / herb shop
      text (which table a shop uses follows its kind: likely, from the message text of each set);
      lbl_80478E48 / E40 / E50 / E38 the counts: 0x4C sets, list u16s, shops, 0x3C sets.
    Returns dict(shops=[dict(index, dialogue, kind, items)], dialogue_4c=[[msg ids]], dialogue_3c=[[msg ids]])."""
    secs, stores, ptrs = rel_prolog_stores(module)
    at = lambda lbl: secs[stores[lbl][0]][0] + stores[lbl][1]
    nshop = u32(module, at(0x80478E50)); lists = at(0x80478E44); rec = at(0x80478E54)
    out = []
    for i in range(nshop):
        b = rec + 4 * i; o = lists + 2 * u16(module, b + 2); items = []
        while u16(module, o): items.append(u16(module, o)); o += 2
        out.append(dict(index=i, dialogue=module[b], kind=module[b + 1], items=items))
    def sets(lbl, cnt, size, n):
        return [[u32(module, at(lbl) + k * size + 4 + 4 * j) for j in range(n)] for k in range(u32(module, at(cnt)))]
    return dict(shops=out, dialogue_4c=sets(0x80478E4C, 0x80478E48, 0x4C, 18),
                dialogue_3c=sets(0x80478E3C, 0x80478E38, 0x3C, 14))


def common_table(rel, label):
    """(section, offset) of a common_rel table from the executable label its prolog fills (e.g. 0x80478E7C)."""
    secs, stores, _ = rel_prolog_stores(rel)
    s = stores.get(label)
    return (secs[s[0]][0] + s[1]) if s else None


def npc_info(rel):
    """NPC info table (peopleInfoData, 0x2C bytes, peopleInfoBiosGetPtr): +0x0C u32 overworld model entry id
    (people_archive / field_common). Other fields not named yet."""
    base = common_table(rel, 0x80478E7C)
    n = u32(rel, 0x184C + 0x14a6e8)
    return [dict(index=i, model=u32(rel, base + i * 0x2C + 0x0C)) for i in range(n)]


def character_names(rel):
    """Character-name table (char_name, 8 bytes, charNameBios*): +0x00 u16 (guess: 'name heard' flag), +0x04 name message."""
    base, cnt = common_table(rel, 0x80478F84), common_table(rel, 0x80478F80)
    n = u32(rel, cnt) if cnt else 607
    return [dict(index=i, flag=u16(rel, base + i * 8), name_msg=u32(rel, base + i * 8 + 4)) for i in range(n)]
