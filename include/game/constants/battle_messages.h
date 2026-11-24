#ifndef GAME_CONSTANTS_BATTLE_MESSAGES_H
#define GAME_CONSTANTS_BATTLE_MESSAGES_H

/*
 * Battle messages: the message keys the battle code opens by number (fightMenuOpenMsg, fightMenuOpenTrainerMsg,
 * winMsgOpenFight, fn_802624CC, the effectiveness and Call branches). Each name was read off the game's own text:
 * every key below was seen in the battle text box with its finished wording during the event-log replays of the reference playthrough's
 * 17 sittings (2026-11-10, port/src/pc_event_log.c's [btext] lines; verified). The comment gives the wording with its
 * placeholders described, not a copy of any one line. Most battle messages come from the battle data (the move
 * scripts), not from C; only the ones the code names directly are here.
 * Added 2026-11-10 (named constants, snatcherLeo's plan: "a batch renaming for everything we do know").
 */

/* the battle's start and end */
#define BATTLE_MSG_TRAINER_WANTS_TO_BATTLE  0x766D  /* "<class> <name> would like to battle!" */
#define BATTLE_MSG_TRAINER_SENT_OUT_ONE     0x7670  /* "<trainer> sent out <Pokemon>!" */
#define BATTLE_MSG_TRAINER_SENT_OUT_TWO     0x7671  /* "<trainer> sent out <Pokemon> and <Pokemon>!" */
#define BATTLE_MSG_PLAYER_SENT_OUT_ONE      0x7678  /* "Go! <Pokemon>!" */
#define BATTLE_MSG_PLAYER_SENT_OUT_TWO      0x7679  /* "Go! <Pokemon> and <Pokemon>!" */
#define BATTLE_MSG_PLAYER_DEFEATED_TRAINER  0x766C  /* "Player defeated <class> <name>!" (the word "Player" as the
                                                     * replays showed it; a Dolphin check is pending) */
#define BATTLE_MSG_OUT_OF_POKEMON           0x7547  /* "<trainer> is out of usable POKEMON!" */
#define BATTLE_MSG_WHITED_OUT               0x7548  /* "<trainer> whited out!" */
#define BATTLE_MSG_GAINED_EXP               0x7532  /* "<Pokemon> gained <n> EXP. Points!" */

/* moves */
#define BATTLE_MSG_USED_MOVE                0x768D  /* "<Pokemon> used <move>!" */
#define BATTLE_MSG_CRITICAL_HIT             0x7631  /* "A critical hit!" */
#define BATTLE_MSG_SUPER_EFFECTIVE          0x7637  /* "It's super effective!" */
#define BATTLE_MSG_NOT_VERY_EFFECTIVE       0x7636  /* "It's not very effective..." */
#define BATTLE_MSG_NO_EFFECT                0x7543  /* "It doesn't affect <Pokemon>..." */
#define BATTLE_MSG_BUT_IT_FAILED            0x7647  /* "But it failed!" */
#define BATTLE_MSG_NO_PP_LEFT               0x7661  /* "There's no PP left for this move!" */
#define BATTLE_MSG_TAUNT_BLOCKS_MOVE        0x75FF  /* "<Pokemon> can't use <move> after the TAUNT!" */
#define BATTLE_MSG_SANDSTORM_RAGES          0x7650  /* "The sandstorm rages." */

/* Shadow Pokemon: Hyper Mode, the Call, the snag */
#define BATTLE_MSG_ENTERED_HYPER_MODE       0x771B  /* "<Pokemon>'s emotions rose to a fever pitch! It entered HYPER
                                                     * MODE!" */
#define BATTLE_MSG_IS_IN_HYPER_MODE         0x771C  /* "<Pokemon> is in HYPER MODE!" */
#define BATTLE_MSG_CALLED_POKEMON           0x7704  /* "<player> called <Pokemon>!" */
#define BATTLE_MSG_CALL_NAME                0x7705  /* "<Pokemon>!" (the call itself) */
#define BATTLE_MSG_CALL_SNAPPED_OUT         0x7707  /* "<Pokemon> came to its senses from the trainer's call!" */
#define BATTLE_MSG_CALL_NOT_LISTENING       0x7709  /* "It's no good! It's not listening!" */
#define BATTLE_MSG_CALL_FAILED_TO_HEAR      0x771D  /* "<Pokemon> failed to hear!" */
#define BATTLE_MSG_SNAG_CAUGHT              0x771F  /* "Gotcha! <Pokemon> was caught!" */

/* the partner's lines about Shadow Pokemon */
#define BATTLE_MSG_PARTNER_SHADOW_SPOTTED   0x770A  /* the partner: "Oh! There's a SHADOW POKEMON!" */
#define BATTLE_MSG_PARTNER_BLACK_AURA       0x7717  /* the partner points out a Pokemon with a black aura */
#define BATTLE_MSG_PARTNER_NO_BLACK_AURA    0x7716  /* the partner: that Pokemon has no black aura after all */
#define BATTLE_MSG_PARTNER_FIRST_SHADOW     0x7729  /* the partner recognises the first Shadow Pokemon (why she was
                                                     * captured) */
#define BATTLE_MSG_PARTNER_FIRST_SNAG_URGE  0x772A  /* the partner urges the player to snag it ("only you can do it") */

#endif /* GAME_CONSTANTS_BATTLE_MESSAGES_H */
