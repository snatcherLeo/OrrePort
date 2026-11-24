#ifndef GAME_CONSTANTS_MOVE_EFFECTS_H
#define GAME_CONSTANTS_MOVE_EFFECTS_H

/*
 * Move effects: the effect number each move carries in the move table (wazaGetStatus(0, move, 9, 0), the move
 * record's effect id; the battle engine runs that effect's script and branches on the number). The same numbering as
 * Gen 3's games (verified 2026-11-10: every effect the the reference playthrough event log saw, 128 of them, belongs to the moves Gen 3
 * gives it, and the battle text matched). Each name comes from the moves the disc's move table gives that number and
 * the game's own move descriptions (decoded US table; the 198 numbers in use; 12, 14, 15, 21, 22, 55, 56, 61, 63, 64,
 * 74, 96, 110, 131, 141 and 163 are used by no move). Effects with one move are named after it. "CHANCE" = the
 * secondary effect happens with the move's own effect chance (move record, effect_chance).
 * Added 2026-11-10 (named constants, snatcherLeo's plan: "a batch renaming for everything we do know").
 */

#define MOVE_EFFECT_HIT                      0    /* plain damage */
#define MOVE_EFFECT_SLEEP                    1
#define MOVE_EFFECT_POISON_CHANCE            2
#define MOVE_EFFECT_DRAIN_HALF               3    /* Absorb, Giga Drain: heals half the damage dealt */
#define MOVE_EFFECT_BURN_CHANCE              4
#define MOVE_EFFECT_FREEZE_CHANCE            5
#define MOVE_EFFECT_PARALYZE_CHANCE          6
#define MOVE_EFFECT_USER_FAINTS              7    /* Selfdestruct, Explosion */
#define MOVE_EFFECT_DREAM_EATER              8
#define MOVE_EFFECT_MIRROR_MOVE              9
#define MOVE_EFFECT_ATTACK_UP                10
#define MOVE_EFFECT_DEFENSE_UP               11
#define MOVE_EFFECT_SP_ATTACK_UP             13   /* Growth */
#define MOVE_EFFECT_EVASION_UP               16
#define MOVE_EFFECT_NEVER_MISSES             17   /* Swift, Aerial Ace */
#define MOVE_EFFECT_ATTACK_DOWN              18
#define MOVE_EFFECT_DEFENSE_DOWN             19
#define MOVE_EFFECT_SPEED_DOWN               20
#define MOVE_EFFECT_ACCURACY_DOWN            23
#define MOVE_EFFECT_EVASION_DOWN             24
#define MOVE_EFFECT_HAZE                     25
#define MOVE_EFFECT_BIDE                     26
#define MOVE_EFFECT_RAMPAGE                  27   /* Thrash, Outrage: 2-3 turns, then confused */
#define MOVE_EFFECT_FORCE_SWITCH             28   /* Roar, Whirlwind */
#define MOVE_EFFECT_MULTI_HIT                29   /* 2 to 5 hits */
#define MOVE_EFFECT_CONVERSION               30
#define MOVE_EFFECT_FLINCH_CHANCE            31
#define MOVE_EFFECT_RECOVER_HALF             32   /* Recover, Slack Off */
#define MOVE_EFFECT_TOXIC                    33
#define MOVE_EFFECT_PAY_DAY                  34
#define MOVE_EFFECT_LIGHT_SCREEN             35
#define MOVE_EFFECT_TRI_ATTACK               36
#define MOVE_EFFECT_REST                     37
#define MOVE_EFFECT_ONE_HIT_KO               38   /* Guillotine, Fissure, Sheer Cold */
#define MOVE_EFFECT_RAZOR_WIND               39   /* two turns: charge, then strike */
#define MOVE_EFFECT_SUPER_FANG               40   /* halves the target's HP */
#define MOVE_EFFECT_DRAGON_RAGE              41   /* always 40 HP */
#define MOVE_EFFECT_TRAP                     42   /* Wrap, Fire Spin, Sand Tomb: 2 to 5 turns */
#define MOVE_EFFECT_HIGH_CRITICAL            43   /* Slash, Crabhammer, Razor Leaf */
#define MOVE_EFFECT_DOUBLE_HIT               44   /* Double Kick, Bonemerang */
#define MOVE_EFFECT_RECOIL_IF_MISS           45   /* Jump Kick, Hi Jump Kick */
#define MOVE_EFFECT_MIST                     46
#define MOVE_EFFECT_FOCUS_ENERGY             47
#define MOVE_EFFECT_RECOIL                   48   /* Take Down, Submission, Struggle, Shadow Rush */
#define MOVE_EFFECT_CONFUSE                  49
#define MOVE_EFFECT_ATTACK_UP_2              50   /* "sharply" */
#define MOVE_EFFECT_DEFENSE_UP_2             51
#define MOVE_EFFECT_SPEED_UP_2               52
#define MOVE_EFFECT_SP_ATTACK_UP_2           53
#define MOVE_EFFECT_SP_DEFENSE_UP_2          54
#define MOVE_EFFECT_TRANSFORM                57
#define MOVE_EFFECT_ATTACK_DOWN_2            58
#define MOVE_EFFECT_DEFENSE_DOWN_2           59
#define MOVE_EFFECT_SPEED_DOWN_2             60
#define MOVE_EFFECT_SP_DEFENSE_DOWN_2        62
#define MOVE_EFFECT_REFLECT                  65
#define MOVE_EFFECT_POISON                   66
#define MOVE_EFFECT_PARALYZE                 67
#define MOVE_EFFECT_ATTACK_DOWN_CHANCE       68
#define MOVE_EFFECT_DEFENSE_DOWN_CHANCE      69
#define MOVE_EFFECT_SPEED_DOWN_CHANCE        70
#define MOVE_EFFECT_SP_ATTACK_DOWN_CHANCE    71
#define MOVE_EFFECT_SP_DEFENSE_DOWN_CHANCE   72
#define MOVE_EFFECT_ACCURACY_DOWN_CHANCE     73
#define MOVE_EFFECT_SKY_ATTACK               75   /* two turns; flinch chance */
#define MOVE_EFFECT_CONFUSE_CHANCE           76
#define MOVE_EFFECT_TWINEEDLE                77
#define MOVE_EFFECT_VITAL_THROW              78
#define MOVE_EFFECT_SUBSTITUTE               79
#define MOVE_EFFECT_RECHARGE                 80   /* Hyper Beam: the user rests the next turn */
#define MOVE_EFFECT_RAGE                     81
#define MOVE_EFFECT_MIMIC                    82
#define MOVE_EFFECT_METRONOME                83
#define MOVE_EFFECT_LEECH_SEED               84
#define MOVE_EFFECT_SPLASH                   85
#define MOVE_EFFECT_DISABLE                  86
#define MOVE_EFFECT_LEVEL_DAMAGE             87   /* Seismic Toss, Night Shade */
#define MOVE_EFFECT_PSYWAVE                  88
#define MOVE_EFFECT_COUNTER                  89
#define MOVE_EFFECT_ENCORE                   90
#define MOVE_EFFECT_PAIN_SPLIT               91
#define MOVE_EFFECT_SNORE                    92
#define MOVE_EFFECT_CONVERSION_2             93
#define MOVE_EFFECT_LOCK_ON                  94   /* Lock-On, Mind Reader */
#define MOVE_EFFECT_SKETCH                   95
#define MOVE_EFFECT_SLEEP_TALK               97
#define MOVE_EFFECT_DESTINY_BOND             98
#define MOVE_EFFECT_FLAIL                    99   /* Flail, Reversal: stronger at low HP */
#define MOVE_EFFECT_SPITE                    100
#define MOVE_EFFECT_FALSE_SWIPE              101
#define MOVE_EFFECT_HEAL_BELL                102  /* Heal Bell, Aromatherapy */
#define MOVE_EFFECT_QUICK_ATTACK             103  /* Quick Attack, Mach Punch, ExtremeSpeed */
#define MOVE_EFFECT_TRIPLE_KICK              104
#define MOVE_EFFECT_STEAL_ITEM               105  /* Thief, Covet */
#define MOVE_EFFECT_PREVENT_ESCAPE           106  /* Mean Look, Spider Web, Block */
#define MOVE_EFFECT_NIGHTMARE                107
#define MOVE_EFFECT_MINIMIZE                 108
#define MOVE_EFFECT_CURSE                    109
#define MOVE_EFFECT_PROTECT                  111  /* Protect, Detect */
#define MOVE_EFFECT_SPIKES                   112
#define MOVE_EFFECT_FORESIGHT                113  /* Foresight, Odor Sleuth */
#define MOVE_EFFECT_PERISH_SONG              114
#define MOVE_EFFECT_SANDSTORM                115
#define MOVE_EFFECT_ENDURE                   116
#define MOVE_EFFECT_ROLLOUT                  117  /* Rollout, Ice Ball */
#define MOVE_EFFECT_SWAGGER                  118
#define MOVE_EFFECT_FURY_CUTTER              119
#define MOVE_EFFECT_ATTRACT                  120
#define MOVE_EFFECT_RETURN                   121
#define MOVE_EFFECT_PRESENT                  122
#define MOVE_EFFECT_FRUSTRATION              123
#define MOVE_EFFECT_SAFEGUARD                124
#define MOVE_EFFECT_THAW_BURN_CHANCE         125  /* Flame Wheel, Sacred Fire */
#define MOVE_EFFECT_MAGNITUDE                126
#define MOVE_EFFECT_BATON_PASS               127
#define MOVE_EFFECT_PURSUIT                  128
#define MOVE_EFFECT_RAPID_SPIN               129
#define MOVE_EFFECT_SONICBOOM                130  /* always 20 HP */
#define MOVE_EFFECT_MORNING_SUN              132
#define MOVE_EFFECT_SYNTHESIS                133
#define MOVE_EFFECT_MOONLIGHT                134
#define MOVE_EFFECT_HIDDEN_POWER             135
#define MOVE_EFFECT_RAIN_DANCE               136
#define MOVE_EFFECT_SUNNY_DAY                137
#define MOVE_EFFECT_DEFENSE_UP_CHANCE        138  /* Steel Wing */
#define MOVE_EFFECT_ATTACK_UP_CHANCE         139  /* Metal Claw, Meteor Mash */
#define MOVE_EFFECT_ALL_STATS_UP_CHANCE      140  /* AncientPower, Silver Wind */
#define MOVE_EFFECT_BELLY_DRUM               142
#define MOVE_EFFECT_PSYCH_UP                 143
#define MOVE_EFFECT_MIRROR_COAT              144
#define MOVE_EFFECT_SKULL_BASH               145  /* two turns */
#define MOVE_EFFECT_TWISTER                  146
#define MOVE_EFFECT_EARTHQUAKE               147
#define MOVE_EFFECT_FUTURE_SIGHT             148  /* Future Sight, Doom Desire */
#define MOVE_EFFECT_GUST                     149
#define MOVE_EFFECT_STOMP                    150  /* flinch chance (Stomp, Astonish, Needle Arm, Extrasensory) */
#define MOVE_EFFECT_SOLARBEAM                151  /* two turns */
#define MOVE_EFFECT_THUNDER                  152
#define MOVE_EFFECT_TELEPORT                 153
#define MOVE_EFFECT_BEAT_UP                  154
#define MOVE_EFFECT_SEMI_INVULNERABLE        155  /* Fly, Dig, Dive, Bounce: two turns, out of reach on the first */
#define MOVE_EFFECT_DEFENSE_CURL             156
#define MOVE_EFFECT_SOFTBOILED               157  /* Softboiled, Milk Drink */
#define MOVE_EFFECT_FAKE_OUT                 158
#define MOVE_EFFECT_UPROAR                   159
#define MOVE_EFFECT_STOCKPILE                160
#define MOVE_EFFECT_SPIT_UP                  161
#define MOVE_EFFECT_SWALLOW                  162
#define MOVE_EFFECT_HAIL                     164
#define MOVE_EFFECT_TORMENT                  165
#define MOVE_EFFECT_FLATTER                  166
#define MOVE_EFFECT_WILL_O_WISP              167
#define MOVE_EFFECT_MEMENTO                  168
#define MOVE_EFFECT_FACADE                   169
#define MOVE_EFFECT_FOCUS_PUNCH              170
#define MOVE_EFFECT_SMELLINGSALT             171
#define MOVE_EFFECT_FOLLOW_ME                172
#define MOVE_EFFECT_NATURE_POWER             173
#define MOVE_EFFECT_CHARGE                   174
#define MOVE_EFFECT_TAUNT                    175
#define MOVE_EFFECT_HELPING_HAND             176
#define MOVE_EFFECT_TRICK                    177
#define MOVE_EFFECT_ROLE_PLAY                178
#define MOVE_EFFECT_WISH                     179
#define MOVE_EFFECT_ASSIST                   180
#define MOVE_EFFECT_INGRAIN                  181
#define MOVE_EFFECT_SUPERPOWER               182
#define MOVE_EFFECT_MAGIC_COAT               183
#define MOVE_EFFECT_RECYCLE                  184
#define MOVE_EFFECT_REVENGE                  185
#define MOVE_EFFECT_BRICK_BREAK              186
#define MOVE_EFFECT_YAWN                     187
#define MOVE_EFFECT_KNOCK_OFF                188
#define MOVE_EFFECT_ENDEAVOR                 189
#define MOVE_EFFECT_ERUPTION                 190  /* Eruption, Water Spout */
#define MOVE_EFFECT_SKILL_SWAP               191
#define MOVE_EFFECT_IMPRISON                 192
#define MOVE_EFFECT_REFRESH                  193
#define MOVE_EFFECT_GRUDGE                   194
#define MOVE_EFFECT_SNATCH                   195
#define MOVE_EFFECT_LOW_KICK                 196
#define MOVE_EFFECT_SECRET_POWER             197
#define MOVE_EFFECT_RECOIL_THIRD             198  /* Double-Edge, Volt Tackle (the "life-risking" recoil) */
#define MOVE_EFFECT_TEETER_DANCE             199
#define MOVE_EFFECT_BLAZE_KICK               200  /* high critical ratio, burn chance */
#define MOVE_EFFECT_MUD_SPORT                201
#define MOVE_EFFECT_POISON_FANG              202
#define MOVE_EFFECT_WEATHER_BALL             203
#define MOVE_EFFECT_OVERHEAT                 204  /* Overheat, Psycho Boost: the user's Sp. Atk sharply down */
#define MOVE_EFFECT_TICKLE                   205
#define MOVE_EFFECT_COSMIC_POWER             206
#define MOVE_EFFECT_SKY_UPPERCUT             207
#define MOVE_EFFECT_BULK_UP                  208
#define MOVE_EFFECT_POISON_TAIL              209  /* high critical ratio, poison chance */
#define MOVE_EFFECT_WATER_SPORT              210
#define MOVE_EFFECT_CALM_MIND                211
#define MOVE_EFFECT_DRAGON_DANCE             212
#define MOVE_EFFECT_CAMOUFLAGE               213

#endif /* GAME_CONSTANTS_MOVE_EFFECTS_H */
