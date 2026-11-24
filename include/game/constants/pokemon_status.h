#ifndef GAME_CONSTANTS_POKEMON_STATUS_H
#define GAME_CONSTANTS_POKEMON_STATUS_H

/*
 * Field selectors for pokemonGetStatus(obj, id, selector, index) and pokemonSetStatus (same selectors): the one
 * numbered accessor everything about a Pokemon goes through. Selectors below 0x6E read the species table for species
 * `id`; 0x6E-0xC1 read a Pokemon record `obj`; 0xC2 onward read Shadow and battle-time state.
 *
 * Verified 2026-10-14 from pokemonGetStatus's own dispatch (0x8012640C): each selector calls one getter, whose real
 * name (symbols.txt) gives the meaning; the full table, with the getter names, is in docs/engine/pokemon-data.md
 * ("Selector table"). The names here are those getters in English (Basis = base stat, Effort = EV, Rnd = IV or
 * personality, Nimbleness = Speed, Abicnt = stat stage, Waza = move, Tokusei = ability, Zokusei = type, Dark = Shadow,
 * Dp = heart gauge, Tamago = egg). Selectors whose meaning is not certain (the "Amari" spare fields, the fusei flag,
 * a few battle flags) have no name here and stay numbers.
 * Added 2026-11-09 (named constants, snatcherLeo's plan of 2026-11-08: "stats", "ribbons").
 */

/* Species table (`id` = species) */
#define POKEMON_STATUS_SPECIES_NAME          0x01
#define POKEMON_STATUS_BASE_HP               0x03
#define POKEMON_STATUS_BASE_ATTACK           0x04
#define POKEMON_STATUS_BASE_DEFENSE          0x05
#define POKEMON_STATUS_BASE_SP_ATTACK        0x06
#define POKEMON_STATUS_BASE_SP_DEFENSE       0x07
#define POKEMON_STATUS_BASE_SPEED            0x08
#define POKEMON_STATUS_EV_YIELD_HP           0x0A
#define POKEMON_STATUS_EV_YIELD_ATTACK       0x0B
#define POKEMON_STATUS_EV_YIELD_DEFENSE      0x0C
#define POKEMON_STATUS_EV_YIELD_SP_ATTACK    0x0D
#define POKEMON_STATUS_EV_YIELD_SP_DEFENSE   0x0E
#define POKEMON_STATUS_EV_YIELD_SPEED        0x0F
#define POKEMON_STATUS_EXP_YIELD             0x10
#define POKEMON_STATUS_GROWTH_RATE           0x11
#define POKEMON_STATUS_CATCH_RATE            0x12
#define POKEMON_STATUS_GENDER_RATIO          0x13
#define POKEMON_STATUS_BASE_FRIENDSHIP       0x14
#define POKEMON_STATUS_SPECIES_HELD_ITEM     0x15  /* index: which of the two */
#define POKEMON_STATUS_SPECIES_TYPE          0x16  /* index: first or second type */
#define POKEMON_STATUS_SPECIES_ABILITY       0x17  /* index: first or second ability */
#define POKEMON_STATUS_EVOLUTION_METHOD      0x19
#define POKEMON_STATUS_EVOLUTION_PARAM       0x1A
#define POKEMON_STATUS_EVOLUTION_SPECIES     0x1B
#define POKEMON_STATUS_LEVEL_UP_MOVE_LEVEL   0x1D
#define POKEMON_STATUS_LEVEL_UP_MOVE         0x1E
#define POKEMON_STATUS_TM_HM_FIRST           0x20  /* 0x20-0x59: one selector per machine (compatibility) */
#define POKEMON_STATUS_TM_HM_LAST            0x59
#define POKEMON_STATUS_MENU_SPRITE_ID        0x5B
#define POKEMON_STATUS_SPECIES_COLOR         0x5C
#define POKEMON_STATUS_CATEGORY_NAME         0x5D  /* the Pokedex category ("TypeName") */
#define POKEMON_STATUS_HEIGHT                0x5E
#define POKEMON_STATUS_WEIGHT                0x5F
#define POKEMON_STATUS_DEX_TEXT              0x60
#define POKEMON_STATUS_CRY                   0x61
#define POKEMON_STATUS_SEEN_FLAG             0x62
#define POKEMON_STATUS_CAUGHT_FLAG           0x63
#define POKEMON_STATUS_HOENN_DEX_NUMBER      0x64  /* NumZukan: Hoenn's order first, then the rest */
#define POKEMON_STATUS_NATIONAL_DEX_NUMBER   0x65  /* NumPokemon */
#define POKEMON_STATUS_MODEL_ID              0x66  /* PkxDataId */
#define POKEMON_STATUS_EGG_MOVE              0x68  /* Kowaza */

