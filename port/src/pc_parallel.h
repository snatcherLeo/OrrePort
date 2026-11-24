/*
 * pc_parallel.h: run a few independent jobs at the same time and wait for all of them (2026-11-13).
 *
 * Why: start-up drew its pictures one after another on one CPU core (the hub's disc SVG seven times, the blur, the
 * window icon's thirteen sizes), about half a second of the 1.45 s before the window showed (measured with the
 * [startup] marks; snatcherLeo: "is it supposed to take a second or 2 for our hub to open up?"). Each picture is a plain
 * function of its inputs (no shared state), so they can be drawn side by side.
 *
 * Rules for a job: it touches only its own output and reads shared inputs; no RmlUi, SDL window or Aurora calls (those
 * belong to the main thread). The first job runs on the calling thread, so one job costs no thread at all.
 *
 * Platforms: std::thread is standard C++ and works the same on Windows (MinGW's POSIX threads, libwinpthread, already
 * shipped), Linux and macOS (verified on Windows; the others likely).
 */
#ifndef PC_PARALLEL_H
#define PC_PARALLEL_H

#include <functional>
#include <thread>
#include <vector>

inline void pc_run_together(const std::vector<std::function<void()>>& jobs) {
    std::vector<std::thread> threads;
    threads.reserve(jobs.size());
    for (size_t i = 1; i < jobs.size(); ++i) {
        threads.emplace_back(jobs[i]);
    }
    if (!jobs.empty()) {
        jobs[0]();
    }
    for (std::thread& t : threads) {
        t.join();
    }
}

#endif
