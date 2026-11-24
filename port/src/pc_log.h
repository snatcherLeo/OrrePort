/* pc_log: an automatic log file for every run, <exe dir>/logs/orreport_YYYYMMDD_HHMMSS.log (see pc_log.c) */
#ifndef PC_LOG_H
#define PC_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Start-up, before anything else prints: opens the log (first line = first_line, e.g. the build stamp), keeps the
 * newest 20 logs, and copies all later stdout / stderr output to it as well as to the console. 0 if it could not. */
int pc_log_start(const char* first_line);
/* Wait until everything printed so far is in the file (crash and watchdog reports call it) */
void pc_log_drain(void);
/* This run's log file, or NULL */
const char* pc_log_path(void);
/* "[startup] <ms> ms: <what>": how long after the program began a start-up step was reached (snatcherLeo, 2026-11-13: "is it
 * supposed to take a second or 2 for our hub to open up?"). Every start's log then says where the time went, so a
 * slow start can be read from a player's report too. The time counts from the log's start (the first thing main
 * does); on Windows the first line also says how long loading the program and its DLLs took before that.
 * Platform-neutral (SDL's clock), except that Windows-only first line. */
void pc_startup_mark(const char* what);

/* "Didn't close cleanly" (5.1, snatcherLeo 2026-11-23: "Yes, as suggested"; the same check as Studio's): every normal end of
 * the program writes "[exit] clean: <how>" as its last line and drains the log (pc_log_exit_clean); a crash, a hang
 * ended by the watchdog or Task Manager's End task leave the line out. At the next start, the newest earlier log is
 * read: when it was written by a build that ends with the line (its "[log] exit line: on") and the line is missing,
 * that run ended unexpectedly, and the hub offers a report with that log (pc_log_previous_unclean). Every OS the same. */
void pc_log_exit_clean(const char* how);
/* the previous run's log when it ended unexpectedly, else NULL; pc_log_previous_unclean_clear forgets it (Not now, or
 * once a report has carried it) */
const char* pc_log_previous_unclean(void);
void pc_log_previous_unclean_clear(void);

#ifdef __cplusplus
}
#endif

#endif
