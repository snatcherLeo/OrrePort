/*
 * sysinfo: the machine a log or report came from, and its screens (2026-11-22; 4.3 part 3, the logging list's
 * items 2 and 5, approved by snatcherLeo: "our logger should be able to tell us EVERYTHING we need to know about that user's
 * app before it crashed").
 *
 * Why: a problem that only one tester sees is often their machine's: an old graphics driver, a second screen at 150 %
 * scaling, little memory. Each function writes plain lines ("cpu: ...", "gpu 1: ...", "display 2: ...") that go into
 * the log at start and into a report's about.txt, so the same facts appear in both.
 *
 * Shared diagnostics library (2026-11-22: written in studio/diag, OrrePort moves onto it when it chooses, its
 * about.txt today reads Aurora's log lines): plain C, no app types. Windows today (the registry for the processor's
 * name and the Windows release, GlobalMemoryStatusEx, DXGI for every graphics adapter and its driver version,
 * EnumDisplayMonitors and GetDpiForMonitor for the screens). Linux would read /proc/cpuinfo, /proc/meminfo, os-release,
 * the GPU through Vulkan or DRM, and the screens through its display server; macOS sysctl, IOKit and NSScreen (likely).
 * Elsewhere for now these write "not read on this system".
 */
#ifndef ORRE_DIAG_SYSINFO_H
#define ORRE_DIAG_SYSINFO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the processor, memory, operating system and every graphics adapter with its driver version, one per line ("\n"
 * after each); returns the length written (cut to fit cap, always zero-ended when cap > 0) */
size_t diag_sysinfo(char* out, size_t cap);

/* every screen: its size, refresh, scaling, which is the main one, one per line */
size_t diag_displays(char* out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
