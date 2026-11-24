"""Colosseum save editor (2026-10-18): decode a memory-card save (.gci), edit the party and the bag from a recipe,
check the edits against the disc's own data, and write a new, re-signed .gci. Never changes the input file.

    python -I tools/local/colo/saveedit.py show <save.gci> <disc image>
    python -I tools/local/colo/saveedit.py edit <save.gci> <out.gci> <disc image> <recipe.json>

Format (docs/engine/save-data.md, pokemon-data.md; field names from the matched code): the card file holds 3 slots of
0x1E000 bytes; the game loads the valid slot with the highest save counter. A slot is encrypted with a SHA-1 key
stream (savefile.py). Writing a slot back: the plain slot with word 3 zeroed is hashed (the digest at +0x1DFEC), bytes
+0x18 .. +0x1DFD8 are encrypted (key = NOT digest; each 20-byte block XORed, the next key = SHA-1 of the encrypted
block), then word 3 is set so the slot's first eight stored words sum to 0 (as every valid slot on the card does,
verified on a real save 2026-10-18; the game repairs exactly that word when no slot verifies). The edited slot is the
one the game loads; the other slots and the card header stay byte for byte.

Player record (HeroSaveData, save block +0x70): name, rnd, 6 party records of 0x138 bytes, then the pockets
({u16 item, u16 count}): normal 0x14, key 0x2B, balls 0x10, TMs 0x40, berries 0x2E, colognes 3; money at +0xA84.
Pokémon record (PokemonBios, the first 0xFC bytes of a party slot): +0x00 species, +0x04 personality, +0x08 attest,
+0x0C catch place, +0x0E catch level, +0x0F ball, +0x10 OT gender, +0x14 OT id, +0x18 OT name, +0x2E nickname,
+0x44 original nickname, +0x5C EXP, +0x60 level, +0x78 moves {u16, u8 PP, u8 PP-ups} x4, +0x88 item, +0x8A HP,
+0x8C stats, +0x98 EVs, +0xA4 IVs (u16 each: HP, Atk, Def, SpA, SpD, Spe), +0xB0 friendship, +0xCC ability slot,
+0xD8 Shadow id, +0xDC heart gauge. A new Pokémon is made from a party member's record (same trainer, same stamps)
with every Pokémon-specific field replaced; bytes +0xFC .. +0x137 are copied from the template (their meaning is not
known yet: the same in both of a real save's records, so likely not per-Pokémon).

Recipe (JSON): {"add_pokemon": [{"species": 212, "level": 100, "moves": [206, 318, 163, 14], "nature": 3,
"ivs": 31, "nickname": null}], "set_items": [{"pocket": "normal", "item": 19, "count": 99}], "template_slot": 1,
"set_money": 5000000}   (set_money: optional, 0 .. 9,999,999, the game's cap; "clear_party": true empties the
six party slots first, keeping the template record, for battle-mode teams; a Pokémon with "to": "box" goes into the
first free PC box slot instead of the party; the "pc" pocket is the PC's item storage, 235 slots;
"zero_shadow_gauge": {"boxes": ["Shadow1", "Shadow2"]} empties the heart gauge of every Shadow Pokémon in those PC
boxes, ready for the Relic Stone; "modify_pokemon": [{"species": 243, "level": 100, "moves": [...], "ivs": [...]}]
changes the one party member of each species in place: level, EXP, stats, full HP, moves, IVs;
"unlock_battle_courses": [5] unlocks Battle Mode courses;
"set_flags": [{"id": 2673, "value": 4}] sets story flags / values by id (the disc's widths, read back to check);
"release_from_box": [{"box": 1, "slot": 8, "species": 57}] releases one boxed, non-Shadow Pokémon: the slot gets the
save's own empty-slot record, and the species must match)

Checks (each failure is reported; "illegal" ones are flagged, never refused, as snatcherLeo asked): the species exists,
each move is in the level-up list (at or below the level), the TM/HM list, or the egg moves; the level's EXP from the
growth rate; stats from the Gen 3 formula (base stats from the disc). Names, PP and growth rates come from the disc
via the readers; names of moves / items from extracted/data_tables (regenerable from the disc) when present.
"""
import csv, hashlib, json, os, struct, sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from colo import savefile
from colo.disc import Disc
from colo.tables import common_rel, table_rows

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
TABLES = os.path.join(REPO, "extracted", "data_tables")
HERO = 0x70
PARTY = HERO + 0x30
POCKETS = [("normal", 0x14), ("key", 0x2B), ("balls", 0x10), ("tms", 0x40), ("berries", 0x2E), ("colognes", 3)]
MONEY = HERO + 0xA84
REC = 0x138
# the PC (save-data.md section 3): 3 boxes of {0x14-byte header with the box name, 30 Pokémon x 0x138}, then 235 item
# slots {u16 item, u16 count}; checked on a real save 2026-10-22 (box names and boxed records decode, stats reproduce)
PCBOX, BOX_SIZE, NBOX, BOX_SLOTS = 0xB88, 0x24A4, 3, 30
PC_ITEMS, PC_ITEM_SLOTS = PCBOX + NBOX * BOX_SIZE, 235
TM_TABLE, TM_COUNT = 0x80368018, 58          # main.dol, 8-byte entries, move id at +6 (TM01-TM50, HM01-HM08)


def pocket_offsets():
    out, o = {}, PARTY + 6 * REC
    for name, n in POCKETS:
        out[name] = (o, n)
        o += 4 * n
    out["pc"] = (PC_ITEMS, PC_ITEM_SLOTS)                            # the PC's item storage, not a bag pocket
    return out


def box_slots():
    """Block offsets of the 90 PC box slots, box by box."""
    return [PCBOX + bx * BOX_SIZE + 0x14 + k * REC for bx in range(NBOX) for k in range(BOX_SLOTS)]


