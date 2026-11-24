#include "dolphin/types.h"
#ifdef TARGET_PC
#include "pc/orre_hooks.h" /* OrreHook_PartyListHold: the party list held open between bag uses (step 3.7) */
#endif

extern u32 lbl_8047A2EC;
extern void menuPokemonClose(void);

void itemUseClosePartyMenu(u32 value)
{
    lbl_8047A2EC = value;
#ifdef TARGET_PC
    /* OrreHook_PartyListHold (step 3.7, snatcherLeo's hand check 2026-11-20: "it just remains on the party menu"; pc/orre_hooks.h):
     * every item use function closes the party list here, also between two uses of the same item from the bag, so the
     * list slid apart and opened again. While the bag's loop runs, OrrePort may keep it open (a Pokémon was picked); the
     * next use then only resumes it, and the bag closes it after the loop. NULL or 0: closed here, as on the GameCube.
     * All platforms. The same lines are in gs_event_exec.c's copy of this function. */
    if (ORRE_PARTY_LIST_HOLD(ORRE_HOLD_CLOSE, value)) {
        return;
    }
#endif
    menuPokemonClose();
}
