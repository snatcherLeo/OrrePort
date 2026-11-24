#include "game/battle/battle_waza_types.h"

void wazaSequenceApplyStop(void* obj)
{
    WazaSequence* sequence;
    WazaSequenceOwner* owner;
    WazaSequenceNode* node;

    sequence = obj;
    if (sequence != NULL) {
        owner = P32(sequence->owner);
        if (sequence->active != 0) {
            if (sequence->cameraActive != 0) {
                battleCameraEndOwnerWaza(owner);
            }
            if (owner->animationActive == 0) {
                wazaSequenceOwnerPlayIdle(owner, 1, 0);
            }
            fn_800E3CC8(P32(owner->model), 0);
            if ((sequence->flags & 0x08000000) != 0) {
                GSmodelLinkToGSparticleBank(P32(owner->model), P32(owner->particleBank));
            }
            if ((sequence->flags & 0x04000000) != 0) {
                battleGridResetModelVisibilityFlags();
            }
            node = P32(sequence->firstNode);
            while (node != NULL) {
                wazaSequenceEntryStop(node, 1);
                node = P32(node->next);
            }
            node = P32(sequence->firstNode);
            while (node != NULL) {
                if (node->kind == 3 && node->state == 0 && P32(node->resource) != NULL) {
                    gsParticleKill(P32(node->resource), 1);
                }
                node = P32(node->next);
            }
            P32_SET(owner->currentSequence, NULL);
            sequence->active = 0;
            sequence->stopping = 0;
        }
    }
}

/* wazaSequenceReleaseOwner: if this is the owner's current sequence, return the owner to idle, end its move
 * camera and clear its current sequence; see wazaSequence.c. */
/* NAME-T3 fn_801DBC30 */
void wazaSequenceReleaseOwner(void* obj)
{
    WazaSequence* sequence;
    WazaSequenceOwner* owner;
    s32 kind;

    sequence = obj;
    if (sequence != NULL) {
        owner = P32(sequence->owner);
        if (sequence->active != 0 && P32(owner->currentSequence) == sequence) {
            kind = sequence->kind;
            if (kind >= 0xB || kind < 9) {
                wazaSequenceOwnerPlayIdle(owner, 1, 0);
            }
            if (sequence->cameraActive != 0) {
                battleCameraEndOwnerWaza(owner);
            }
            fn_800E3CC8(P32(owner->model), 0);
            P32_SET(owner->currentSequence, NULL);
        }
    }
}

void wazaSequenceStart(void* sequencePtr)
{
    WazaSequence* sequence;
    WazaSequenceOwner* owner;
    WazaSequenceNode* node;
    WazaSequence* current;
    s32 bit;
    struct GSmodel* model;
    u32 flags;

    sequence = sequencePtr;
    if (sequence->active == 0) {
        owner = P32(sequence->owner);
        flags = sequence->flags;
        current = P32(owner->currentSequence);
        flags = (flags >> 1) & 1;
        node = P32(sequence->firstNode);
        model = P32(owner->model);
        bit = flags;
        if (current != NULL) {
            wazaSequenceApplyStop(current);
        }
        if (owner->animationActive != 0 && sequence->animationMode == 2) {
            wazaSequenceSysResetAnimationExcept(owner);
        }
        wazaSequenceOwnerSetAnim(owner, sequence);
        if ((sequence->flags & 0x08000000) != 0) {
            GSmodelLinkToGSparticleBank(model, NULL);
        }
        wazaSequencePokemonMotionStart(owner, bit);
        P32_SET(owner->currentSequence, sequence);
        sequence->active = 1;
        if ((sequence->flags & 0x04000000) != 0) {
            battleGridHideModelsExcept(owner);
        }
        if (sequence->cameraActive != 0) {
#ifdef TARGET_PC
            /* PC (observe only, 2026-10-28): marks the move camera's draws for the replay's per-draw resync
             * (ORRE_ROLL_SEEDS, src/game/pc_debug_game.c), which keeps them in their own list */
            extern s32 pc_move_camera_depth;
            pc_move_camera_depth++;
            battleCameraStartWaza(owner, sequence);
            pc_move_camera_depth--;
#else
            battleCameraStartWaza(owner, sequence);
#endif
        }
        while (node != NULL) {
            node->runtimeState = 0;
            node = P32(node->next);
        }
        sequence->state = 0;
        sequence->stopping = 0;
        wazaSequenceUpdate(sequence);
    }
}
