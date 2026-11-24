/**
 * @file input_exact_800F75FC.c
 * @brief Exact setter at 0x800F75FC: installs the script VM's native-call table
 *        (GSVMPool.natives; main() passes lbl_802E1CF0, docs/engine/script-vm.md).
 */
#include "dolphin/types.h"
#include "game/gs_vm.h"

s32 fn_800F75FC(void* natives) {
    lbl_80478B00->natives = natives;
    return 0;
}
