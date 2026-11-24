"""Data tables: common_rel (REL data module), main.dol tables, GSfsys.toc (docs/engine/game-data-tables.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import struct
from .fsys import fsys_entries

TABLES = [  # (name, section-4 offset, count offset, entry size or None)   docs/engine/game-data-tables.md
    ("species", 0x121a04, 0x14a6e0, 0x11C), ("moves", 0x11c7c4, 0x14a6d4, 0x38),
    ("trainers", 0x91684, 0x14a6ac, 0x34), ("trainer_pokemon", 0x9e5dc, 0x14a6b4, 0x50),
    ("trainer_classes", 0x8f724, 0x14a684, 12), ("trainer_ai", 0x9bd14, 0x14a6b0, 0x28),
    ("battles", 0x109fbc, 0x14a6b8, 0x38), ("battle_kinds", 0x90f4c, 0x14a694, 0x20),
    ("battle_floors", 0x8f974, 0x14a68c, 24), ("floors", 0x8bcf4, 0x14a670, 0x4C),
    ("treasure", 0x11bfac, 0x14a6d0, 28), ("npc_info", 0x13e440, 0x14a6e8, 0x2C),
    ("natures", 0x121614, 0x14a6d8, 0x28), ("shadow_pokemon", 0x1439d8, 0x14a6f8, 0x38),
    ("flags", 0x111e04, 0x14a6c0, 8), ("menu_faces", 0x44, 0x14a65c, 8),
    ("sound_effects", 0x13fe08, 0x14a6ec, 12),
]


SEC4 = 0x184C


def common_rel(disc):
    for k, name, t, ident, load in fsys_entries(disc.file("common.fsys")):
        if name == "common_rel": return load()


def table_rows(rel, off, cnt_off, size):
    count = struct.unpack(">I", rel[SEC4 + cnt_off:SEC4 + cnt_off + 4])[0]
    return [rel[SEC4 + off + i * size:SEC4 + off + (i + 1) * size] for i in range(count)]


def species_names(disc, rel, msgs=None):
    from .msg import all_text, clean
    msgs = msgs if msgs is not None else all_text(disc)
    names = {}
    for i, e in enumerate(table_rows(rel, 0x121a04, 0x14a6e0, 0x11C)):
        names[i] = clean(msgs.get(struct.unpack(">I", e[0x18:0x1C])[0], "")) or f"species{i:03d}"
    return names


def dol_table(disc, addr, count):
    """12-byte {archive id, entry id, name message id} records from main.dol (lbl_80370840 trainers, lbl_80370BD0 Pokémon)."""
    d = disc.dol()
    offs = struct.unpack(">18I", d[0:72]); addrs = struct.unpack(">18I", d[72:144]); sizes = struct.unpack(">18I", d[144:216])
    for O, A, S in zip(offs, addrs, sizes):
        if A <= addr < A + S:
            b = O + addr - A
            return [struct.unpack(">III", d[b + 12 * i:b + 12 * i + 12]) for i in range(count)]
    return []


def find_entry(disc, archive_id, ident, toc):
    name = toc.get(archive_id)
    if not name or f"{name}.fsys" not in disc.files: return None
    for k, ename, t, eid, load in fsys_entries(disc.file(f"{name}.fsys")):
        if eid == ident: return name, k, ename, load
    return None


def gsfsys_toc(disc):
    t = disc.file("GSfsys.toc"); cnt, tab = struct.unpack(">I", t[8:12])[0], struct.unpack(">I", t[0x10:0x14])[0]
    out = {}
    for k in range(cnt):
        i, o = struct.unpack(">II", t[tab + 8 * k:tab + 8 * k + 8]); out[i] = t[o:t.index(b"\0", o)].decode()
    return out


GX_TO_GS = {8: 0x00, 9: 0x01, 10: 0x30, 0: 0x40, 2: 0x41, 1: 0x42, 3: 0x43, 4: 0x44, 6: 0x45, 5: 0x90, 14: 0xB0}

