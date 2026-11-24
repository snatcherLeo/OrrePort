/*
 * pc_health: OrrePort watching itself (snatcherLeo, 2026-11-13: "We will need a memory watch and to ensure this program (and
 * especialy STudio) are monitored, in case those programs themselves ever have issues"; "loggers, whatever is needed.
 * SEparate loggers for the app performance specifically"). See pc_health.c for the why of each part.
 */
#ifndef PC_HEALTH_H
#define PC_HEALTH_H

#ifdef __cplusplus
extern "C" {
#endif

/* Once, after the platform's start-up checks (pc_platform_start): the hang watch, the performance log and the memory
 * watch. Safe to call when the log could not start (it then does nothing). */
void pc_health_start(void);

/* Once per presented frame, on the main thread (the game's frames, the hub's, the menu's held frames): the
 * performance log's frame times */
void pc_health_frame(void);

/* This run's performance log (logs/orreport_<date>_perf.csv), or NULL; Report a problem adds it */
const char* pc_health_perf_path(void);

#ifdef __cplusplus
}
#endif

#endif