def box_names(block):
    """The three PC box names (the box header's first bytes, UTF-16BE, zero-terminated)."""
    return [bytes(block[PCBOX + bx * BOX_SIZE:PCBOX + bx * BOX_SIZE + 0x14]).decode("utf-16-be", "replace")
            .split("\0")[0] for bx in range(NBOX)]


def set_flag(save, gd, spec, report):
    """One story flag or value by id ({"id": 2673, "value": 4}; 2026-11-11). Its class, width and bit position come
    from the disc's definitions the way the game packs them (savefile.flag_layout, GSflagInitBitPos); the value is
    written into the class's buffer (save sections 4 / 5 / 6) in the game's layout: bits from the low end of big-endian
    u32 words, a value crossing a word putting its upper bits in the next word (_flagSet). Class-0 flags have no
    storage and are refused; a value too wide for the flag is refused."""
    fid, value = int(spec["id"]), int(spec["value"])
    layout = savefile.flag_layout(gd.flag_defs)
    if not 0 <= fid < len(layout):
        sys.exit(f"set_flags: flag {fid} is not defined (0 .. {len(layout) - 1})")
    cls, width, pos = layout[fid]
    if cls not in savefile.FLAG_SECTIONS:
        sys.exit(f"set_flags: flag {fid} is class {cls}, which the save doesn't store")
    if not 0 <= value < (1 << width):
        sys.exit(f"set_flags: flag {fid} is {width} bits wide; {value} doesn't fit")
    base, nwords = savefile.FLAG_SECTIONS[cls]
    old = savefile.flag_values(bytes(save.block), gd.flag_defs).get(fid)
    wi, bp = pos >> 5, pos & 31
    def word(i): return struct.unpack(">I", save.block[base + 4 * i:base + 4 * i + 4])[0]
    low_bits = min(width, 32 - bp)
    low_mask = ((1 << low_bits) - 1) << bp
    save.put(base + 4 * wi, ">I", (word(wi) & ~low_mask & 0xFFFFFFFF) | ((value << bp) & low_mask))
    if width > low_bits:                  # the upper bits continue in the next word
        if wi + 1 >= nwords:
            sys.exit(f"set_flags: flag {fid} runs past the end of its section")
        hi_bits = width - low_bits
        hi_mask = (1 << hi_bits) - 1
        save.put(base + 4 * (wi + 1), ">I", (word(wi + 1) & ~hi_mask & 0xFFFFFFFF) | ((value >> low_bits) & hi_mask))
    new = savefile.flag_values(bytes(save.block), gd.flag_defs).get(fid)
    if new != value:
        sys.exit(f"set_flags: flag {fid} reads back {new}, not {value}")
    report.append(f"flag {fid} (class {cls}, {width} bit{'s' if width > 1 else ''}): {old} -> {value}")


def zero_shadow_gauges(save, gd, boxes, report):
    """Empty the heart gauge (record +0xDC, `dp`) of every Shadow Pokémon in the named boxes, so the Relic Stone
    purifies it (snatcherLeo, 2026-10-24: the purify-all edit). Empty = 0: the gauge level is 6, "ready" (shadow-system.md);
    a Pokémon is still Shadow while its Shadow id (+0xD8) is set and the gauge is >= 0 (pokemonBiosGetDarkFlag).
    Nothing else changes: the purified flag, banked EXP and friendship stay for the ceremony to apply. The Shadow
    table's per-id counter (section 15, `darkPokemonSaveSetWalkSteps`) is a walking-step counter, not the gauge."""
    names = box_names(save.block)
    want = [names.index(n) for n in boxes if n in names]
    missing = [n for n in boxes if n not in names]
    if missing:
        sys.exit(f"no PC box named {missing}: the boxes are {names}")
    done = 0
    for bx in want:
        for k in range(BOX_SLOTS):
            p = PCBOX + bx * BOX_SIZE + 0x14 + k * REC
            sp, sid = save.u16(p), save.u16(p + 0xD8)
            dp = struct.unpack(">i", save.block[p + 0xDC:p + 0xE0])[0]
            if not sp or not sid:
                continue
            if dp <= 0:
                report.append(f"  {names[bx]} #{k + 1} {gd.name(sp)}: Shadow id {sid}, gauge {dp}: already empty, "
                              f"left as is")
                continue
            save.put(p + 0xDC, ">i", 0)
            done += 1
            report.append(f"  {names[bx]} #{k + 1} {gd.name(sp)}: Shadow id {sid}, gauge {dp} -> 0")
    report.append(f"heart gauges emptied: {done} Shadow Pokémon in {', '.join(boxes)}")


