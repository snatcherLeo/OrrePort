/*
 * pc_adapter_setup.h: Settings > Controls' "GameCube adapter" row (2026-11-13; snatcherLeo: "as long as they don't need
 * Zadig, and it just converts it for them", then "Separate helper (1)"). pc_adapter_setup.c says how and why.
 *
 * Platforms: Windows only does anything. Linux and macOS get their own way later, based on how each system can best
 * read the adapter (snatcherLeo, 2026-11-13; likely a udev rule on Linux, nothing on macOS); until then these report
 * "nothing to set up" there, so the row never shows.
 */
#ifndef PC_ADAPTER_SETUP_H
#define PC_ADAPTER_SETUP_H

#include <stddef.h> /* size_t */

#ifdef __cplusplus
extern "C" {
#endif

/* Looks again whether Nintendo's adapter (USB 057E:0337) is plugged in on a driver that can't read it. */
void pc_adapter_refresh(void);
/* 1 when it is (the row shows), from the last look */
int pc_adapter_needs_setup(void);

enum {
    PC_ADAPTER_STARTED = 0,
    PC_ADAPTER_NO_HELPER = -1, /* "OrrePort Adapter Setup.exe" isn't next to the exe */
    PC_ADAPTER_DECLINED = -2,  /* Windows' administrator question was answered No */
    PC_ADAPTER_FAILED = -3     /* it couldn't be started */
};
/* Starts the setup program (Windows asks for administrator rights first). */
int pc_adapter_setup_start(void);
/* While it runs: 1. When it has ended: 0, with its exit code (orre_adapter_setup.c: 0 ready, 2 no adapter, 3 / 4
 * failed). Not running: -1. */
int pc_adapter_setup_poll(int* exit_code);
/* While it runs: its last step from its progress file ("find", "prepare", "install", "done") and that step's percent
 * (5, 15, 40, 100); 0 when there is nothing to read yet. The busy screen's bar (snatcherLeo: "have a progress bar too"). */
int pc_adapter_setup_progress(int* percent, char* step, size_t size);

#ifdef __cplusplus
}
#endif

#endif
