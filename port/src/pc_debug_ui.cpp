/*
 * pc_debug_ui.cpp: the debug toolkit's in-game menu (ImGui through Aurora; F1 shows / hides it). Only with
 * ORRE_DEBUG=1. Every button queues the same text command the command line and pad scripts use (pc_debug.c), so the
 * three ways in behave the same and the log shows exactly what ran.
 */
#include "pc_debug.h"
#include "pc_settings.h"

#include <aurora/imgui.h>
#include <imgui.h>

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace {
bool s_open = false; /* closed until F1 (once, its gamepad navigation read the real controller and toggled a checkbox in a scripted run; navigation by pad is now off) */
int s_room = 1, s_entry = 0;
int s_flag = 0, s_flag_value = 1;
int s_item = 1, s_item_count = 1;
int s_species = 1, s_level = 5;
char s_team[32] = "test";
int s_seed = 0;
int s_speed = 100;
bool s_skip_movies = false;
int s_slot = 0;
int s_battle = 10;
bool s_panel_loaded = false;
char s_slot_label[64] = "";
bool s_slots_stale = true;
char s_spot_filter[32] = "";
ImTextureID s_pause_tex = 0; /* the paused picture's texture (aurora_imgui_add_texture) */
int s_pause_tex_serial = -1;
int s_dock = 2;           /* 0 floating, 1 left edge, 2 right edge */
int s_alpha_percent = 80; /* background opacity */
char s_command[160] = "";

void queue(const char* fmt, ...) IM_FMTARGS(1);
void queue(const char* fmt, ...) {
    char line[160];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);
    pc_debug_queue(line);
}
} // namespace

/* Help › Debug menu… in the menu bar (pc_settings_overlay.cpp): the same as F1 (snatcherLeo, 2026-11-13, the Debug tools
 * switch) */
extern "C" void pc_debug_ui_toggle(void) {
    if (pc_debug_enabled()) {
        s_open = !s_open;
    }
}

