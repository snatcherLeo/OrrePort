"""Message banks: UTF-16 text with 0xFFFF control codes (docs/engine/messages.md).
Read-only reader (no file output): a port candidate, see docs/tools.md "Readers"."""
import re, struct
from .fsys import archives, fsys_entries
from .tables import SEC4, common_rel

# Control codes (0xFFFF + one byte): the msgctrlcode table (src/game/data/data_80363774.c, 95 records) and the bytes
# each handler reads from the text after the code (src/game/msgctrl*.c: the handlers advance the text cursor).
# Mode (top two flag bits, gs_msg.c GSmsgDispatchControl): 0 = acts (layout, wait, colour), 1 = inserts a string,
# 2 = inserts a message by key. code: (handler name, argument bytes, mode)
CONTROL = {
    0x00: ("CR", 0, 0), 0x01: ("CR", 0, 0), 0x02: ("KeyEnd", 0, 0), 0x03: ("KeyWait", 0, 0), 0x04: ("RubyStart", 0, 0),
    0x05: ("RubyTop", 0, 0), 0x06: ("RubyEnd", 0, 0), 0x07: ("Font", 1, 0), 0x08: ("Color", 4, 0), 0x09: ("Wait", 1, 0),
    0x0D: ("EvStrBuf0", 0, 1), 0x0E: ("EvStrBuf1", 0, 1), 0x0F: ("AttackMons", 0, 1), 0x10: ("DeffenceMons", 0, 1),
    0x11: ("ClientMos", 0, 1), 0x12: ("TsuikaMons", 0, 1), 0x13: ("MyName", 0, 1), 0x14: ("MyMons", 0, 1),
    0x15: ("MyMons2", 0, 1), 0x16: ("EnemyMons", 0, 1), 0x17: ("EnemyMons2", 0, 1), 0x18: ("EnemyTmons", 0, 1),
    0x19: ("EnemyTmons2", 0, 1), 0x1A: ("SpeabiNamea", 0, 1), 0x1B: ("SpeabiNamed", 0, 1), 0x1C: ("SpeabiNamec", 0, 1),
    0x1D: ("SpeabiNamet", 0, 1), 0x1E: ("Clientnowork", 0, 1), 0x1F: ("SideAttackNameha", 0, 1),
    0x20: ("SideAttackNamewo", 0, 1), 0x21: ("SideAttackNameno", 0, 1), 0x22: ("TrainerType", 0, 1),
    0x23: ("TrainerName", 0, 1), 0x24: ("TrainerLose", 0, 1), 0x25: ("TrainerEnename", 0, 1),
    0x26: ("TrainerEnename2", 0, 1), 0x27: ("TrainerClientno", 0, 1), 0x28: ("WazaName", 0, 1), 0x29: ("ItemName", 0, 1),
    0x2A: ("PasoName", 0, 1), 0x2B: ("Hero", 0, 1), 0x2C: ("Hizuki", 0, 1), 0x2D: ("Item", 0, 2), 0x2E: ("Item2", 0, 2),
    0x2F: ("Digit", 0, 1), 0x30: ("Digit2", 0, 1), 0x31: ("MsgID", 0, 2), 0x32: ("Pokemon", 0, 1), 0x33: ("Pokemon2", 0, 1),
    0x34: ("MenuDigit", 0, 1), 0x35: ("MenuDigit2", 0, 1), 0x36: ("MenuPokemon", 0, 1), 0x37: ("MenuMsg", 0, 1),
    0x38: ("Palette", 1, 0), 0x39: ("Waza", 0, 2), 0x3D: ("SeOff", 0, 0), 0x3E: ("SeOn", 0, 0), 0x41: ("EvStrBuf2", 0, 1),
    0x42: ("SideDefenceNameha", 0, 1), 0x43: ("SideDefenceNamewo", 0, 1), 0x44: ("SideDefenceNameno", 0, 1),
    0x45: ("MenuUDigit", 0, 1), 0x46: ("MenuUDigit2", 0, 1), 0x47: ("MenuHex", 0, 1), 0x48: ("MenuHex2", 0, 1),
    0x49: ("MenuZDigit", 0, 1), 0x4A: ("MenuZDigit2", 0, 1), 0x4B: ("Money", 0, 1), 0x4C: ("Time", 0, 1),
    0x4D: ("String", 0, 1), 0x4E: ("PokemonID", 0, 2), 0x4F: ("MenuFullDigit", 0, 1), 0x50: ("MenuMoney", 0, 1),
    0x51: ("MenuMsg2", 0, 1), 0x52: ("Shadow", 1, 0), 0x53: ("Align", 1, 0), 0x54: ("TalkSE", 1, 0),
    0x55: ("MenuMsgID", 0, 2), 0x56: ("MenuMsgID2", 0, 2), 0x57: ("String2", 0, 1), 0x58: ("Tribe", 0, 2),
    0x59: ("Npc", 0, 2), 0x5A: ("IndentOff", 0, 0), 0x5B: ("LineSpace", 1, 0), 0x5C: ("BaseLineBias", 1, 0),
    0x5D: ("SndPlay", 0, 0), 0x5E: ("SndWait", 0, 0),
}  # codes 0x0A-0x0C, 0x3A-0x3C, 0x3F, 0x40 and above 0x5E have no handler (no arguments)


def bank_text(d):
    """[(message id, text)]: control codes as <XX>, or <XX:arguments in hex> for codes that take argument bytes."""
    count = struct.unpack(">H", d[4:6])[0]; lines = []
    for i in range(count):
        mid, o = struct.unpack(">II", d[0x10 + 8 * i:0x18 + 8 * i]); out = []; j = o
        while j + 1 < len(d):
            c = struct.unpack(">H", d[j:j + 2])[0]; j += 2
            if c == 0: break
            if c == 0xFFFF:
                code = d[j]; j += 1; n = CONTROL.get(code, ("", 0, 0))[1]
                out.append(f"<{code:02X}:{d[j:j + n].hex().upper()}>" if n else f"<{code:02X}>"); j += n; continue
            out.append(chr(c) if not 0xD800 <= c < 0xE000 else f"[{c:04X}]")
        lines.append((mid, "".join(out)))
    return lines


# Message banks embedded in common_rel (its resource table, tableResBiosGetResPtr entries 4-6):
# section-4 offsets. They hold species, move, item, ability and other shared names.
REL_BANKS = [("common_rel_bank4", 0x58044), ("common_rel_bank5", 0x647b4), ("common_rel_bank6", 0x76c94)]


def rel_banks(disc):
    rel = common_rel(disc)
    for name, off in REL_BANKS: yield name, rel[SEC4 + off:]


def all_text(disc):  # needs species_names' module for common_rel
    """{message id: text} over every bank on the disc (first bank wins)."""
    msgs = {}; seen = set()
    for name, data in rel_banks(disc):
        for mid, s in bank_text(data): msgs.setdefault(mid, s)
    for arch, arc in archives(disc):
        for k, name, t, ident, load in fsys_entries(arc):
            if t != 5 or ident in seen: continue
            seen.add(ident)
            for mid, s in bank_text(load()): msgs.setdefault(mid, s)
    return msgs


def clean(s): return re.sub(r"<[0-9A-F]{2}(?::[0-9A-F]*)?>", "", s.replace("<00>", " ")).strip()