/* Pokemon record (`obj`) */
#define POKEMON_STATUS_SPECIES               0x6E
#define POKEMON_STATUS_PERSONALITY           0x6F
#define POKEMON_STATUS_ATTEST                0x70  /* the record's origin stamp */
#define POKEMON_STATUS_MET_LOCATION          0x71
#define POKEMON_STATUS_MET_LEVEL             0x72
#define POKEMON_STATUS_BALL                  0x73
#define POKEMON_STATUS_OT_GENDER             0x74
#define POKEMON_STATUS_OT_ID                 0x75
#define POKEMON_STATUS_OT_NAME               0x76
#define POKEMON_STATUS_NICKNAME              0x77
#define POKEMON_STATUS_NICKNAME_ORIGINAL     0x78
#define POKEMON_STATUS_EXP                   0x79
#define POKEMON_STATUS_LEVEL                 0x7A
#define POKEMON_STATUS_IS_EMPTY              0x7B  /* computed: species 0 */
#define POKEMON_STATUS_CONDITION_SLOT        0x7C  /* the saved status condition (see conditions.h) */
#define POKEMON_STATUS_MOVE                  0x7F  /* index: move slot 0-3 */
#define POKEMON_STATUS_MOVE_PP               0x80  /* index: move slot */
#define POKEMON_STATUS_MOVE_PP_UPS           0x81  /* index: move slot */
#define POKEMON_STATUS_HELD_ITEM             0x82
#define POKEMON_STATUS_HP                    0x83
#define POKEMON_STATUS_MAX_HP                0x87
#define POKEMON_STATUS_ATTACK                0x88
#define POKEMON_STATUS_DEFENSE               0x89
#define POKEMON_STATUS_SP_ATTACK             0x8A
#define POKEMON_STATUS_SP_DEFENSE            0x8B
#define POKEMON_STATUS_SPEED                 0x8C
#define POKEMON_STATUS_EV_HP                 0x8D
#define POKEMON_STATUS_EV_ATTACK             0x8E
#define POKEMON_STATUS_EV_DEFENSE            0x8F
#define POKEMON_STATUS_EV_SP_ATTACK          0x90
#define POKEMON_STATUS_EV_SP_DEFENSE         0x91
#define POKEMON_STATUS_EV_SPEED              0x92
#define POKEMON_STATUS_IV_HP                 0x93
#define POKEMON_STATUS_IV_ATTACK             0x94
#define POKEMON_STATUS_IV_DEFENSE            0x95
#define POKEMON_STATUS_IV_SP_ATTACK          0x96
#define POKEMON_STATUS_IV_SP_DEFENSE         0x97
#define POKEMON_STATUS_IV_SPEED              0x98
#define POKEMON_STATUS_FRIENDSHIP            0x99
#define POKEMON_STATUS_CONTEST_COOL          0x9C  /* Style */
#define POKEMON_STATUS_CONTEST_BEAUTY        0x9D
#define POKEMON_STATUS_CONTEST_CUTE          0x9E
#define POKEMON_STATUS_CONTEST_SMART         0x9F  /* Clever */
#define POKEMON_STATUS_CONTEST_TOUGH         0xA0  /* Strong */
#define POKEMON_STATUS_CONTEST_SHEEN         0xA1  /* Fur */
#define POKEMON_STATUS_RIBBON_CHAMPION       0xA3
#define POKEMON_STATUS_RIBBON_WINNING        0xA4
#define POKEMON_STATUS_RIBBON_VICTORY        0xA5
#define POKEMON_STATUS_RIBBON_ARTIST         0xA6  /* Bromide */
#define POKEMON_STATUS_RIBBON_EFFORT         0xA7  /* Ganba */
#define POKEMON_STATUS_RIBBON_MARINE         0xA8
#define POKEMON_STATUS_RIBBON_LAND           0xA9
#define POKEMON_STATUS_RIBBON_SKY            0xAA
#define POKEMON_STATUS_RIBBON_COUNTRY        0xAB
#define POKEMON_STATUS_RIBBON_NATIONAL       0xAC
#define POKEMON_STATUS_RIBBON_EARTH          0xAD
#define POKEMON_STATUS_RIBBON_WORLD          0xAE
#define POKEMON_STATUS_COOL_MEDAL            0xB0  /* the contest ribbons' rank, per category */
#define POKEMON_STATUS_BEAUTY_MEDAL          0xB1
#define POKEMON_STATUS_CUTE_MEDAL            0xB2
#define POKEMON_STATUS_SMART_MEDAL           0xB3
#define POKEMON_STATUS_TOUGH_MEDAL           0xB4
#define POKEMON_STATUS_POKERUS               0xB5
#define POKEMON_STATUS_IS_EGG                0xB6
#define POKEMON_STATUS_ABILITY_SLOT          0xB7
#define POKEMON_STATUS_GENDER                0xBA  /* computed; see gender.h */
#define POKEMON_STATUS_BOX_MARKINGS          0xBB
#define POKEMON_STATUS_MAIL_ID               0xBC
#define POKEMON_STATUS_NATURE                0xBF  /* computed: personality mod 25; see natures.h */
#define POKEMON_STATUS_LEVEL_FROM_EXP        0xC0  /* computed */
#define POKEMON_STATUS_IS_SHINY              0xC1  /* computed */

