#ifndef GAME_CONSTANTS_ROOMS_H
#define GAME_CONSTANTS_ROOMS_H

/*
 * Room numbers (the "floor" / room id the room table and the room-change code use; 196 rooms). Names: each room's
 * archive name on the disc (the developers' own file names, e.g. M1_out for Phenac City's outdoors); the comment
 * gives the place name. Archive prefixes: S Outskirt Stand / Snagem hideout, M towns, D story locations, T1 the
 * Colosseum arena; _BF is a room's battle-field copy (map and collision, no NPCs), the stage an overworld battle
 * there is drawn in (battle-arenas doc); 800-999 are menus, logos and demos.
 * Added 2026-11-09 (named constants, snatcherLeo's plan of 2026-11-08).
 */

#define ROOM_NONE                      0
#define ROOM_S1_OUT                    1 /* Outskirt Stand */
#define ROOM_S1_SHOP_1F                2 /* Outskirt Stand */
#define ROOM_M1_OUT                    3 /* Phenac City */
#define ROOM_M1_PC_1F                  4 /* Phenac City */
#define ROOM_M1_HOUSEA_1F              5 /* Phenac City */
#define ROOM_M1_HOUSEA_2F              6 /* Phenac City */
#define ROOM_M1_HOUSEB_1F              7 /* Phenac City */
#define ROOM_M1_HOUSEC_1F              8 /* Phenac City */
#define ROOM_M1_SHOP_1F                9 /* Phenac City */
#define ROOM_M1_SHOP_2F               10 /* Phenac City */
#define ROOM_M1_GYM_1F                11 /* Phenac City */
#define ROOM_M1_GYM_B1                12 /* Phenac City */
#define ROOM_M1_STADIUM_1F            13 /* Phenac City */
#define ROOM_M1_WATER_COLO            14 /* Phenac City */
#define ROOM_M2_OUT                   15 /* Pyrite Town */
#define ROOM_M2_POLICE_1F             16 /* Pyrite Town */
#define ROOM_M2_HOTEL_1F              17 /* Pyrite Town */
#define ROOM_M2_URANAI_1F             18 /* Pyrite Town */
#define ROOM_M2_HOUSEA_1F             19 /* Pyrite Town */
#define ROOM_M2_GUILD_1F_1            20 /* Pyrite Town */
#define ROOM_M2_GUILD_1F_2            21 /* Pyrite Town */
#define ROOM_M2_SHOP_1F               22 /* Pyrite Town */
#define ROOM_M2_WINDMILL_1F           23 /* Pyrite Town */
#define ROOM_M2_WINDMILL_2F           24 /* Pyrite Town */
#define ROOM_M2_BUILDING_1F           25 /* Pyrite Town */
#define ROOM_M2_BUILDING_2F           26 /* Pyrite Town */
#define ROOM_M2_BUILDING_3F           27 /* Pyrite Town */
#define ROOM_M2_BUILDING_4F           28 /* Pyrite Town */
#define ROOM_M2_CAVE_1F_1             29 /* Pyrite Town */
#define ROOM_M2_CAVE_1F_2             30 /* Pyrite Town */
#define ROOM_M2_CAVE_2F_1             31 /* Pyrite Town */
#define ROOM_M2_CAVE_2F_2             32 /* Pyrite Town */
#define ROOM_M2_CAVE_3F_1             33 /* Pyrite Town */
#define ROOM_M2_CAVE_3F_2             34 /* Pyrite Town */
#define ROOM_M2_EARTH_COLO            35 /* Pyrite Town */
#define ROOM_M3_OUT                   36 /* Agate Village */
#define ROOM_M3_CAVE_1F_1             37 /* Agate Village */
#define ROOM_M3_CAVE_1F_2             38 /* Agate Village */
#define ROOM_M3_SHRINE_1F             39 /* Agate Village */
#define ROOM_M3_HOUSEA_1F             40 /* Agate Village */
#define ROOM_M3_HOUSEB_1F             41 /* Agate Village */
#define ROOM_M3_PC_1F                 42 /* Agate Village */
#define ROOM_M3_SHOP_1F               43 /* Agate Village */
#define ROOM_M3_HOUSEC_1F             44 /* Agate Village */
#define ROOM_M3_HOUSEC_2F             45 /* Agate Village */
#define ROOM_M3_HOUSED_1F             46 /* Agate Village */
#define ROOM_M4_OUT                   47 /* The Under */
#define ROOM_M4_JUNK_1F               48 /* The Under */
#define ROOM_M4_JUNK_B1               49 /* The Under */
#define ROOM_M4_HOTEL_1F              50 /* The Under */
#define ROOM_M4_HOTEL_B1              51 /* The Under */
#define ROOM_M4_SHOP_1F               52 /* The Under */
#define ROOM_M4_LABO_1F               53 /* The Under */
#define ROOM_M4_LABO_B1               54 /* The Under */
#define ROOM_M4_HOUSEA_1F             55 /* The Under */
#define ROOM_M4_SUBWAY_B1             57 /* The Under */
#define ROOM_M4_SUBWAY_B2_1           58 /* The Under */
#define ROOM_M4_SUBWAY_B2_2           59 /* The Under */
#define ROOM_M4_SUBWAY_B3             60 /* The Under */
#define ROOM_M4_TRAIN_1F              61 /* The Under */
#define ROOM_M4_BOTTOM_1F             62 /* The Under */
#define ROOM_M4_CYLINDER_COLO         63 /* The Under */
#define ROOM_M4_BOTTOM_COLO           64 /* The Under */
#define ROOM_D1_OUT                   65 /* Shadow Pokemon Lab */
/* ENHANCEMENT NOTE (snatcherLeo, 2026-11-09): in this room (the lab's first room), once the door to the right of where Wes
 * stands is opened, that doorway flickers and glitches as Wes moves around. The original game does it too (seen in
 * Dolphin), so it is not a port bug. Planned: a look-only fix, built in. The cause is not found yet (likely the room's
 * model or the door's animation data rather than code); when it is, the fix's hook goes where the cause is and this
 * note points there. */
