// cry.cpp: a species' cry, read from the disc and decoded (2026-11-22, for the Pokémon editor's Cries card; snatcherLeo: "a
// play button for a Pokemon to play their cry and fainting cry").
// Why: Studio plays the game's own cries without the game's sound engine. The path a cry takes in the game (verified on
// 8 species across both banks with a scratch reader, 2026-11-22; docs/engine/music.md "Cries"):
//   species record +0x0E, its sound id (National Dex number + 569)
//   -> the game's sound table in common_rel (snd_data, 12-byte rows; +0x04 the MusyX effect id; tables.py
//      "sound_effects")
//   -> the effect in a cry bank's effect table (MusyX FX_TAB, 10 bytes: id, macro, voices, priority, volume, pan, key,
//      volume group; src/musyx/runtime/synth.c), se_nakigoe holding effects 0-200 and se_nakigoe2 980-1166
//   -> its macro in the bank's pool (8-byte steps; opcode 0x10 starts a sample, its id in bits 8-23)
//   -> the sample in the bank's sample directory (sdir, 0x20 bytes each) and data file (sound/snd_<bank>.samp),
//      GameCube DSP-ADPCM, decoded as tools/local/colo/musyx.py does.
// Every cry macro is the same short program (age speed, volume, start sample, wait, end), played at note 60 on a
// sample keyed 60: the cry is its sample at its own rate, 11,025 Hz. The rate is still worked out from the note and the
// sample's key, so a modded effect with another note plays at its pitch. A macro that reaches its sample through a
// keymap or a layer, or other macro steps (pitch, envelopes), aren't followed: the 386 cries have none.
// Platforms: plain C++, every OS.
#include <cmath>
#include <cstdlib>

#include "internal.hpp"