/* Shadow data */
#define POKEMON_STATUS_TRAINER_POKEMON_ID    0xC2
#define POKEMON_STATUS_SHADOW_ID             0xC3
#define POKEMON_STATUS_HEART_GAUGE_INITIAL   0xC4
#define POKEMON_STATUS_HEART_GAUGE           0xC5
#define POKEMON_STATUS_STORED_EXP            0xC6  /* released when purified */
#define POKEMON_STATUS_STORED_FRIENDSHIP     0xC7
#define POKEMON_STATUS_IS_SHADOW             0xC8
#define POKEMON_STATUS_SHADOW_CONDITION_SLOT 0xC9

/* The battle Pokemon (stays with the Pokemon for the whole battle) */
#define POKEMON_STATUS_PARTY_RECORD_PTR      0xCB  /* MotoPokemonPtr: the party record it was copied from */
#define POKEMON_STATUS_RECORD_COPY_PTR       0xCC  /* PokemonBuffPtr: the battle's copy of the record */
#define POKEMON_STATUS_BATTLE_CONDITION_SLOT 0xCD  /* FightJoutaiPtr */
#define POKEMON_STATUS_ENTRY_ID              0xCE
#define POKEMON_STATUS_LEVEL_UP_FLAG         0xD0
#define POKEMON_STATUS_CAUGHT_IN_BATTLE_FLAG 0xD2  /* HokakuFlag */