#define ROOM_D1_LABO_1F               66 /* Shadow Pokemon Lab */
#define ROOM_D1_LABO_B1               67 /* Shadow Pokemon Lab */
#define ROOM_D1_LABO_B2               68 /* Shadow Pokemon Lab */
#define ROOM_D1_LABO_B3               69 /* Shadow Pokemon Lab */
#define ROOM_D1_GARAGE_1F             70 /* Shadow Pokemon Lab */
#define ROOM_D1_GARAGE_B1             71 /* Shadow Pokemon Lab */
#define ROOM_D1_GARAGE_B2_1           72 /* Shadow Pokemon Lab */
#define ROOM_D1_GARAGE_B2_2           73 /* Shadow Pokemon Lab */
#define ROOM_D2_OUT                   74 /* Mt Battle */
#define ROOM_D2_PC_1F                 75 /* Mt Battle */
#define ROOM_D2_VALLEY_1              76 /* Mt Battle */
#define ROOM_D2_REST_1                77 /* Mt Battle */
#define ROOM_D2_VALLEY_2              78 /* Mt Battle */
#define ROOM_D2_REST_2                79 /* Mt Battle */
#define ROOM_D2_VALLEY_3              80 /* Mt Battle */
#define ROOM_D2_REST_3                81 /* Mt Battle */
#define ROOM_D2_MAGMA_1               82 /* Mt Battle */
#define ROOM_D2_REST_4                83 /* Mt Battle */
#define ROOM_D2_MAGMA_2               84 /* Mt Battle */
#define ROOM_D2_REST_5                85 /* Mt Battle */
#define ROOM_D2_MAGMA_3               86 /* Mt Battle */
#define ROOM_D2_REST_6                87 /* Mt Battle */
#define ROOM_D2_CLOUD_1               88 /* Mt Battle */
#define ROOM_D2_REST_7                89 /* Mt Battle */
#define ROOM_D2_CLOUD_2               90 /* Mt Battle */
#define ROOM_D2_REST_8                91 /* Mt Battle */
#define ROOM_D2_CLOUD_3               92 /* Mt Battle */
#define ROOM_D2_REST_9                93 /* Mt Battle */
#define ROOM_D2_CLOUD_4               94 /* Mt Battle */
#define ROOM_D2_CRATER_COLO           95 /* Mt Battle */
#define ROOM_D4_OUT                  102 /* Realgam Tower */
#define ROOM_D4_DOME_1               103 /* Realgam Tower */
#define ROOM_D4_DOME_2               104 /* Realgam Tower */
#define ROOM_D4_DOME_3               105 /* Realgam Tower */
#define ROOM_D4_DOME_4               106 /* Realgam Tower */
#define ROOM_D4_DOME_5               107 /* Realgam Tower */
#define ROOM_D4_DOME_6               108 /* Realgam Tower */
#define ROOM_D4_DOME_7               109 /* Realgam Tower */
#define ROOM_D4_DOME_8               110 /* Realgam Tower */
#define ROOM_D4_DOME_9               111 /* Realgam Tower */
#define ROOM_D4_DOME_10              112 /* Realgam Tower */
#define ROOM_D4_DOME_11              113 /* Realgam Tower */
#define ROOM_D4_TOWER_1F_1           114 /* Realgam Tower */
#define ROOM_D4_TOWER_1F_2           115 /* Realgam Tower */
#define ROOM_D4_TOWER_1F_3           116 /* Realgam Tower */
#define ROOM_D4_TOWER_2F             117 /* Realgam Tower */
#define ROOM_D4_CASINO_COLO          118 /* Realgam Tower */
#define ROOM_M1_PC_B1                119 /* Phenac City */
#define ROOM_M2_ENTER_1F             120 /* Pyrite Town */
#define ROOM_M4_ENTER_1F             121 /* The Under */
#define ROOM_M4_OUT_2                122 /* The Under */
#define ROOM_D4_OUT_2                123 /* Realgam Tower */
#define ROOM_D4_TOWER_2F_2           124 /* Realgam Tower */
#define ROOM_M4_ENTER_B1             125 /* The Under */
#define ROOM_T1_ANCIENT_COLO         126 /* Colosseum Arena T1 */
#define ROOM_M4_KANPO_1F             127 /* The Under */
#define ROOM_M1_CARDE_1              128 /* Phenac City */
#define ROOM_S2_HALLWAY_1            129 /* Team Snagem Hideout */
#define ROOM_S2_SNATCHROOM_1         130 /* Team Snagem Hideout */
#define ROOM_S2_BOSSROOM_1           131 /* Team Snagem Hideout */
#define ROOM_S2_BUILDING_1F_2        132 /* Team Snagem Hideout */
#define ROOM_S2_BUILDING_2F_2        133 /* Team Snagem Hideout */
#define ROOM_S2_BUILDING_3F_2        134 /* Team Snagem Hideout */
#define ROOM_S2_OUT_1                135 /* Team Snagem Hideout */
#define ROOM_S2_OUT_2                136 /* Team Snagem Hideout */
#define ROOM_S2_OUT_3                137 /* Team Snagem Hideout */
#define ROOM_M4_SUBWAY_B3_EV_1       138 /* The Under */
#define ROOM_M4_SUBWAY_B3_EV_2       139 /* The Under */
#define ROOM_D1_GARAGE_B2_2_EV_1     140 /* Shadow Pokemon Lab */
#define ROOM_D1_GARAGE_B2_2_EV_2     141 /* Shadow Pokemon Lab */
#define ROOM_M2_ENTER_1F_2           142 /* Pyrite Town */
#define ROOM_D2_PRESENT_ROOM         143 /* Mt Battle */
#define ROOM_S1_OUT_BF               200 /* Outskirt Stand */
#define ROOM_S2_BUILDING_1F_2_BF     201 /* Team Snagem Hideout */
#define ROOM_M1_OUT_BF               202 /* Phenac City */
#define ROOM_M1_GYM_1F_BF            203 /* Phenac City */
#define ROOM_M1_HOUSEA_1F_BF         204 /* Phenac City */
#define ROOM_M2_OUT_BF               205 /* Pyrite Town */
#define ROOM_M2_CAVE_3F_1_BF         206 /* Pyrite Town */
#define ROOM_M2_BUILDING_1F_BF       207 /* Pyrite Town */
#define ROOM_M3_OUT_BF               208 /* Agate Village */
#define ROOM_M3_SHRINE_1F_BF         209 /* Agate Village */
#define ROOM_M3_CAVE_1F_1_BF         210 /* Agate Village */
#define ROOM_M4_OUT_BF               211 /* The Under */
#define ROOM_M4_HOUSEA_1F_BF         212 /* The Under */
#define ROOM_M4_SUBWAY_B2_1_BF       213 /* The Under */
#define ROOM_D1_OUT_BF               214 /* Shadow Pokemon Lab */
#define ROOM_D1_LABO_B1_BF           215 /* Shadow Pokemon Lab */
#define ROOM_D2_OUT_BF               216 /* Mt Battle */
#define ROOM_D2_VALLEY_1_BF          217 /* Mt Battle */
#define ROOM_D2_CLOUD_1_BF           218 /* Mt Battle */
#define ROOM_D2_PC_1F_BF             219 /* Mt Battle */
#define ROOM_D2_MAGMA_1_BF           220 /* Mt Battle */
#define ROOM_D4_DOME_2_BF            221 /* Realgam Tower */
#define ROOM_D4_TOWER_1F_1_BF        222 /* Realgam Tower */
#define ROOM_D4_TOWER_1F_2_BF        223 /* Realgam Tower */
#define ROOM_D4_TOWER_1F_3_BF        224 /* Realgam Tower */
#define ROOM_D4_TOWER_2F_BF          225 /* Realgam Tower */
#define ROOM_D4_TOWER_2F_2_BF        226 /* Realgam Tower */
#define ROOM_D4_CASINO_COLO_BF       227 /* Realgam Tower */
#define ROOM_D2_CRATER_COLO_BF       228 /* Mt Battle */
#define ROOM_EX_TITLE                800
#define ROOM_EX_TOP                  801
#define ROOM_EX_COUPON               802
#define ROOM_EX_SHRINE               803
#define ROOM_TITLE                   900
#define ROOM_M3_SHRINE_1F_RL         901 /* Agate Village */
#define ROOM_EVOLUTION               902
#define ROOM_DNA_KEY                 903
#define ROOM_WORLD_MAP               910
#define ROOM_POCKET_MENU             911
#define ROOM_NAME_ENTRY_MENU         912
#define ROOM_PCBOX_MENU              913
#define ROOM_PDA_MENU                914
#define ROOM_CARDE_MENU              915
#define ROOM_POKEMONCHANGE_MENU      916
#define ROOM_COLOSSEUMBATTLE_MENU    917
#define ROOM_COLOSSEUMBATTLECONNECTION_MENU  918
#define ROOM_TOOLBATTLE_MENU         919
#define ROOM_NINTENDO_LOGO           920
#define ROOM_POKEMON_LOGO            921
#define ROOM_GENIUS_LOGO             922
#define ROOM_FIGHT_DEMO              923
#define ROOM_AUTO_DEMO               924
#define ROOM_WAZA_MENU               925
#define ROOM_POKECOUPON_MENU         926
#define ROOM_STAFFROLL               927
#define ROOM_OPENING_DEMO            928
#define ROOM_TOPMENU                 929
#define ROOM_KEYDISC                 996
#define ROOM_WAZA_VIEWER             999

#endif /* GAME_CONSTANTS_ROOMS_H */