namespace orre {
namespace {

constexpr uint32_t kSoundOff = 0x13fe08, kSoundCount = 0x14a6ec, kSoundSize = 12;  // common_rel's sound table

// the disc file whose path ends with `tail` ("snd_se_nakigoe.samp" sits in the sound folder)
bool read_tail(Disc& d, const std::string& tail, Bytes& out) {
    for (const auto& [path, idx] : d.files)
        if (path.size() >= tail.size() && path.compare(path.size() - tail.size(), tail.size(), tail) == 0)
            return d.read_file(path, out);
    return false;
}

// GameCube DSP-ADPCM -> 16-bit PCM (8-byte frames: a header byte, then 14 signed 4-bit samples)
void dsp_adpcm(const uint8_t* data, size_t size, int count, const int16_t coefs[16], std::vector<int16_t>& out) {
    out.clear();
    out.reserve((size_t)count);
    int h1 = 0, h2 = 0;
    for (size_t p = 0; (int)out.size() < count && p + 8 <= size; p += 8) {
        const int ps = data[p], pred = (ps >> 4) & 7, scale = 1 << (ps & 15);
        const int c1 = coefs[pred * 2], c2 = coefs[pred * 2 + 1];
        for (int i = 0; i < 14 && (int)out.size() < count; ++i) {
            const int byte = data[p + 1 + i / 2];
            int nib = i % 2 == 0 ? byte >> 4 : byte & 15;
            if (nib >= 8) nib -= 16;
            int v = ((nib * scale) << 11) + c1 * h1 + c2 * h2 + 1024;
            v >>= 11;
            v = v < -32768 ? -32768 : v > 32767 ? 32767 : v;
            out.push_back((int16_t)v);
            h2 = h1;
            h1 = v;
        }
    }
}

// One cry bank's effect id -> PCM; false when the bank hasn't that effect
bool bank_cry(Disc& d, const char* bank, uint16_t fx_id, OrreSound* out) {
    const Bytes* common = d.archive("common");
    if (!common) return false;
    Bytes proj, pool, sdir, samp;
    const std::string b = bank;
    for (const auto& e : fsys_entries(*common)) {
        if (e.name == "snd_" + b + "_proj") proj = fsys_load(*common, e);
        else if (e.name == "snd_" + b + "_pool") pool = fsys_load(*common, e);
        else if (e.name == "snd_" + b + "_sdir") sdir = fsys_load(*common, e);
    }
    if (proj.empty() || pool.empty() || sdir.empty()) return false;
    // the project's sound-effect group (GROUP_DATA: next, id, type; +0x1C the effect table: u16 count, then 10-byte
    // entries from +4, as sndPushGroup hands them to dataInsertFX)
    uint32_t macro = 0xFFFF;
    int key = 60;
    for (uint32_t o = 0, guard = 0; o + 0x28 <= proj.size() && guard < 64; ++guard) {
        const uint32_t next = be32(proj, o);
        if (be16(proj, o + 6) == 1) {
            const uint32_t t = be32(proj, o + 0x1C);
            const uint16_t n = be16(proj, t);
            for (uint32_t k = 0; k < n; ++k) {
                const uint32_t e = t + 4 + 10 * k;
                if (be16(proj, e) == fx_id) { macro = be16(proj, e + 2); key = u8(proj, e + 8) & 0x7F; break; }
            }
        }
        if (macro != 0xFFFF || next == 0xFFFFFFFF) break;
        o = next;
    }
    if (macro == 0xFFFF || (macro & 0xC000) != 0) return false;  // (not found, or a keymap / layer: not followed)
    // the macro in the pool (header: the macro list's offset; entries {u32 size, u16 id, u16 0, steps})
    int sample = -1;
    for (uint32_t o = be32(pool, 0), guard = 0; o && o + 8 <= pool.size() && guard < 4096; ++guard) {
        const uint32_t size = be32(pool, o);
        if (size == 0xFFFFFFFF || size < 8) break;
        if (be16(pool, o + 4) == macro) {
            for (uint32_t s = o + 8; s + 8 <= o + size && s + 8 <= pool.size(); s += 8) {
                const uint32_t p0 = be32(pool, s);
                const int op = p0 & 0x7F;
                if (op == 0x10) { sample = (int)((p0 >> 8) & 0xFFFF); break; }
                if (op == 0x00 || op == 0x01) break;
            }
            break;
        }
        o += size;
    }
    if (sample < 0) return false;
    // the sample directory: 0x20-byte entries {u16 id, ..., +4 offset, +0xC info (key << 24 | rate), +0x10 length
    // (format << 24 | samples), +0x1C the ADPCM block (coefficients from +8)}, ended by id 0xFFFF
    for (size_t i = 0; i + 0x20 <= sdir.size(); i += 0x20) {
        const uint16_t sid = be16(sdir, i);
        if (sid == 0xFFFF) break;
        if (sid != sample) continue;
        const uint32_t off = be32(sdir, i + 4), info = be32(sdir, i + 0xC), length = be32(sdir, i + 0x10);
        const uint32_t extra = be32(sdir, i + 0x1C);
        const int rate = (int)(info & 0xFFFF), root = (int)(info >> 24), count = (int)(length & 0xFFFFFF);
        if ((length >> 24) != 0 || rate <= 0 || count <= 0 || extra + 0x28 > sdir.size()) return false;  // (not ADPCM)
        int16_t coefs[16];
        for (int k = 0; k < 16; ++k) coefs[k] = (int16_t)be16(sdir, extra + 8 + 2 * k);
        if (!read_tail(d, "snd_" + b + ".samp", samp)) return false;
        const size_t bytes = (size_t)(count + 13) / 14 * 8;
        if (off + bytes > samp.size()) return false;
        std::vector<int16_t> pcm;
        dsp_adpcm(samp.data() + off, bytes, count, coefs, pcm);
        out->rate = (int)std::lround(rate * std::pow(2.0, (key - root) / 12.0));
        out->count = (int)pcm.size();
        out->pcm = static_cast<int16_t*>(std::malloc(pcm.size() * sizeof(int16_t)));
        if (!out->pcm) return false;
        std::memcpy(out->pcm, pcm.data(), pcm.size() * sizeof(int16_t));
        return true;
    }
    return false;
}

}  // namespace
}  // namespace orre

using namespace orre;

extern "C" void orre_sound_free(OrreSound* s) {
    if (!s) return;
    std::free(s->pcm);
    *s = OrreSound{};
}

extern "C" int orre_species_cry(OrreDisc* od, int species, OrreSound* out) {
    if (!od || !out) return 0;
    *out = OrreSound{};
    OrreSpecies s{};
    if (!orre_species_get(od, species, &s)) return 0;
    std::lock_guard<std::recursive_mutex> g(od->d.lock);
    auto rows = table_rows(od->d.rel(), kSoundOff, kSoundCount, kSoundSize);
    if (s.cry >= rows.size()) return 0;
    const uint8_t* row = rows[s.cry];
    if (!(row[0] & 0x80)) return 0;  // (bit 7: a sound effect; a cry always is)
    const uint16_t fx = be16(row + 4);
    for (const char* bank : {"se_nakigoe", "se_nakigoe2"})
        if (bank_cry(od->d, bank, fx, out)) return 1;
    return 0;
}
