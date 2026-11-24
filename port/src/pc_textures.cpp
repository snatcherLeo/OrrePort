/*
 * pc_textures.cpp: Dolphin-format texture packs and the render scale. The pack loader is kept for developer runs only
 * (snatcherLeo's experiment, 2026-10-17; never on in GameCube-accurate mode).
 *
 * *Changed 2026-11-22 (snatcherLeo: "Our goal is to have the "Mods" folder house everything, so we don't want that texture
 * pack thing in settings.ini at all"; "that'll be later anyhow, after playtest release, when we add that option for our
 * Studio app to recognize texture packs and apply them"):* there is no player setting for packs any more: no
 * settings.ini key (the "texture_pack" entry left pc_options.c, and pc_settings.c drops an old line on load) and no
 * menu-bar switch. Packs come back after the playtest release, through the Mods folder and Studio. Until then the
 * loader runs only when a developer sets ORRE_TEXTURE_PACK=1 for one run, so the Aurora path stays tested.
 *
 * Texture packs: Aurora already replaces textures the way Dolphin does (files named
 * tex1_<w>x<h>[_m]_<texture hash>[_<palette hash>]_<format>.dds or .png, Dolphin's "Load/Textures/<game id>" layout).
 * With ORRE_TEXTURE_PACK=1 the port loads every pack file under the pack folder (pc_paths.h PC_PLACE_TEXTURES:
 * Pokemon Colosseum\Textures\ in the portable layout; <exe>/user/textures/GC6E01/ in the flat layout; subfolders
 * allowed) at start-up. With ORRE_TEXTURE_REPORT=1, Aurora also logs, once per texture, "applied <name>" for a pack
 * entry the game uses and "missing runtime key <name>" (with the pack's near misses: same size, same hash) for one it
 * lacks, and dumps that texture to <exe>/cache/texture_dumps/; port/tools/texpack_report.py turns the log into the
 * lists of pack files that apply and that never match.
 *
 * Render scale: settings.ini "render_scale" = 0 (auto: the window's resolution, Aurora's default), 1 (the console's
 * 640x480), 2, 3, ... (multiples of it).
 *
 * Platforms: plain C++ and std::filesystem over Aurora; the same on Windows, Linux and macOS (likely; only Windows
 * built).
 */
#include <dolphin/gx.h> /* aurora/texture.hpp uses GXTexObj / GXTlutObj */
#include <aurora/texture.hpp>
#include <dolphin/vi.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#include "pc_options.h"
#include "pc_settings.h"

namespace {
aurora::texture::ReplacementGroup s_group;
} // namespace

extern "C" void pc_textures_init(const char* pack_folder, int automated) {
    const int scale = pc_settings_int("render_scale", 0);
    (void)automated;
    if (scale > 0) {
        VISetFrameBufferScale(static_cast<float>(scale));
        std::printf("[textures] render scale %dx the console's frame buffer (settings.ini)\n", scale);
    }

    /* the developer switch (2026-11-22): nothing in settings.ini or the menus turns packs on */
    const char* env = getenv("ORRE_TEXTURE_PACK");
    if (env == nullptr || atoi(env) == 0) {
        return;
    }
    if (pc_option_gamecube_accurate()) {
        std::printf("[textures] ORRE_TEXTURE_PACK ignored: GameCube-accurate mode keeps the original textures\n");
        return;
    }
    std::error_code ec;
    std::string folder = pack_folder != nullptr ? pack_folder : "";
    while (!folder.empty() && (folder.back() == '/' || folder.back() == '\\')) folder.pop_back(); /* as before 4.1 */
    const std::filesystem::path root = std::filesystem::u8path(folder);
    if (!std::filesystem::is_directory(root, ec)) {
        std::printf("[textures] ORRE_TEXTURE_PACK=1 but no pack folder at %s\n", root.u8string().c_str());
        return; /* no pack: nothing to do (Aurora would create the folder) */
    }
    s_group = aurora::texture::load_replacement_directory(root);
    std::printf("[textures] texture pack (ORRE_TEXTURE_PACK, developer run): %zu file(s) registered from %s%s\n",
                s_group.registrations.size(), root.u8string().c_str(),
                getenv("ORRE_TEXTURE_REPORT") != nullptr ? " (report mode: applied / missing lines in this log)" : "");
}
