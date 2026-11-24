/**
 * @file msgctrl_exact_80132A38.c
 * @brief msgctrl.c tail: the message-variable setter and reset
 *        (0x80132A38 - 0x80132C6C).
 *
 * msgctrlSetValue stores the value one of the message control codes reads
 * (for example 0x4D/0x57 set the strings msgctrlString/msgctrlString2 print,
 * 0x55/0x56 the menu message IDs); other ids are ignored. msgctrlInitValue
 * clears the menu and string variables. The switch compiles to
 * jumptable_80363630, which this unit owns in .data; the case values are
 * read from that retail table (id - 0x0D indexes it), and the cases are in
 * the order their stores are laid out in retail.
 *
 * msgctrl.c was built at -O4,p with the peephole pass off: with that one
 * unit-wide flag (configure.py) and no local pragmas these two functions and
 * most of the TU's other functions are exact; retail keeps the redundant
 * clrlwi before each halfword store that the peephole pass removes.
 */
#include "dolphin/types.h"
#include "game/effect/effect_util_types.h"

void msgctrlSetValue(u32 id, u32 value)
{
    switch (id) {
    case 0x2D: _Item = (u16)value; return;
    case 0x2E: _Item2 = (u16)value; return;
    case 0x2F: lbl_8047AE54 = value; return;
    case 0x30: lbl_8047AE58 = value; return;
    case 0x31: lbl_8047AE5C = value; return;
    case 0x32: _Pokemon = value; return;
    case 0x33: _Pokemon2 = value; return;
    case 0x34: lbl_8047AE68 = value; return;
    case 0x35: lbl_8047AE6C = value; return;
    case 0x36: _MenuPokemon = value; return;
    case 0x37: _MenuMsg = value; return;
    case 0x51: _MenuMsg2 = value; return;
    case 0x39: _Waza = (u16)value; return;
    case 0x4D: _String = value; return;
    case 0x57: _String2 = value; return;
    case 0x4E: _PokemonID = (u16)value; return;
    case 0x4B: lbl_8047AE80 = value; return;
    case 0x4C: lbl_8047AE84 = value; return;
    case 0x50: lbl_8047AE94 = value; return;
    case 0x55: _MenuMsgID = value; return;
    case 0x56: _MenuMsgID2 = value; return;
    case 0x58: _Tribe = (u16)value; return;
    case 0x59: _Npc = (u16)value; return;
    case 0x5D: _SndID = (u16)value; return;
    case 0x0D: lbl_8047ADC8 = value; return;
    case 0x0E: lbl_8047ADCC = value; return;
    case 0x41: lbl_8047ADD0 = value; return;
    case 0x0F: _ATTACK_MONS = value; return;
    case 0x10: _DEFENCE_MONS = value; return;
    case 0x11: _CLIENT_MONS = value; return;
    case 0x12: _TSUIKA_MONS = value; return;
    case 0x13: lbl_8047ADE4 = value; return;
    case 0x14: lbl_8047ADE8 = value; return;
    case 0x15: lbl_8047ADEC = value; return;
    case 0x16: lbl_8047ADF0 = value; return;
    case 0x17: lbl_8047ADF4 = value; return;
    case 0x18: lbl_8047ADF8 = value; return;
    case 0x19: lbl_8047ADFC = value; return;
    case 0x1A: lbl_8047AE00 = value; return;
    case 0x1B: lbl_8047AE04 = value; return;
    case 0x1C: lbl_8047AE08 = value; return;
    case 0x1D: _SPEABI_NAMET = value; return;
    case 0x1E: _CLIENTNOWORK = value; return;
    case 0x1F: _SIDE_ATTACK_NAMEHA = value; return;
    case 0x20: _SIDE_ATTACK_NAMEWO = value; return;
    case 0x21: _SIDE_ATTACK_NAMENO = value; return;
    case 0x22: _TRAINER_TYPE = value; return;
    case 0x23: _TRAINER_NAME = value; return;
    case 0x24: _TRAINER_LOSE = value; return;
    case 0x25: _TRAINER_ENENAME = value; return;
    case 0x26: _TRAINER_ENENAME2 = value; return;
    case 0x27: _TRAINER_CLIENTNO = value; return;
    case 0x28: _WAZA_NAME = value; return;
    case 0x29: _ITEM_NAME = value; return;
    case 0x2A: _PASO_NAME = value; return;
    case 0x42: _SIDE_DEFENCE_NAMEHA = value; return;
    case 0x43: _SIDE_DEFENCE_NAMEWO = value; return;
    case 0x44: _SIDE_DEFENCE_NAMENO = value; return;
    }
}

void msgctrlInitValue(void)
{
    _MenuPokemon = 0;
    _MenuMsg = 0;
    _MenuMsg2 = 0;
    _Pokemon = 0;
    _Pokemon2 = 0;
    _String = 0;
    _String2 = 0;
}