# ---- disc data -------------------------------------------------------------------------------------------------
class GameData:
    def __init__(self, disc_path):
        disc = Disc(disc_path)
        rel = common_rel(disc)
        self.species = table_rows(rel, 0x121a04, 0x14a6e0, 0x11C)
        self.flag_defs = table_rows(rel, 0x111e04, 0x14a6c0, 8)  # story-flag definitions (savefile.flag_layout)
        self.moves = table_rows(rel, 0x11c7c4, 0x14a6d4, 0x38)
        self.tm_moves = self._tm_table(disc)
        self.move_info = self._csv("moves_decoded.csv", "index")
        self.item_info = self._csv("items_decoded.csv", "item_id")
        sp = os.path.join(TABLES, "species.json")
        self.species_names = {e["index"]: e["name"] for e in json.load(open(sp))} if os.path.exists(sp) else {}

    @staticmethod
    def _csv(name, key):
        p = os.path.join(TABLES, name)
        if not os.path.exists(p):
            return {}
        return {int(r[key]): r for r in csv.DictReader(open(p, encoding="utf-8", errors="replace"))}

    @staticmethod
    def _tm_table(disc):
        d = disc.dol()
        offs, addrs, sizes = (struct.unpack(">18I", d[k:k + 72]) for k in (0, 72, 144))
        for O, A, S in zip(offs, addrs, sizes):
            if A <= TM_TABLE < A + S:
                b = O + TM_TABLE - A
                return [struct.unpack(">H", d[b + 8 * i + 6:b + 8 * i + 8])[0] for i in range(TM_COUNT)]
        raise ValueError("TM table not found in main.dol")

    def name(self, sp):
        return self.species_names.get(sp, f"species {sp}")

    def move_name(self, m):
        r = self.move_info.get(m)
        return (r["name"] or f"move {m}") if r else f"move {m}"

    def item_name(self, i):
        r = self.item_info.get(i)
        return (r["name"] or f"item {i}") if r else f"item {i}"

    def base_pp(self, m):
        r = self.move_info.get(m)
        if not r:
            raise ValueError(f"no PP for move {m}: extracted/data_tables/moves_decoded.csv missing")
        return int(r["pp"])

    def sp(self, s):
        """(growth, base stats [HP, Atk, Def, SpA, SpD, Spe], base friendship, abilities, level-up [(lv, move)],
        TM flags, egg moves) for species s (PokemonData, pokemon_data.h)."""
        e = self.species[s]
        base = list(struct.unpack(">6H", e[0x84:0x90]))
        learn = [(e[0xBA + 4 * k], struct.unpack(">H", e[0xBC + 4 * k:0xBE + 4 * k])[0]) for k in range(0x14)]
        return dict(growth=e[0], base=base, friend=struct.unpack(">H", e[8:10])[0], abilities=list(e[0x32:0x34]),
                    learn=[(lv, m) for lv, m in learn if m], tm=list(e[0x34:0x34 + TM_COUNT]),
                    egg=[m for m in struct.unpack(">8H", e[0x74:0x84]) if m], gender=e[2])

    def line(self, s):
        """s and the species it evolves from, back to the first of its line (the species records' evolution entries,
        +0x9C, 5 x {u8 kind, u8, u16 value, u16 species}, PokemonDataEvolution). An egg hatches as the first species
        of the line, so its egg moves come from that species' list (Donphan's Ancient Power is a Phanpy egg move)."""
        if not hasattr(self, "_from"):
            self._from = {}
            for t, e in enumerate(self.species):
                for k in range(5):
                    kind, _, _, to = struct.unpack(">BBHH", e[0x9C + 6 * k:0xA2 + 6 * k])
                    if kind and to and to not in self._from:
                        self._from[to] = t
        out = [s]
        while out[-1] in self._from and self._from[out[-1]] not in out:
            out.append(self._from[out[-1]])
        return out


