/**
 * @file gs_range_801653CC_prefix.c
 * @brief Candidate gs-engine range, 0x801653CC - 0x801654E0.
 */
#include "game/gs_range_801653CC_shared.h"

u32 soundPlayFanfare(u32 id, void* buffer, u32 size)
{
    GSsndEntry* entry;
    void* resource;
    void* wave;
    u32 waveIndex;

    if (id == 0 || id >= *snd_data_number) {
        return 0;
    }

    entry = snd_data;
    entry += id;
    waveIndex = entry->waveIndex;
    if (waveIndex < *lbl_80478E30) {
        resource = GSresAllocResourceAlign(0x10000, 0x20, 0, 0x408, 0);
        if (resource != 0) {
            wave = DP(lbl_80478E34[waveIndex].data);
            if (wave != 0) {
                fn_80165EE0(wave, resource, 0x10000);
                soundAttachWork(id, 0, 0x408);
                soundStart(id, PC_PTR32(buffer), size, 0);
                /* Retail emits fn_801666BC before _threadSwitch, i.e. the
                 * bottom-test rotation. */
                for (;;) {
                    if (GSsndGetStatus(id) != 2) {
                        break;
                    }
                    _threadSwitch();
                }
                soundKill(id);
            }
            fn_800F9210(0, 0x408);
        }
    }

    return 1;
}