/* The Pokemon on the field (battle) */
#define POKEMON_STATUS_ORIGINAL_BATTLE_POKEMON_PTR 0xD5 /* MotoFightPokemonPtr */
#define POKEMON_STATUS_BATTLE_POKEMON_PTR    0xD6  /* FightPokemonPtr */
#define POKEMON_STATUS_TRANSFORM_COPY_PTR    0xD7  /* FightPokemonHensinBuffPtr: Transform's copy */
#define POKEMON_STATUS_FIELD_CONDITION_SLOT  0xD8  /* FightoutJoutaiPtr */
#define POKEMON_STATUS_BATTLE_MOVE_PTR       0xD9  /* FightWazaPtr: the move being used */
#define POKEMON_STATUS_MOVE_IN_USE           0xDA
#define POKEMON_STATUS_BATTLE_ITEM_PTR       0xE5  /* FightItemPtr */
#define POKEMON_STATUS_STAGE_ATTACK          0xE6  /* stat stages: 6 is neutral */
#define POKEMON_STATUS_STAGE_DEFENSE         0xE7
#define POKEMON_STATUS_STAGE_SP_ATTACK       0xE8
#define POKEMON_STATUS_STAGE_SP_DEFENSE      0xE9
#define POKEMON_STATUS_STAGE_SPEED           0xEA
#define POKEMON_STATUS_STAGE_ACCURACY        0xEB
#define POKEMON_STATUS_STAGE_EVASION         0xEC
#define POKEMON_STATUS_TURNS_ON_FIELD        0xED
#define POKEMON_STATUS_SEQUENCE_PTR          0xEE  /* SequencePtr */
#define POKEMON_STATUS_SKETCH_MOVE           0xEF
#define POKEMON_STATUS_LAST_SELECTED_MOVE    0xF0
#define POKEMON_STATUS_LAST_USED_MOVE        0xF1
#define POKEMON_STATUS_LAST_HIT_BY_MOVE      0xF3
#define POKEMON_STATUS_LAST_HIT_BY_MOVE_TYPE 0xF4
#define POKEMON_STATUS_BIDE_DAMAGE           0xF5  /* Gaman */
#define POKEMON_STATUS_BIDE_TARGET           0xF6
#define POKEMON_STATUS_MIRROR_MOVE           0xF7  /* Oumu: the move Mirror Move copies */
#define POKEMON_STATUS_TRUANT_FLAG           0xF9  /* Namake */
#define POKEMON_STATUS_USED_ITEM             0xFA
#define POKEMON_STATUS_PROTECT_COUNT         0xFC  /* SuccessCnt */
#define POKEMON_STATUS_ACTION_PTR            0xFE  /* FightActionBuffPtr: the battler's chosen action */
#define POKEMON_STATUS_BATTLE_TYPE           0xFF  /* index: first or second type */
#define POKEMON_STATUS_BATTLE_ABILITY        0x100
#define POKEMON_STATUS_PHYSICAL_DAMAGE_TAKEN 0x102 /* DamageAtk: for Counter */
#define POKEMON_STATUS_PHYSICAL_DAMAGE_FROM  0x103
#define POKEMON_STATUS_SPECIAL_DAMAGE_TAKEN  0x104 /* DamageSpe: for Mirror Coat */
#define POKEMON_STATUS_SPECIAL_DAMAGE_FROM   0x105
#define POKEMON_STATUS_FULLY_PARALYZED_FLAG  0x106 /* MahiNoAttack */
#define POKEMON_STATUS_HURT_ITSELF_FLAG      0x107 /* KonranMyselfAttack: hit itself in confusion */
#define POKEMON_STATUS_CHARGING_MOVE_FLAG    0x109 /* TameWaza */
#define POKEMON_STATUS_IMPRISON_BLOCKED_FLAG 0x10B /* HuuinNoAttack */
#define POKEMON_STATUS_INFATUATED_FLAG       0x10C /* MeroMeroNoAttack */
#define POKEMON_STATUS_DISABLE_BLOCKED_FLAG  0x10D /* KanashibariNoAttack */
#define POKEMON_STATUS_TAUNT_BLOCKED_FLAG    0x10E /* ChouhatsuNoAttack */
#define POKEMON_STATUS_TORMENT_BLOCKED_FLAG  0x10F /* IchamonNoAttack */
#define POKEMON_STATUS_FLINCHED_FLAG         0x110 /* HirumuNoAttack */
#define POKEMON_STATUS_CLEAR_BODY_FLAG       0x113 /* DoClearbody */
#define POKEMON_STATUS_LIGHTNINGROD_FLAG     0x114 /* ReceivesWazaHiraishin: drew the move in */
#define POKEMON_STATUS_INTIMIDATE_DONE_FLAG  0x116 /* DoIkaku */
#define POKEMON_STATUS_TRACE_DONE_FLAG       0x117 /* DoTrace */
#define POKEMON_STATUS_SWITCHED_FLAG         0x119 /* Irekaeta */
#define POKEMON_STATUS_FOCUS_BAND_FLAG       0x11A /* ItemKoraeta: held on with an item */
#define POKEMON_STATUS_SHELL_BELL_DAMAGE     0x11B /* KaigaraDamageValue */
#define POKEMON_STATUS_FAINTED_FLAG          0x120 /* Kizetu */
#define POKEMON_STATUS_SWITCH_TARGET         0x121 /* IrekaeTargetEntryId */

#endif /* GAME_CONSTANTS_POKEMON_STATUS_H */