# ---- formulas (Gen 3, the rules Colosseum uses; checked against a real record below) ---------------------------
def exp_for(growth, n):
    if n <= 1:
        return 0
    if growth == 0: return n ** 3                                        # medium fast
    if growth == 4: return 4 * n ** 3 // 5                               # fast
    if growth == 5: return 5 * n ** 3 // 4                               # slow
    if growth == 3: return 6 * n ** 3 // 5 - 15 * n ** 2 + 100 * n - 140  # medium slow
    if growth == 1:                                                      # erratic
        if n <= 50: return n ** 3 * (100 - n) // 50
        if n <= 68: return n ** 3 * (150 - n) // 100
        if n <= 98: return n ** 3 * ((1911 - 10 * n) // 3) // 500
        return n ** 3 * (160 - n) // 100
    if growth == 2:                                                      # fluctuating
        if n <= 15: return n ** 3 * ((n + 1) // 3 + 24) // 50
        if n <= 36: return n ** 3 * (n + 14) // 50
        return n ** 3 * (n // 2 + 32) // 50
    raise ValueError(f"growth rate {growth}")


NATURE_STAT = [1, 2, 5, 3, 4]   # nature n raises stat NATURE_STAT[n // 5], lowers NATURE_STAT[n % 5] (Atk Def Spe SpA SpD)


def stats(base, ivs, evs, level, nature):
    out = [(2 * base[0] + ivs[0] + evs[0] // 4) * level // 100 + level + 10]
    for k in range(1, 6):
        v = (2 * base[k] + ivs[k] + evs[k] // 4) * level // 100 + 5
        up, down = NATURE_STAT[nature // 5], NATURE_STAT[nature % 5]
        if up != down:
            v = v * 110 // 100 if k == up else (v * 90 // 100 if k == down else v)
        out.append(v)
    return out


# ---- slot crypto ------------------------------------------------------------------------------------------------
def encrypt_slot(plain, stored_word3_area):
    """Encrypted slot bytes from a plain slot (word 3 ignored): digest, key stream, then word 3 fixed so the first
    eight stored words sum to 0."""
    s = bytearray(plain)
    s[0x0C:0x10] = bytes(4)
    digest = hashlib.sha1(bytes(s[:savefile.ENC_END])).digest()
    s[savefile.DIGEST_AT:savefile.DIGEST_AT + 20] = digest
    key = bytes(b ^ 0xFF for b in digest)
    for o in range(savefile.ENC_START, savefile.ENC_END, 20):
        blk = bytes(a ^ b for a, b in zip(s[o:o + 20], key))
        s[o:o + 20] = blk
        key = hashlib.sha1(blk).digest()
    words = list(struct.unpack(">8I", s[:32]))
    words[3] = (-(sum(words) - words[3])) & 0xFFFFFFFF
    s[0x0C:0x10] = struct.pack(">I", words[3])
    return bytes(s)


class Save:
    def __init__(self, path):
        self.raw = open(path, "rb").read()
        self.head = len(self.raw) - (savefile.SLOTS_AT + savefile.NSLOTS * savefile.SLOT_SIZE)
        card = savefile.card_file(self.raw)
        best = None
        for i in range(savefile.NSLOTS):
            o = savefile.SLOTS_AT + i * savefile.SLOT_SIZE
            plain, ok = savefile.decrypt_slot(card[o:o + savefile.SLOT_SIZE])
            cnt = struct.unpack(">I", plain[4:8])[0]
            if ok and (best is None or cnt > best[1]):
                best = (i, cnt, bytearray(plain))
        if best is None:
            raise ValueError("no valid slot in this save")
        self.slot, self.counter, self.plain = best

    @property
    def block(self):
        return memoryview(self.plain)[savefile.BLOCK_AT:savefile.BLOCK_AT + savefile.BLOCK_SIZE]

    def write(self, path):
        card = bytearray(savefile.card_file(self.raw))
        o = savefile.SLOTS_AT + self.slot * savefile.SLOT_SIZE
        card[o:o + savefile.SLOT_SIZE] = encrypt_slot(self.plain, None)
        out = self.raw[:self.head] + bytes(card)
        open(path, "wb").write(out)
        return out

    def u16(self, o): return struct.unpack(">H", self.block[o:o + 2])[0]
    def put(self, o, fmt, *v): self.block[o:o + struct.calcsize(fmt)] = struct.pack(fmt, *v)

    def party(self):
        return [i for i in range(6) if self.u16(PARTY + i * REC)]

    def pocket(self, name):
        o, n = pocket_offsets()[name]
        return [struct.unpack(">HH", self.block[o + 4 * k:o + 4 * k + 4]) for k in range(n)]


def describe(save, gd):
    b = save.block
    lines = [f"slot {save.slot} (save counter {save.counter}); player {savefile.player_name(bytes(b))}; "
             f"money {struct.unpack('>i', b[MONEY:MONEY + 4])[0]}"]
    for i in save.party():
        p = PARTY + i * REC
        sp, lv = save.u16(p), b[p + 0x60]
        mv = [save.u16(p + 0x78 + 4 * k) for k in range(4)]
        st = list(struct.unpack(">6H", b[p + 0x8C:p + 0x98]))
        lines.append(f"  party {i}: {gd.name(sp)} ({sp}) Lv {lv}, HP {save.u16(p + 0x8A)}/{st[0]}, stats {st}, "
                     f"moves {[gd.move_name(m) for m in mv if m]}, item {save.u16(p + 0x88)}, "
                     f"nature {struct.unpack('>I', b[p + 4:p + 8])[0] % 25}, "
                     f"Shadow id {save.u16(p + 0xD8)}")
    for name in [n for n, _ in POCKETS] + ["pc"]:
        items = [(i, c) for i, c in save.pocket(name) if i]
        if items:
            lines.append(f"  {name}: " + ", ".join(f"{gd.item_name(i)} x{c}" for i, c in items))
    boxed = [(n, save.u16(p), b[p + 0x60]) for n, p in enumerate(box_slots()) if save.u16(p)]
    if boxed:
        lines.append(f"  PC boxes ({len(boxed)}): " + ", ".join(
            f"box {n // BOX_SLOTS + 1} #{n % BOX_SLOTS + 1} {gd.name(sp)} Lv {lv}" for n, sp, lv in boxed))
    return "\n".join(lines)


def check_template_stats(save, gd, slot):
    """The stat formula against a real record (the template): True when it reproduces the game's numbers."""
    p, b = PARTY + slot * REC, save.block
    sp, lv = save.u16(p), b[p + 0x60]
    ivs = list(struct.unpack(">6H", b[p + 0xA4:p + 0xB0]))
    evs = list(struct.unpack(">6H", b[p + 0x98:p + 0xA4]))
    nat = struct.unpack(">I", b[p + 4:p + 8])[0] % 25
    real = list(struct.unpack(">6H", b[p + 0x8C:p + 0x98]))
    calc = stats(gd.sp(sp)["base"], ivs, evs, lv, nat)
    return calc == real, real, calc, ivs, evs


def legality(gd, sp, level, moves):
    info, notes = gd.sp(sp), []
    for m in moves:
        how = []
        if any(mm == m and lv <= level for lv, mm in info["learn"]): how.append("level-up")
        if m in gd.tm_moves and info["tm"][gd.tm_moves.index(m)]: how.append("TM/HM")
        # egg moves: the species' own list and its pre-evolutions' (2026-10-28, the project's report: the egg comes from the
        # first species of the line, so Donphan inherits Phanpy's list)
        egg_from = [s for s in gd.line(sp) if m in gd.sp(s)["egg"]]
        if egg_from: how.append("egg move" if egg_from[0] == sp else f"egg move (as species {egg_from[-1]})")
        notes.append((m, how))
    return notes


def make_pid(nature, ot_id, gender_ratio, want_male=True, seed=0x5EED1234, upper_mod10=None):
    """A personality value with the wanted nature, not shiny, and the wanted gender where the species has two.
    upper_mod10 "low" / "high": (personality >> 16) % 10 below 5 / 5 or more (Wurmple's Silcoon / Cascoon split,
    evolution.md kinds 11 / 12)."""
    x = seed
    while True:
        x = (x * 0x41C64E6D + 0x6073) & 0xFFFFFFFF
        pid = x
        if pid % 25 != nature:
            continue
        if upper_mod10 is not None and (((pid >> 16) % 10) < 5) != (upper_mod10 == "low"):
            continue
        if ((ot_id >> 16) ^ (ot_id & 0xFFFF) ^ (pid >> 16) ^ (pid & 0xFFFF)) < 8:
            continue
        if gender_ratio not in (0, 0xFE, 0xFF) and ((pid & 0xFF) >= gender_ratio) != want_male:
            continue
        return pid


def name_utf16(text, size):
    raw = text.encode("utf-16-be")[:size - 2]
    return raw + bytes(size - len(raw))


def add_pokemon(save, gd, spec, template_slot, report, template_rec=None):
    if spec.get("to") == "box":                                     # the first free PC box slot (box 1 first)
        free = [n for n, p in enumerate(box_slots()) if not save.u16(p)]
        if not free:
            raise ValueError("the PC boxes are full")
        dst, where = box_slots()[free[0]], f"box {free[0] // BOX_SLOTS + 1} #{free[0] % BOX_SLOTS + 1}"
        free = [6 + free[0]]                                         # keeps the personality seeds apart from the party's
    else:
        free = [i for i in range(6) if not save.u16(PARTY + i * REC)]
        if not free:
            raise ValueError("party is full (\"to\": \"box\" puts a Pokémon in the PC)")
        dst, where = PARTY + free[0] * REC, f"party {free[0]}"
    src = PARTY + template_slot * REC
    b = save.block
    rec = bytearray(template_rec if template_rec is not None else b[src:src + REC])
    sp, lv = spec["species"], spec["level"]
    info = gd.sp(sp)
    ot_id = struct.unpack(">I", rec[0x14:0x18])[0]
    nature = spec.get("nature", 0)
    # a seed per species and party slot, so two new Pokémon with the same nature don't share a personality value
    pid = make_pid(nature, ot_id, info["gender"], spec.get("male", True),
                   seed=spec.get("pid_seed", (0x5EED1234 + sp * 7919 + free[0] * 104729) & 0xFFFFFFFF),
                   upper_mod10=spec.get("pid_upper_mod10"))
    iv = spec.get("ivs", 31)
    ivs = [iv] * 6 if isinstance(iv, int) else list(iv)
    evs = spec.get("evs", [0] * 6)
    st = stats(info["base"], ivs, evs, lv, nature)
    name = spec.get("nickname") or gd.name(sp)
    struct.pack_into(">H", rec, 0x00, sp)
    struct.pack_into(">I", rec, 0x04, pid)
    struct.pack_into(">H", rec, 0x0C, spec.get("catch_place", struct.unpack(">H", rec[0x0C:0x0E])[0]))
    rec[0x0E] = spec.get("catch_level", lv)
    rec[0x0F] = spec.get("ball", 4)
    rec[0x2E:0x44] = name_utf16(name, 0x16)
    rec[0x44:0x5C] = name_utf16(name, 0x18)
    struct.pack_into(">I", rec, 0x5C, exp_for(info["growth"], lv) if lv > 1 else 1)
    rec[0x60] = lv
    rec[0x64:0x78] = bytes(0x14)                                     # status condition and the rest of +0x64..
    for k in range(4):
        m = spec["moves"][k] if k < len(spec["moves"]) else 0
        struct.pack_into(">HBB", rec, 0x78 + 4 * k, m, gd.base_pp(m) if m else 0, 0)
    struct.pack_into(">H", rec, 0x88, spec.get("item", 0))
    struct.pack_into(">H", rec, 0x8A, st[0])
    struct.pack_into(">6H", rec, 0x8C, *st)
    struct.pack_into(">6H", rec, 0x98, *evs)
    struct.pack_into(">6H", rec, 0xA4, *ivs)
    struct.pack_into(">H", rec, 0xB0, spec.get("friendship", info["friend"]))
    rec[0xB2:0xCC] = bytes(0xCC - 0xB2)                              # contest stats, ribbons, medals, Pokérus
    rec[0xB3] = spec.get("beauty", 0)       # Beauty (pokemonBiosGetBeautiful, +0xB3): Feebas evolves at 170 or more
    rec[0xCC] = spec.get("ability_slot", 0)
    rec[0xCD] = 0                                                    # fusei (illegitimate) flag
    struct.pack_into(">H", rec, 0xD6, 0)
    struct.pack_into(">H", rec, 0xD8, 0)                             # not a Shadow Pokémon
    struct.pack_into(">iII", rec, 0xDC, 0, 0, 0)                     # heart gauge, stored EXP / friendship
    b[dst:dst + REC] = bytes(rec)
    legal = legality(gd, sp, lv, [m for m in spec["moves"] if m])
    report.append(f"{where}: {name} ({sp}) Lv {lv}, nature {nature}, IVs {ivs}, EVs {evs}, stats {st}, "
                  f"personality {pid:#010x} (upper % 10 = {(pid >> 16) % 10}), friendship "
                  f"{spec.get('friendship', info['friend'])}, beauty {rec[0xB3]}, item {spec.get('item', 0)}")
    for m, how in legal:
        report.append(f"    {gd.move_name(m)} ({m}): " + (", ".join(how) if how else
                      "NOT learnable by this species from its level-up, TM or egg lists (flagged, kept as asked)"))


def move_to_box(save, gd, spec, report):
    """Move a party member into the first free PC box slot, unchanged (snatcherLeo, 2026-11-03: the reference playthrough's Sneasel made room for
    Furret): {"species": 215}. Box slots hold the same record as party slots (add_pokemon's "to": "box" writes it the
    same way), so the record is copied as it is and its party slot emptied; nothing is deleted. A later add_pokemon
    fills the emptied slot (the first free one), and main() refuses a party left with a gap."""
    sp = spec["species"]
    slots = [i for i in save.party() if save.u16(PARTY + i * REC) == sp]
    if len(slots) != 1:
        sys.exit(f"move_to_box: {gd.name(sp)} ({sp}) is in the party {len(slots)} times (needs exactly one)")
    free = [n for n, p in enumerate(box_slots()) if not save.u16(p)]
    if not free:
        sys.exit("move_to_box: the PC boxes are full")
    src, dst = PARTY + slots[0] * REC, box_slots()[free[0]]
    b = save.block
    b[dst:dst + REC] = bytes(b[src:src + REC])
    b[src:src + REC] = bytes(REC)
    report.append(f"party {slots[0]}: {gd.name(sp)} ({sp}) moved to box {free[0] // BOX_SLOTS + 1} "
                  f"#{free[0] % BOX_SLOTS + 1}, unchanged")


def release_from_box(save, gd, spec, report):
    """Release one boxed Pokémon (snatcherLeo, 2026-11-04: "Yes, remove it", the reference playthrough's Primeape after Hitmonlee took its party
    slot): {"box": 1, "slot": 8, "species": 57}; box and slot count from 1, as the game shows them, and the species
    must match, so a wrong slot never clears another Pokémon. The slot gets the save's own empty-slot record: every
    empty box slot of a real save holds the same 0x138 bytes (zero except +0x10 = 2, +0xD0 = 0xFF, +0xDC = -100,
    verified on the reference playthrough's card 2026-11-04: 82 empty slots, one distinct record), so the bytes are copied from an empty
    slot of the same save; the edit refuses if the empty slots differ. The game's own release
    (`delPokemon__5PCBOXFScSc`, pcbox.c) does the same: it only runs `pokemonInit` on that one slot (heart gauge -100,
    the +0xDC value), nothing else in the save. Only Shadow id 0 (not Shadow): a Shadow
    Pokémon also has an entry in the Shadow table (shadow-system.md), which this step does not touch."""
    bx, k, sp = int(spec["box"]), int(spec["slot"]), spec["species"]
    if not (1 <= bx <= NBOX and 1 <= k <= BOX_SLOTS):
        sys.exit(f"release: box {bx} #{k} does not exist ({NBOX} boxes of {BOX_SLOTS})")
    slots = box_slots()
    p = slots[(bx - 1) * BOX_SLOTS + (k - 1)]
    if save.u16(p) != sp:
        sys.exit(f"release: box {bx} #{k} holds species {save.u16(p)}, not {gd.name(sp)} ({sp})")
    if save.u16(p + 0xD8):
        sys.exit(f"release: box {bx} #{k} is a Shadow Pokémon (Shadow id {save.u16(p + 0xD8)}); not supported")
    b = save.block
    empty = {bytes(b[q:q + REC]) for q in slots if not save.u16(q)}
    if len(empty) != 1:
        sys.exit(f"release: the save's empty box slots differ ({len(empty)} kinds), so there is no one empty record")
    lv = b[p + 0x60]
    b[p:p + REC] = empty.pop()
    report.append(f"box {bx} #{k}: {gd.name(sp)} ({sp}) Lv {lv} released (the slot now holds the save's empty record)")


def modify_pokemon(save, gd, spec, report):
    """Change a party Pokémon in place (snatcherLeo, 2026-10-24: E4, the Ho-Oh team): {"species": 243, "level": 100,
    "moves": [...], "ivs": [HP, Atk, Def, SpA, SpD, Spe] (optional)}. Finds the one party member of that species, checks
    that the stat formula reproduces its current stats, then sets the level, its EXP, the stats (its own EVs and nature
    kept), full HP, the moves at full PP and the IVs if given. Personality, trainer, nickname, purified state, ribbons
    and the rest stay as they are. Without "moves" (snatcherLeo, 2026-11-09: the reference playthrough's team to level 75) the moves, their PP and
    PP Ups stay exactly as they are."""
    sp = spec["species"]
    slots = [i for i in save.party() if save.u16(PARTY + i * REC) == sp]
    if len(slots) != 1:
        sys.exit(f"modify: {gd.name(sp)} ({sp}) is in the party {len(slots)} times (needs exactly one)")
    i = slots[0]
    ok, real, calc, _, _ = check_template_stats(save, gd, i)
    # "ev_lag_ok": the game recomputes stats only at a level-up, so EVs gained since the last one leave the stored stats
    # a point or two below the formula (the reference playthrough's Rex and Espeon); accepted only when every stat is 0-3 below, and reported
    lag = [c - r for r, c in zip(real, calc)]
    if not ok and spec.get("ev_lag_ok") and all(0 <= d <= 3 for d in lag):
        report.append(f"party {i}: {gd.name(sp)}'s stored stats {real} trail the formula {calc} by {lag} (EVs gained "
                      f"since its last level-up; the level-up applies them, as the game's own would)")
    elif not ok:
        sys.exit(f"modify: the stat formula doesn't reproduce {gd.name(sp)}'s stats (game {real}, formula {calc})")
    p = PARTY + i * REC
    b = save.block
    info = gd.sp(sp)
    nature = struct.unpack(">I", b[p + 4:p + 8])[0] % 25
    evs = list(struct.unpack(">6H", b[p + 0x98:p + 0xA4]))
    iv = spec.get("ivs")
    ivs = ([iv] * 6 if isinstance(iv, int) else list(iv)) if iv is not None else \
        list(struct.unpack(">6H", b[p + 0xA4:p + 0xB0]))
    lv = spec.get("level", b[p + 0x60])
    st = stats(info["base"], ivs, evs, lv, nature)
    save.put(p + 0x5C, ">I", exp_for(info["growth"], lv) if lv > 1 else 1)
    b[p + 0x60] = lv
    if "moves" in spec:
        for k in range(4):
            m = spec["moves"][k] if k < len(spec["moves"]) else 0
            save.put(p + 0x78 + 4 * k, ">HBB", m, gd.base_pp(m) if m else 0, 0)
    save.put(p + 0x8A, ">H", st[0])
    save.put(p + 0x8C, ">6H", *st)
    save.put(p + 0xA4, ">6H", *ivs)
    report.append(f"party {i}: {gd.name(sp)} ({sp}) -> Lv {lv}, nature {nature}, IVs {ivs}, EVs {evs}, stats {st}")
    for m, how in legality(gd, sp, lv, [m for m in spec.get("moves", []) if m]):
        report.append(f"    {gd.move_name(m)} ({m}): " + (", ".join(how) if how else
                      "NOT learnable by this species from its level-up, TM or egg lists (flagged, kept as asked)"))


def replace_move(save, gd, spec, report):
    """Swap one move of one party Pokémon and touch nothing else (snatcherLeo, 2026-11-05: the reference playthrough's Rex, Bite -> Ice Beam):
    {"species": 160, "from": 44, "to": 58}. Finds the one party member of that species and the one move slot holding
    `from`, and writes `to` there with its full base PP and no PP Ups (the record's move slot: u16 move, u8 PP, u8 PP
    Ups, +0x78 + 4 * slot). The other moves' PP, HP, stats and everything else stay as they are."""
    sp, old, new = spec["species"], spec["from"], spec["to"]
    slots = [i for i in save.party() if save.u16(PARTY + i * REC) == sp]
    if len(slots) != 1:
        sys.exit(f"replace_move: {gd.name(sp)} ({sp}) is in the party {len(slots)} times (needs exactly one)")
    i = slots[0]
    p = PARTY + i * REC
    moves = [save.u16(p + 0x78 + 4 * k) for k in range(4)]
    if moves.count(old) != 1:
        sys.exit(f"replace_move: {gd.name(sp)} knows {gd.move_name(old)} ({old}) {moves.count(old)} times (needs once)")
    if new in moves:
        sys.exit(f"replace_move: {gd.name(sp)} already knows {gd.move_name(new)} ({new})")
    k = moves.index(old)
    save.put(p + 0x78 + 4 * k, ">HBB", new, gd.base_pp(new), 0)
    report.append(f"party {i}: {gd.name(sp)} ({sp}) move slot {k}: {gd.move_name(old)} -> {gd.move_name(new)} "
                  f"(PP {gd.base_pp(new)}, no PP Ups)")
    for m, how in legality(gd, sp, save.block[p + 0x60], [new]):
        report.append(f"    {gd.move_name(m)} ({m}): " + (", ".join(how) if how else
                      "NOT learnable by this species from its level-up, TM or egg lists (flagged, kept as asked)"))


def hidden_power(ivs):
    """(type id, power) as the game computes it (pokemonGetMezamerupower, verified): bits from HP, Atk, Def, Spe, SpA,
    SpD; ivs in record order [HP, Atk, Def, SpA, SpD, Spe]; the game's type ids skip 9 (the "???" type)."""
    hp, at, df, sa, sd, sp = ivs
    order = [hp, at, df, sp, sa, sd]
    lo = sum((v & 1) << k for k, v in enumerate(order))
    hi = sum(((v >> 1) & 1) << k for k, v in enumerate(order))
    t = lo * 15 // 63 + 1
    return (t + 1 if t >= 9 else t), hi * 40 // 63 + 30


def set_item(save, pocket, item, count, report, every=False):
    o, n = pocket_offsets()[pocket]
    b = save.block
    slots = save.pocket(pocket)
    if every:
        # "all": true (2026-11-06, snatcherLeo: "the rare candy is twice in there ... make them both x99"): every stack of the
        # item in that pocket gets the count; an item not in the pocket is an error, nothing is added
        ks = [i for i, (it, _) in enumerate(slots) if it == item]
        if not ks:
            raise ValueError(f"{pocket} pocket has no item {item} (\"all\" changes existing stacks only)")
        for k in ks:
            struct.pack_into(">HH", b, o + 4 * k, item, count)
            report.append(f"{pocket}[{k}]: item {item} x{count} (all stacks)")
        return
    k = next((i for i, (it, _) in enumerate(slots) if it == item), None)
    if k is None:
        k = next((i for i, (it, _) in enumerate(slots) if it == 0), None)
    if k is None:
        raise ValueError(f"{pocket} pocket full")
    struct.pack_into(">HH", b, o + 4 * k, item, count)
    report.append(f"{pocket}[{k}]: item {item} x{count}")


def bm_suspended_run(save, spec, report):
    """Change the suspended Battle Mode Mt. Battle run (section 14, block +0xE3E8; save-data.md "A run in progress and
    a suspended run", verified 2026-11-11 from menuCBResumeYama / menuCBCanResume / the D2_cloud_4 script): the
    suspended copy of the seven run words at +0xC98C (wins at +0xC9A0) and the player memo (+0xC9A8: valid byte, resume
    room, previous room, position and rotation, the variables 0xAFC areas cleared, 0xAFD Ho-Oh given, 0xB11 rest stops,
    0xDE1). Resuming writes the words back, sets 0xB59 from the wins and puts the player in the memo's room; in cloud 4
    (room 0x5E) the next trainer slot is 0xB59 - 90. The run must already be suspended (a run the player started and
    quit), so the team, rule and battle type are the game's own; only the fields given are changed.
    spec: {"wins": 99, "room": 94, "prev_room": 218, "pos": [x, y, z], "rot": [x, y, z], "areas": 9, "rest_stops": 9}"""
    s14 = 0xE3E8
    b = save.block
    if b[s14 + 0xC98B] != 1:
        sys.exit("bm_suspended_run: this save has no suspended Battle Mode run (start Mt. Battle, win, Quit, then save)")
    mode, course = struct.unpack(">I", b[s14 + 0xC98C:s14 + 0xC990])[0], \
        struct.unpack(">I", b[s14 + 0xC998:s14 + 0xC99C])[0]
    if (mode, course) != (1, 6):
        sys.exit(f"bm_suspended_run: the suspended run is mode {mode}, course {course}, not Mt. Battle (1, 6)")

    def put_word(o, v, name, fmt=">I"):
        old = struct.unpack(fmt, b[s14 + o:s14 + o + 4])[0]
        save.put(s14 + o, fmt, v)
        report.append(f"Battle Mode suspended run: {name} {old} -> {v}")

    if "wins" in spec:
        if not 0 <= int(spec["wins"]) <= 99:
            sys.exit("bm_suspended_run: wins must be 0..99 (the 100th is the last battle)")
        put_word(0xC9A0, int(spec["wins"]), "wins")
    if "room" in spec:
        put_word(0xC9AC, int(spec["room"]), "resume room")
    if "prev_room" in spec:
        put_word(0xC9B0, int(spec["prev_room"]), "previous room")
    for k, v in enumerate(list(spec.get("pos", []))[:3]):
        put_word(0xC9B4 + 4 * k, float(v), f"position[{k}]", ">f")
    for k, v in enumerate(list(spec.get("rot", []))[:3]):
        put_word(0xC9C0 + 4 * k, float(v), f"rotation[{k}]", ">f")
    if "areas" in spec:
        put_word(0xC9CC, int(spec["areas"]), "0xAFC areas cleared")
    if "rest_stops" in spec:
        put_word(0xC9D4, int(spec["rest_stops"]), "0xB11 rest stops")


def main():
    a = sys.argv[1:]
    if not a or a[0] not in ("show", "edit"):
        sys.exit(__doc__)
    if a[0] == "show":
        save, gd = Save(a[1]), GameData(a[2])
        print(describe(save, gd))
        return
    src, out, disc, recipe = a[1:5]
    if os.path.abspath(src) == os.path.abspath(out):
        sys.exit("the output must be a new file")
    save, gd = Save(src), GameData(disc)
    r = json.load(open(recipe))
    tpl = r.get("template_slot", save.party()[0])
    ok, real, calc, ivs, evs = check_template_stats(save, gd, tpl)
    print(f"stat formula on the template (party {tpl}): {'matches the game' if ok else 'DIFFERS'} "
          f"(game {real}, formula {calc}, IVs {ivs}, EVs {evs})")
    # the check guards recipes that build or change Pokémon from the formula; a recipe that touches no Pokémon (flags,
    # items, money, courses) only reports it (2026-11-11: a flags-only edit was stopped by a template's EV lag)
    if not ok and (r.get("add_pokemon") or r.get("modify_pokemon")):
        sys.exit("stopping: the stat formula doesn't reproduce a real record")
    elif not ok:
        print("(this recipe makes or changes no Pokémon, so the stat check doesn't stop it)")
    report = []
    tpl_rec = None
    if r.get("clear_party"):
        # a battle-mode team replaces the whole party (E-BM): keep the template record (same trainer, same stamps),
        # then empty all six slots; the new Pokémon are made from the kept record
        tpl_rec = bytes(save.block[PARTY + tpl * REC:PARTY + (tpl + 1) * REC])
        save.block[PARTY:PARTY + 6 * REC] = bytes(6 * REC)
        report.append("party cleared (clear_party): the six slots emptied before adding")
    if r.get("move_to_box"):
        # keep the template record first: the moved member may be the template itself (2026-11-03)
        if tpl_rec is None:
            tpl_rec = bytes(save.block[PARTY + tpl * REC:PARTY + (tpl + 1) * REC])
        for spec in r["move_to_box"]:
            move_to_box(save, gd, spec, report)
    for spec in r.get("add_pokemon", []):
        add_pokemon(save, gd, spec, tpl, report, template_rec=tpl_rec)
    for spec in r.get("release_from_box", []):   # boxed Pokémon released (2026-11-04)
        release_from_box(save, gd, spec, report)
    filled = [bool(save.u16(PARTY + i * REC)) for i in range(6)]
    if any(not f and any(filled[i + 1:]) for i, f in enumerate(filled)):
        sys.exit(f"stopping: the party would have a gap (slots {filled}); add a Pokémon for each one moved")
    for it in r.get("set_items", []):
        set_item(save, it["pocket"], it["item"], it["count"], report, every=bool(it.get("all")))
    if r.get("zero_shadow_gauge"):       # {"boxes": ["Shadow1", "Shadow2"]}: the purify-all edit (E3)
        zero_shadow_gauges(save, gd, r["zero_shadow_gauge"]["boxes"], report)
    for spec in r.get("modify_pokemon", []):  # party members changed in place (E4)
        modify_pokemon(save, gd, spec, report)
    for spec in r.get("replace_move", []):  # one move swapped, nothing else touched (2026-11-05)
        replace_move(save, gd, spec, report)
    for spec in r.get("set_flags", []):  # story flags / values by id (2026-11-11, snatcherLeo's Mt. Battle edit)
        set_flag(save, gd, spec, report)
    for course in r.get("unlock_battle_courses", []):
        # Battle Mode's course unlock bytes: section 14 +0xCBD4 + course (savedataBattleIsCourseUnlocked, verified);
        # courses 0 Phenac, 1 Pyrite, 2 The Under, 3 Orre Colosseum Lv 50, 4 Realgam Tower, 5 Orre Colosseum Lv 100,
        # 6 Mt. Battle (verified 2026-11-11 from Battle Mode's code, docs/engine/battle-mode.md; new saves open 0, 1, 2, 4)
        o = 0xE3E8 + 0xCBD4 + int(course)
        report.append(f"Battle Mode course {course}: unlocked (was {save.block[o]})")
        save.block[o] = 1
    if r.get("bm_suspended_run"):         # a suspended Battle Mode Mt. Battle run (2026-11-11, snatcherLeo's Ho-Oh edit)
        bm_suspended_run(save, r["bm_suspended_run"], report)
    if "set_money" in r:                 # the game caps money at 9,999,999 (heroBiosSetPokedoru, bag-and-pc.md)
        money = int(r["set_money"])
        if not 0 <= money <= 9999999:
            sys.exit(f"set_money {money}: outside the game's range 0..9,999,999")
        save.put(MONEY, ">i", money)
        report.append(f"money: {money}")
    print("\n".join(report))
    save.write(out)
    check = Save(out)                                                # read back: valid, and the same save block
    same = check.slot == save.slot and bytes(check.block) == bytes(save.block)
    orig = savefile.card_file(save.raw)
    new = savefile.card_file(open(out, "rb").read())
    o = savefile.SLOTS_AT + save.slot * savefile.SLOT_SIZE
    others = orig[:o] == new[:o] and orig[o + savefile.SLOT_SIZE:] == new[o + savefile.SLOT_SIZE:]
    head = save.raw[:save.head] == open(out, "rb").read()[:save.head]
    print(f"written {out}: slot {save.slot} re-signed and valid on read-back: {same}; other slots and card header "
          f"unchanged: {others and head}")
    print(describe(check, gd))


if __name__ == "__main__":
    main()