extern "C" void pc_debug_ui_draw(void) {
    if (!pc_debug_enabled() || ImGui::GetCurrentContext() == nullptr) {
        return;
    }
    /* the controller belongs to the game: the menu is driven by mouse and keyboard only, so a pad (real or scripted)
     * never changes a setting in it (2026-10-17) */
    ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_NavEnableGamepad;
    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) {
        s_open = !s_open;
    }
    {
        /* while the game is paused nothing else draws: show the paused picture (pc_debug.c) full-size, kept 4:3,
         * behind the menu. Aurora's ImGui textures cannot be freed, so each new picture costs 1.2 MB (debug only). */
        int serial = 0;
        const unsigned char* image = pc_debug_pause_image(&serial);
        if (image != nullptr) {
            if (serial != s_pause_tex_serial) {
                s_pause_tex = aurora_imgui_add_texture(640, 480, image);
                s_pause_tex_serial = serial;
            }
            const ImVec2 size = ImGui::GetIO().DisplaySize;
            float w = size.x, h = size.x * 3.0f / 4.0f;
            if (h > size.y) {
                h = size.y;
                w = size.y * 4.0f / 3.0f;
            }
            const ImVec2 p0((size.x - w) / 2.0f, (size.y - h) / 2.0f);
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            bg->AddRectFilled(ImVec2(0, 0), size, IM_COL32(0, 0, 0, 255));
            bg->AddImage(s_pause_tex, p0, ImVec2(p0.x + w, p0.y + h));
            bg->AddText(ImVec2(p0.x + 12, p0.y + 10), IM_COL32(255, 255, 0, 255), "PAUSED (Pause: resume, '.': one frame)");
        }
    }
    if (pc_debug_paused()) { /* over the re-presented last frame (pc_vi.c) */
        ImGui::GetForegroundDrawList()->AddText(ImVec2(12, 10), IM_COL32(255, 255, 0, 255),
                                                "PAUSED (Pause: resume, '.': one frame)");
    }
    if (!s_open) {
        return;
    }
    const PcDebugApi* api = pc_debug_api();
    /* placement (window-and-debug-plan.md, batch 1 item 4: the panel must not cover the game): docked to the left or
     * right edge at full height, or floating where it was dragged; see-through by a chosen amount; both remembered
     * in user/settings.ini (the floating place in imgui.ini, ImGui's own file in the same folder) */
    if (!s_panel_loaded) {
        s_panel_loaded = true;
        s_dock = pc_settings_int("debug_panel_dock", 2);
        s_alpha_percent = pc_settings_int("debug_panel_opacity", 80);
    }
    {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        const float width = 400.0f;
        if (s_dock == 1 || s_dock == 2) {
            ImGui::SetNextWindowPos(ImVec2(s_dock == 1 ? vp->WorkPos.x : vp->WorkPos.x + vp->WorkSize.x - width,
                                           vp->WorkPos.y),
                                    ImGuiCond_Always);
            ImGui::SetNextWindowSize(ImVec2(width, vp->WorkSize.y), ImGuiCond_Always);
        } else {
            ImGui::SetNextWindowSize(ImVec2(width, 520), ImGuiCond_FirstUseEver);
        }
        ImGui::SetNextWindowBgAlpha(s_alpha_percent / 100.0f);
    }
    if (!ImGui::Begin("OrrePort debug (F1)", &s_open)) {
        ImGui::End();
        return;
    }
    if (ImGui::CollapsingHeader("Panel")) {
        bool changed = false;
        changed |= ImGui::RadioButton("float", &s_dock, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("dock left", &s_dock, 1);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("dock right", &s_dock, 2);
        changed |= ImGui::SliderInt("opacity %", &s_alpha_percent, 20, 100);
        if (changed) {
            pc_settings_set_int("debug_panel_dock", s_dock);
            pc_settings_set_int("debug_panel_opacity", s_alpha_percent);
            pc_settings_save();
        }
        ImGui::TextDisabled("F5 save / F9 load the slot below, F12 screenshot, Pause / '.' frame step");
    }
    if (api == nullptr) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "game side not connected: game commands are unavailable");
    } else {
        /* the header (window-and-debug-plan.md, batch 2 item 1): build, room (with its archive name), battle */
        ImGui::TextUnformatted(pc_debug_build_stamp());
        if (api->current_room != nullptr) {
            const PcDebugU32 room = api->current_room();
            const char* name = api->room_name != nullptr ? api->room_name(room) : nullptr;
            const PcDebugU32 battle = api->battle_number != nullptr ? api->battle_number() : 0;
            if (battle != 0) {
                ImGui::Text("room 0x%X %s | battle %u", room, name != nullptr ? name : "", battle);
            } else {
                ImGui::Text("room 0x%X %s", room, name != nullptr ? name : "");
            }
        }
    }

    if (ImGui::CollapsingHeader("Warp", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::InputInt("room", &s_room, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputInt("entry", &s_entry);
        if (ImGui::Button("warp")) {
            queue("warp 0x%X %d", s_room, s_entry);
        }
    }
    if (ImGui::CollapsingHeader("Flags")) {
        ImGui::InputInt("flag id", &s_flag);
        ImGui::InputInt("value", &s_flag_value);
        if (ImGui::Button("set")) {
            queue("flag %d %d", s_flag, s_flag_value);
        }
        ImGui::SameLine();
        if (ImGui::Button("read")) {
            queue("flag %d", s_flag);
        }
    }
    if (ImGui::CollapsingHeader("Items and Pokemon")) {
        ImGui::InputInt("item id", &s_item);
        ImGui::InputInt("count", &s_item_count);
        if (ImGui::Button("give item")) {
            queue("item %d %d", s_item, s_item_count);
        }
        ImGui::InputInt("species", &s_species);
        ImGui::InputInt("level", &s_level);
        if (ImGui::Button("give Pokemon")) {
            queue("pokemon %d %d", s_species, s_level);
        }
        ImGui::InputText("preset team", s_team, sizeof(s_team));
        if (ImGui::Button("set team")) {
            queue("team %s", s_team);
        }
    }
    if (ImGui::CollapsingHeader("Battle")) {
        ImGui::InputInt("battle id", &s_battle); /* 10 = Willie */
        if (ImGui::Button("start battle")) {
            queue("fight %d", s_battle);
        }
        if (ImGui::Button("win")) {
            queue("battle win");
        }
        ImGui::SameLine();
        if (ImGui::Button("lose")) {
            queue("battle lose");
        }
    }
    if (ImGui::CollapsingHeader("Run", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SliderInt("speed %", &s_speed, 10, 1000)) {
            queue("speed %d", s_speed);
        }
        /* an exact value and presets (snatcherLeo 2026-10-17: the slider alone was easy to overshoot) */
        if (ImGui::InputInt("speed (type a value)", &s_speed, 10, 100, ImGuiInputTextFlags_EnterReturnsTrue)) {
            s_speed = s_speed < 10 ? 10 : (s_speed > 1000 ? 1000 : s_speed);
            queue("speed %d", s_speed);
        }
        {
            static const int presets[] = {100, 200, 300};
            for (int i = 0; i < 3; i++) {
                char label[16];
                snprintf(label, sizeof(label), "%d%%", presets[i]);
                if (i > 0) {
                    ImGui::SameLine();
                }
                if (ImGui::Button(label)) {
                    s_speed = presets[i];
                    queue("speed %d", s_speed);
                }
            }
        }
        if (ImGui::Button(pc_debug_paused() ? "resume (Pause)" : "pause (Pause)")) {
            queue("pause");
        }
        ImGui::SameLine();
        if (ImGui::Button("one frame ('.')")) {
            queue("step");
        }
        ImGui::SameLine();
        if (ImGui::Button("screenshot (F12)")) {
            queue("screenshot");
        }
        if (ImGui::Button("where is it stuck? (5 stack samples to the log)")) {
            queue("stack 5");
        }
        if (ImGui::Checkbox("skip movies and cutscenes", &s_skip_movies)) {
            queue("skipmovies %s", s_skip_movies ? "on" : "off");
        }
        ImGui::InputInt("RNG seed", &s_seed, 1, 100, ImGuiInputTextFlags_CharsHexadecimal);
        if (ImGui::Button("set seed")) {
            queue("seed 0x%X", s_seed);
        }
        if (ImGui::InputInt("checkpoint slot", &s_slot)) {
            s_slot = s_slot < 0 ? 0 : (s_slot > 99 ? 99 : s_slot);
            pc_debug_set_hotkey_slot(s_slot); /* F5 / F9 use the same slot */
        }
        ImGui::InputText("label", s_slot_label, sizeof(s_slot_label));
        if (ImGui::Button("save checkpoint")) {
            queue("save %d %s", s_slot, s_slot_label);
            s_slots_stale = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("load checkpoint")) {
            queue("load %d", s_slot);
        }
    }
    if (ImGui::CollapsingHeader("Checkpoint slots")) {
        /* the labels of slots 0-9 (named checkpoint slots, batch 2 item 2); click one to pick it for F5 / F9 */
        static char labels[10][160];
        if (s_slots_stale || ImGui::Button("refresh")) {
            for (int i = 0; i < 10; i++) {
                pc_debug_slot_label(i, labels[i], sizeof(labels[i]));
            }
            s_slots_stale = false;
        }
        for (int i = 0; i < 10; i++) {
            char line[200];
            snprintf(line, sizeof(line), "%d: %s", i, labels[i][0] != '\0' ? labels[i] : "(empty)");
            if (ImGui::Selectable(line, s_slot == i)) {
                s_slot = i;
                pc_debug_set_hotkey_slot(i);
            }
        }
    }
    if (ImGui::CollapsingHeader("Quick jumps")) {
        /* the spots.json, one per story step (batch 2 item 4): the flags first, then the warp, in one click */
        ImGui::InputText("filter", s_spot_filter, sizeof(s_spot_filter));
        ImGui::BeginChild("spots", ImVec2(0, 180), ImGuiChildFlags_Borders);
        for (int i = 0; i < pc_debug_spot_count(); i++) {
            const char* name = pc_debug_spot_name(i);
            if (s_spot_filter[0] != '\0' && strstr(name, s_spot_filter) == nullptr) {
                continue;
            }
            if (ImGui::Selectable(name)) {
                queue("jump %s", pc_debug_spot_id(i));
            }
        }
        ImGui::EndChild();
        ImGui::TextDisabled("spots are likely right until each jump is checked in the game");
    }
    if (ImGui::CollapsingHeader("Command", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::InputText("##cmd", s_command, sizeof(s_command), ImGuiInputTextFlags_EnterReturnsTrue)) {
            pc_debug_queue(s_command);
            s_command[0] = '\0';
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(Enter; 'help')");
        ImGui::BeginChild("log", ImVec2(0, 160), ImGuiChildFlags_Borders);
        for (int i = 0; i < pc_debug_log_count(); i++) {
            ImGui::TextUnformatted(pc_debug_log_line(i));
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }
    ImGui::End();
}
