#include <dolphin/si.h>
#include "../../gamepad.hpp"
#include "../pad/adapter.hpp"

#include <SDL3/SDL_power.h>

#include <cstdio>

// OrrePort (2026-11-11, snatcherLeo's GBA link look): print each new (port, caller) of SIProbe once, with the answer,
// so the log shows which game code probes for a device (the GBA code looks for type 0x40000) next to the controller
// polling; the answer itself is unchanged. `addr2line -f -e orrecolo.exe <address>` names the caller.
static uint32_t si_probe_answer(int32_t chan);
uint32_t SIProbe(int32_t chan) {
  static struct { int32_t chan; void* caller; uint32_t answer; } seen[32];
  static int seen_n = 0;
  void* const caller = __builtin_return_address(0);
  const uint32_t answer = si_probe_answer(chan);
  for (int i = 0; i < seen_n; i++) {
    if (seen[i].chan == chan && seen[i].caller == caller && seen[i].answer == answer) {
      return answer;
    }
  }
  if (seen_n < 32) {
    seen[seen_n++] = {chan, caller, answer};
    printf("[si] SIProbe port %d from %p -> 0x%X (first)\n", static_cast<int>(chan), caller,
           static_cast<unsigned>(answer));
    fflush(stdout);
  }
  return answer;
}

static uint32_t si_probe_answer(int32_t chan) {
  auto* const controller = aurora::gamepad::get_controller_for_player(chan);
  if (controller == nullptr) {
    // OrrePort: a port driven by PADSetVirtualStatus (scripted input) answers like a standard controller, so games
    // that check the device type before reading menus (Colosseum's input.c) accept it
    if (chan >= 0 && aurora::pad::detail::has_virtual_status(static_cast<uint32_t>(chan))) {
      return SI_GC_CONTROLLER;
    }
    return SI_ERROR_NO_RESPONSE;
  }

  if (controller->m_isGameCube) {
    auto level = SDL_GetJoystickPowerInfo(SDL_GetGamepadJoystick(controller->m_controller), nullptr);
    if (level == SDL_POWERSTATE_UNKNOWN) {
      return SI_GC_WAVEBIRD;
    }
  }

  return SI_GC_CONTROLLER;
}
