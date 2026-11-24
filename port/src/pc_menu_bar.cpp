/*
 * pc_menu_bar.cpp: the window's menu bar (snatcherLeo, 2026-10-17: "a top toolbar 'File' or something" with the texture pack
 * switch). Drawn with Aurora's ImGui over the top edge of the picture, so it works the same on Windows, Linux and Mac.
 * It stays out of the way while playing: it appears when the mouse is at the top of the window and hides again when
 * the mouse leaves it with no menu open. Mouse only: the controller belongs to the game. Never shown in an automated
 * run (those never change the player's settings).
 *
 * Menus today: File (Open OrrePort folder: the user folder, screenshots), Graphics (Stretch to window, Fullscreen;
 * the Texture pack item was removed 2026-11-22), Gameplay (Smooth battle start: works once the game side's hook is in). New player options go here first; the F1 debug panel stays for testing tools. No Exit item yet: closing
 * the window is the one tested way out.
 */
#include <imgui.h>

#include <SDL3/SDL_misc.h>

#include <cstddef>
#include <string>

extern "C" {
const char* pc_platform_data_path(const char* name, char* out, size_t size); /* pc_platform.c: <exe folder>/<name> */
void pc_platform_toggle_fullscreen(void); /* pc_platform.c */
int pc_platform_is_fullscreen(void);
int pc_platform_automated_run(void);
int pc_platform_stretched(void);
void pc_platform_set_stretched(int on);
/* the RmlUi trial's Report a problem (pc_settings_overlay.cpp; only in RmlUi builds, so a weak reference: NULL in
 * the others, and the Help menu then has no entry) */
void pc_settings_overlay_open_report(void) __attribute__((weak));
}

#include "pc_options.h" /* the preset system: the Gameplay menu lists its entries */

#include <cstring>

namespace {
constexpr float RevealHeight = 6.0f; /* the mouse within this many pixels of the top edge shows the bar */
bool s_shown = false;
} // namespace

extern "C" void pc_menu_bar_draw(void) {
    /* builds with the RmlUi overlay have its own bar (pc_settings_overlay.cpp, mockup 46: File and Help, snatcherLeo
     * 2026-11-13); this developer-toolkit bar is then not drawn */
    if (pc_settings_overlay_open_report != nullptr) {
        return;
    }
    if (ImGui::GetCurrentContext() == nullptr || pc_platform_automated_run()) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const bool mouseInWindow = ImGui::IsMousePosValid(&io.MousePos);
    const float barHeight = ImGui::GetFrameHeight();
    if (!s_shown) {
        s_shown = mouseInWindow && io.MousePos.y >= 0.0f && io.MousePos.y < RevealHeight;
    } else if (!ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        s_shown = mouseInWindow && io.MousePos.y < barHeight + 8.0f; /* a little slack below the bar */
    }
    if (!s_shown || !ImGui::BeginMainMenuBar()) {
        return;
    }
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open OrrePort folder")) {
            char path[1100];
            const char* folder = pc_platform_data_path("", path, sizeof(path));
            std::string url = folder[0] == '/' ? "file://" : "file:///"; /* Linux / Mac paths start with '/' */
            url += folder;
            for (char& c : url) {
                if (c == '\\') {
                    c = '/';
                }
            }
            SDL_OpenURL(url.c_str()); /* the system's file browser */
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Graphics")) {
        /* *Removed 2026-11-22 (snatcherLeo):* the Texture pack item; packs have no switch until the Mods folder and Studio
         * bring them back after the playtest release (pc_textures.cpp) */
        bool stretched = pc_platform_stretched() != 0;
        if (ImGui::MenuItem("Stretch to window", nullptr, &stretched)) {
            pc_platform_set_stretched(stretched ? 1 : 0);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Off (default): the console's 4:3 picture with black bars. On: fill the window");
        }
        bool fullscreen = pc_platform_is_fullscreen() != 0;
        if (ImGui::MenuItem("Fullscreen", "F11", &fullscreen)) {
            pc_platform_toggle_fullscreen();
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Gameplay")) {
        /* every entry of the preset system's list in this category (pc_options.c); the accurate preset greys them out */
        const bool accurate = pc_option_gamecube_accurate() != 0;
        ImGui::TextDisabled(accurate ? "Preset: GameCube-accurate (every enhancement off)" : "Preset: Normal");
        ImGui::Separator();
        for (int i = 0; i < pc_option_count(); i++) {
            const PcOption* o = pc_option_at(i);
            if (!o->in_menu || std::strcmp(o->category, "Gameplay") != 0) {
                continue;
            }
            bool on = pc_option(o->id) != 0;
            if (ImGui::MenuItem(o->name, nullptr, &on, !accurate)) {
                pc_option_set(o->id, on ? 1 : 0);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                ImGui::SetTooltip("%s", accurate ? "Off in GameCube-accurate mode (this save's own setting)"
                                                 : o->description);
            }
        }
        ImGui::EndMenu();
    }
    if (pc_settings_overlay_open_report != nullptr && ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Report a problem…")) {
            pc_settings_overlay_open_report();
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
}
