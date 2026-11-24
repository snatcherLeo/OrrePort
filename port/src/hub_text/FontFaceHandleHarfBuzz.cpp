/*
 * FontFaceHandleHarfBuzz.cpp: OrrePort's copy of RmlUi's HarfBuzz sample font engine's per-size face
 * (RmlUi, Samples/basic/harfbuzz/src/FontFaceHandleHarfBuzz.cpp at the RmlUi revision Aurora pins; MIT licence, the RmlUi
 * team; the licence travels in licenses\RmlUi-LICENSE.txt). The other files of that sample are compiled unchanged from
 * RmlUi's fetched source (port/CMakeLists.txt); this one replaces the sample's own.
 *
 * What OrrePort changed (the project's build helper, 2026-11-16; hub_text/pc_ui_shaping.h says why):
 *   1. GetStringWidth and GenerateString share one layout, ShapeLine below, so a measured line and a drawn line always
 *      agree.
 *   2. The bidi order: the line's characters go through SheenBidi (the Unicode bidi algorithm), and each level run is
 *      shaped in its own direction and placed in drawn order. The sample shaped the whole line in one direction, taken
 *      from the element's `lang` (registered languages) or the first character's script, so an Arabic name inside an
 *      English line, or digits inside Arabic, came out in the wrong order.
 *   3. A font per run: each character goes to the page's own font if it has the letter, else to the first fallback face
 *      that has it (the Noto chain, loaded in pc_font_files.cpp's order by pc_ui_load_fonts), and each run of one font is
 *      shaped by HarfBuzz with that font. The sample shaped the line with the page's font and then retried unknown
 *      clusters one by one in the fallback fonts, which loses the context Arabic joining needs (each letter alone takes
 *      its isolated form) and splits emoji sequences. Marks, joiners and emoji modifiers stay in the run of the letter
 *      they ride on.
 *   4. Glyph places use HarfBuzz's own font tables (hb-ot) at the handle's size, so a shared FreeType face resized for
 *      another size can't skew them, and y offsets are applied upward (HarfBuzz's y points up, the page's down; the
 *      sample added them downward, which put stacked Thai marks too low).
 *   5. All glyphs of a line are found (and added to the textures) before the textures are rebuilt, so a new letter
 *      draws in the same frame.
 * Everything else (layers, effects, textures, glyph bitmaps) is the sample's code.
 *
 * Platforms: plain C++; the same on Windows, Linux and macOS (likely).
 */
#include "FontFaceHandleHarfBuzz.h"
#include "FontEngineDefault/FreeTypeInterface.h"
#include "FontFaceLayer.h"
#include "FontProvider.h"
#include "FreeTypeInterface.h"
#include <SheenBidi/SheenBidi.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cmath>
#include <functional>
#include <hb-ft.h>
#include <hb-ot.h>
#include <hb.h>
#include <numeric>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

static bool IsControlCharacter(Character c)
{
	return (char32_t)c < U' ' || ((char32_t)c >= U'\x7F' && (char32_t)c <= U'\x9F');
}

static bool IsKerningEnabled(const TextShapingContext& text_shaping_context, int font_size)
{
	static constexpr int minimum_font_size_to_enable_kerning = 14;

	switch (text_shaping_context.font_kerning)
	{
	case Style::FontKerning::Normal: return true;
	case Style::FontKerning::None: return false;
	default: return font_size >= minimum_font_size_to_enable_kerning;
	}
}

static Vector<hb_feature_t> GetTextShapingFeatures(const TextShapingContext& text_shaping_context, int font_size)
{
	Vector<hb_feature_t> shaping_features;

	if (!IsKerningEnabled(text_shaping_context, font_size))
	{
		// Kerning is enabled by default in HarfBuzz, so we need to explicitly disable it.
		shaping_features.emplace_back();
		hb_feature_from_string("kern=0", -1, &shaping_features.back());
	}

	return shaping_features;
}

/* ---- OrrePort: the shared line layout (changes 1-4 above) ---- */
namespace {

/* One face a line may use: [0] the page's own font, then the fallback faces in their load order. */
struct FaceRef {
	FT_Face ft;
	FontFaceHandleHarfBuzz* handle;
};
/* The faces, each fallback fetched only when a character asks for it: about 80 Noto fallbacks are loaded, and making a
 * sized handle for each one on every line would build 80 empty glyph textures per font size for an English page. */
struct Faces {
	std::function<FaceRef(int)> load; // fallback i (0-based) at this size, from the member function (it may read handles)
	std::vector<FaceRef> list;
	std::vector<char> loaded;
	Faces(FaceRef primary, int fallbacks, std::function<FaceRef(int)> loader) : load(std::move(loader))
	{
		list.assign((size_t)fallbacks + 1, FaceRef{nullptr, nullptr});
		loaded.assign((size_t)fallbacks + 1, 0);
		list[0] = primary;
		loaded[0] = 1;
	}
	int size() const { return (int)list.size(); }
	const FaceRef& operator[](int i)
	{
		if (!loaded[(size_t)i])
		{
			loaded[(size_t)i] = 1;
			list[(size_t)i] = load(i - 1);
		}
		return list[(size_t)i];
	}
};

/* One placed glyph of a line, in drawn order. */
struct LineGlyph {
	int face;          /* index into the FaceRef list */
	uint32_t gid;      /* glyph index in that face (0: no face had the character) */
	Character ch;      /* the character it came from (its cluster's first) */
	float x_off, y_off; /* pixels; y down (the page's direction) */
	float x_adv;       /* pixels */
};

/* A HarfBuzz font reading the face's own tables (hb-ot) at a pixel size, kept per face and size for the engine's life.
 * The FreeType face's variation (the variable fonts' named weight the page loaded) is carried over by hb_ft_font_create.
 * Keyed by the FreeType face, which lives until RmlUi shuts down. */
hb_font_t* ShapingFont(FT_Face face, int size)
{
	static std::unordered_map<uint64_t, hb_font_t*> s_fonts;
	const uint64_t key = (uint64_t)(uintptr_t)face * 1315423911u ^ (uint64_t)size;
	auto it = s_fonts.find(key);
	if (it != s_fonts.end())
		return it->second;
	hb_font_t* font = hb_ft_font_create_referenced(face);
	hb_ot_font_set_funcs(font);
	hb_font_set_scale(font, size * 64, size * 64); // 26.6 pixels, as the sample read FreeType's
	s_fonts.emplace(key, font);
	return font;
}

bool Rides(char32_t c)
{
	if (c == 0x200C || c == 0x200D || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0x1F3FB && c <= 0x1F3FF) || (c >= 0xE0020 && c <= 0xE007F) ||
		(c >= 0xE0100 && c <= 0xE01EF) || c == 0x20E3)
		return true;
	const hb_unicode_general_category_t g = hb_unicode_general_category(hb_unicode_funcs_get_default(), c);
	return g == HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK || g == HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK ||
		g == HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK;
}

/* Which face draws character c: the page's font if it has it, else the first fallback that has it (cached per
 * character: coverage doesn't depend on the size). -1: none. */
int FaceFor(Faces& faces, char32_t c)
{
	if (faces[0].ft && FT_Get_Char_Index(faces[0].ft, c) != 0)
		return 0;
	static std::unordered_map<char32_t, int> s_fallback; // index into faces (fallbacks start at 1)
	auto it = s_fallback.find(c);
	if (it != s_fallback.end() && it->second < faces.size())
	{
		if (it->second > 0)
			(void)faces[it->second]; // fetch its handle at this size
		return it->second;
	}
	int found = -1;
	for (int i = 1; i < faces.size(); ++i)
		if (faces[i].ft && FT_Get_Char_Index(faces[i].ft, c) != 0)
		{
			found = i;
			break;
		}
	s_fallback[c] = found;
	return found;
}

/* The line laid out: characters to faces, SheenBidi's level runs in drawn order, each run cut where its face changes and
 * shaped by HarfBuzz in the run's direction with the whole line as context. */
std::vector<LineGlyph> ShapeLine(StringView string, Faces& faces, int size, const TextShapingContext& ctx)
{
	std::vector<LineGlyph> out;
	std::vector<char32_t> cps;
	for (const char* p = string.begin(); p < string.end();)
	{
		const Character c = Rml::StringUtilities::ToCharacter(p, string.end());
		cps.push_back((char32_t)c);
		p += std::max<size_t>(1, Rml::StringUtilities::BytesUTF8(c));
	}
	const int n = (int)cps.size();
	if (n == 0)
		return out;

	// faces per character: a mark, joiner or emoji modifier stays with the character before it, and so does the
	// character after a zero-width joiner when that face has it (an emoji family stays one run). A letter with things
	// riding on it goes to the first face that has the letter and every rider that shows (seen 2026-11-16: a thumbs-up
	// is also in Noto Sans Symbols 2, black and white, which lacks the skin tones, so 👍🏽 drew as two pictures until
	// the emoji font, which has both, was preferred). Variation selectors and joiners don't count: HarfBuzz hides them.
	auto has = [&](int f, char32_t c) { return faces[f].ft && FT_Get_Char_Index(faces[f].ft, c) != 0; };
	auto shows = [](char32_t c) { return !(c == 0x200C || c == 0x200D || (c >= 0xFE00 && c <= 0xFE0F) || (c >= 0xE0100 && c <= 0xE01EF)); };
	std::vector<int> face(n, 0);
	for (int i = 0; i < n; ++i)
	{
		if (i > 0 && (Rides(cps[i]) || (cps[i - 1] == 0x200D && has(face[i - 1], cps[i]))))
		{
			face[i] = face[i - 1];
			continue;
		}
		int f = FaceFor(faces, cps[i]);
		int end = i + 1;
		while (end < n && Rides(cps[end]))
			++end;
		bool all = true;
		for (int j = i + 1; j < end && f >= 0; ++j)
			all = all && (!shows(cps[j]) || has(f, cps[j]));
		if (f >= 0 && !all)
			for (int k = 0; k < faces.size(); ++k)
			{
				if (!has(k, cps[i]))
					continue;
				bool every = true;
				for (int j = i + 1; j < end; ++j)
					every = every && (!shows(cps[j]) || has(k, cps[j]));
				if (every)
				{
					f = k;
					break;
				}
			}
		face[i] = f < 0 ? 0 : f; // no face has it: the page's font (its missing-glyph handling)
	}

	// the bidi runs (rules P2-L2 for one paragraph). The paragraph's direction: the element's `dir` when set, else
	// left to right, the direction of every language OrrePort's pages are in. Not "from the first strong letter":
	// seen 2026-11-16, a Saves row "محمد سلام · 14:46 played · ..." then read as a right-to-left line and its English
	// parts swapped places. A right-to-left translation of the pages would set dir="rtl" on its documents.
	SBLevel base = 0;
	if (ctx.text_direction == Style::Direction::Rtl)
		base = 1;
	const SBCodepointSequence seq = {SBStringEncodingUTF32, cps.data(), (SBUInteger)n};
	SBAlgorithmRef algo = SBAlgorithmCreate(&seq);
	SBParagraphRef para = algo ? SBAlgorithmCreateParagraph(algo, 0, (SBUInteger)n, base) : nullptr;
	SBLineRef line = para ? SBParagraphCreateLine(para, 0, SBParagraphGetLength(para)) : nullptr;
	struct Run {
		int a, b;
		bool rtl;
	};
	std::vector<Run> runs;
	if (line)
	{
		const SBRun* r = SBLineGetRunsPtr(line);
		for (SBUInteger i = 0; i < SBLineGetRunCount(line); ++i)
			runs.push_back({(int)r[i].offset, (int)(r[i].offset + r[i].length), (r[i].level & 1) != 0});
		// a paragraph ends at a paragraph separator; anything after it (not expected in one line) keeps its order
		if (SBParagraphGetLength(para) < (SBUInteger)n)
			runs.push_back({(int)SBParagraphGetLength(para), n, false});
	}
	else
		runs.push_back({0, n, false});
	if (line)
		SBLineRelease(line);
	if (para)
		SBParagraphRelease(para);
	if (algo)
		SBAlgorithmRelease(algo);

	const Vector<hb_feature_t> features = GetTextShapingFeatures(ctx, size);
	hb_buffer_t* buf = hb_buffer_create();
	for (const Run& run : runs)
	{
		// the run's pieces of one face, in drawn order
		std::vector<std::pair<int, int>> pieces;
		for (int i = run.a; i < run.b;)
		{
			int j = i + 1;
			while (j < run.b && face[j] == face[i])
				++j;
			pieces.push_back({i, j});
			i = j;
		}
		if (run.rtl)
			std::reverse(pieces.begin(), pieces.end());
		for (const auto& piece : pieces)
		{
			const FaceRef& fr = faces[face[piece.first]];
			if (!fr.ft)
				continue;
			hb_buffer_clear_contents(buf);
			hb_buffer_add_codepoints(buf, reinterpret_cast<const hb_codepoint_t*>(cps.data()), n, (unsigned)piece.first,
				piece.second - piece.first);
			hb_buffer_set_direction(buf, run.rtl ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
			if (!ctx.language.empty())
				hb_buffer_set_language(buf, hb_language_from_string(ctx.language.c_str(), -1));
			hb_buffer_guess_segment_properties(buf);
			hb_shape(ShapingFont(fr.ft, size), buf, features.empty() ? nullptr : features.data(), (unsigned)features.size());
			unsigned count = 0;
			const hb_glyph_info_t* info = hb_buffer_get_glyph_infos(buf, &count);
			const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(buf, nullptr);
			for (unsigned g = 0; g < count; ++g)
			{
				const Character ch = (Character)cps[info[g].cluster];
				if (IsControlCharacter(ch))
					continue;
				out.push_back({face[piece.first], info[g].codepoint, ch, pos[g].x_offset / 64.f, -pos[g].y_offset / 64.f,
					pos[g].x_advance / 64.f});
			}
		}
	}
	hb_buffer_destroy(buf);
	return out;
}

} // namespace

FontFaceHandleHarfBuzz::FontFaceHandleHarfBuzz()
{
	base_layer = nullptr;
	metrics = {};
	ft_face = 0;
	hb_font = nullptr;
}

FontFaceHandleHarfBuzz::~FontFaceHandleHarfBuzz()
{
	hb_font_destroy(hb_font);

	glyphs.clear();
	layers.clear();
}

bool FontFaceHandleHarfBuzz::Initialize(FontFaceHandleFreetype face, int font_size, bool load_default_glyphs)
{
	ft_face = face;

	RMLUI_ASSERTMSG(layer_configurations.empty(), "Initialize must only be called once.");

	if (!FreeType::InitialiseFaceHandle(ft_face, font_size, glyphs, metrics, load_default_glyphs))
		return false;

	hb_font = hb_ft_font_create_referenced((FT_Face)ft_face);
	RMLUI_ASSERT(hb_font != nullptr);
	hb_font_set_ptem(hb_font, (float)font_size);
	hb_ft_font_set_funcs(hb_font);

	// Generate the default layer and layer configuration.
	base_layer = GetOrCreateLayer(nullptr);
	layer_configurations.push_back(LayerConfiguration{base_layer});

	return true;
}

const FontMetrics& FontFaceHandleHarfBuzz::GetFontMetrics() const
{
	return metrics;
}

const FontGlyphMap& FontFaceHandleHarfBuzz::GetGlyphs() const
{
	return glyphs;
}

const FallbackFontGlyphMap& FontFaceHandleHarfBuzz::GetFallbackGlyphs() const
{
	return fallback_glyphs;
}

const FallbackFontClusterGlyphsMap& FontFaceHandleHarfBuzz::GetFallbackClusterGlyphs() const
{
	return fallback_cluster_glyphs;
}

/* OrrePort: for each placed glyph, the bitmap to draw: from this face's glyphs, or a fallback face's glyph copied in as
 * a "cluster" glyph keyed by (glyph index, a stand-in character 0x110000 + the face's place in the list), so two faces'
 * glyphs with one index never collide (the sample keyed fallback glyphs by character, which a shaped glyph, a joined
 * Arabic form or a ligature, doesn't have one of its own). */
namespace {
struct ResolvedGlyph {
	FontGlyphIndex index;
	Character character;
	bool is_cluster;
	const FontGlyph* bitmap;
};
} // namespace

int FontFaceHandleHarfBuzz::GetStringWidth(StringView string, const TextShapingContext& text_shaping_context,
	const LanguageDataMap& /*registered_languages*/, Character /*prior_character*/)
{
	// a lambda in a member function may read another handle's FreeType face (private), as the sample's fallback code does
	Faces faces({(FT_Face)ft_face, this}, FontProvider::CountFallbackFontFaces(), [this](int i) {
		FontFaceHandleHarfBuzz* f = FontProvider::GetFallbackFontFace(i, metrics.size);
		return FaceRef{f && f != this ? (FT_Face)f->ft_face : nullptr, f};
	});
	const std::vector<LineGlyph> line = ShapeLine(string, faces, metrics.size, text_shaping_context);

	float width = 0;
	for (const LineGlyph& g : line)
	{
		// the same glyph lookup as GenerateString, so new letters join the textures before they are drawn
		Character c = g.ch;
		const FontGlyph* bitmap = nullptr;
		if (g.face == 0)
			bitmap = GetOrAppendGlyph(g.gid, c, g.gid == 0);
		else
		{
			const Character synth = (Character)(0x110000u + (unsigned)g.face);
			const uint64_t id = GetFallbackFontClusterGlyphLookupID(g.gid, synth);
			if (fallback_cluster_glyphs_lookup.find(id) == fallback_cluster_glyphs_lookup.end())
			{
				FontFaceHandleHarfBuzz* fb = faces[g.face].handle;
				const FontGlyph* fg = fb ? fb->GetOrAppendGlyph(g.gid, c, false) : nullptr;
				if (fg)
				{
					const String key = "\x01orre:" + std::to_string(g.face) + ":" + std::to_string(g.gid);
					Vector<FontClusterGlyphData> one;
					one.push_back(FontClusterGlyphData{g.gid, FontGlyphData{fg->WeakCopy(), synth}});
					auto pair = fallback_cluster_glyphs.emplace(key, std::move(one));
					fallback_cluster_glyphs_lookup.emplace(id, &pair.first->second[0].glyph_data.bitmap);
					is_layers_dirty = true;
				}
			}
		}
		(void)bitmap;
		width += g.x_adv + text_shaping_context.letter_spacing;
	}
	return Rml::Math::Max((int)std::lround(width), 0);
}

int FontFaceHandleHarfBuzz::GenerateLayerConfiguration(const FontEffectList& font_effects)
{
	if (font_effects.empty())
		return 0;

	// Check each existing configuration for a match with this arrangement of effects.
	int configuration_index = 1;
	for (; configuration_index < (int)layer_configurations.size(); ++configuration_index)
	{
		const LayerConfiguration& configuration = layer_configurations[configuration_index];

		// Check the size is correct. For a match, there should be one layer in the configuration
		// plus an extra for the base layer.
		if (configuration.size() != font_effects.size() + 1)
			continue;

		// Check through each layer, checking it was created by the same effect as the one we're
		// checking.
		size_t effect_index = 0;
		for (size_t i = 0; i < configuration.size(); ++i)
		{
			// Skip the base layer ...
			if (configuration[i]->GetFontEffect() == nullptr)
				continue;

			// If the ith layer's effect doesn't match the equivalent effect, then this
			// configuration can't match.
			if (configuration[i]->GetFontEffect() != font_effects[effect_index].get())
				break;

			// Check the next one ...
			++effect_index;
		}

		if (effect_index == font_effects.size())
			return configuration_index;
	}

	// No match, so we have to generate a new layer configuration.
	layer_configurations.push_back(LayerConfiguration());
	LayerConfiguration& layer_configuration = layer_configurations.back();

	bool added_base_layer = false;

	for (size_t i = 0; i < font_effects.size(); ++i)
	{
		if (!added_base_layer && font_effects[i]->GetLayer() == FontEffect::Layer::Front)
		{
			layer_configuration.push_back(base_layer);
			added_base_layer = true;
		}

		FontFaceLayer* new_layer = GetOrCreateLayer(font_effects[i]);
		layer_configuration.push_back(new_layer);
	}

	// Add the base layer now if we still haven't added it.
	if (!added_base_layer)
		layer_configuration.push_back(base_layer);

	return (int)(layer_configurations.size() - 1);
}

bool FontFaceHandleHarfBuzz::GenerateLayerTexture(Vector<byte>& texture_data, Vector2i& texture_dimensions, const FontEffect* font_effect,
	int texture_id, int handle_version) const
{
	if (handle_version != version)
	{
		RMLUI_ERRORMSG("While generating font layer texture: Handle version mismatch in texture vs font-face.");
		return false;
	}

	auto it = std::find_if(layers.begin(), layers.end(), [font_effect](const EffectLayerPair& pair) { return pair.font_effect == font_effect; });

	if (it == layers.end())
	{
		RMLUI_ERRORMSG("While generating font layer texture: Layer id not found.");
		return false;
	}

	return it->layer->GenerateTexture(texture_data, texture_dimensions, texture_id,
		FontGlyphMaps{&glyphs, &fallback_glyphs, &fallback_cluster_glyphs_lookup});
}

int FontFaceHandleHarfBuzz::GenerateString(RenderManager& render_manager, TexturedMeshList& mesh_list, StringView string, const Vector2f position,
	const ColourbPremultiplied colour, const float opacity, const TextShapingContext& text_shaping_context,
	const LanguageDataMap& registered_languages, const int layer_configuration_index)
{
	RMLUI_ASSERT(layer_configuration_index >= 0);
	RMLUI_ASSERT(layer_configuration_index < (int)layer_configurations.size());

	// OrrePort: lay the line out and find every glyph first (GetStringWidth adds any new one to the textures), then
	// rebuild the textures once, then make the geometry.
	GetStringWidth(string, text_shaping_context, registered_languages);
	Faces faces({(FT_Face)ft_face, this}, FontProvider::CountFallbackFontFaces(), [this](int i) {
		FontFaceHandleHarfBuzz* f = FontProvider::GetFallbackFontFace(i, metrics.size);
		return FaceRef{f && f != this ? (FT_Face)f->ft_face : nullptr, f};
	});
	const std::vector<LineGlyph> line = ShapeLine(string, faces, metrics.size, text_shaping_context);
	std::vector<ResolvedGlyph> resolved;
	resolved.reserve(line.size());
	for (const LineGlyph& g : line)
	{
		ResolvedGlyph r{g.gid, g.ch, false, nullptr};
		if (g.face == 0)
			r.bitmap = GetOrAppendGlyph(r.index, r.character, g.gid == 0);
		else
		{
			r.character = (Character)(0x110000u + (unsigned)g.face);
			r.is_cluster = true;
			auto it = fallback_cluster_glyphs_lookup.find(GetFallbackFontClusterGlyphLookupID(g.gid, r.character));
			if (it != fallback_cluster_glyphs_lookup.end())
				r.bitmap = it->second;
		}
		resolved.push_back(r);
	}

	UpdateLayersOnDirty();

	int geometry_index = 0;
	float line_width = 0;

	// Fetch the requested configuration and generate the geometry for each one.
	const LayerConfiguration& layer_configuration = layer_configurations[layer_configuration_index];

	// Each texture represents one geometry.
	const int num_geometries = std::accumulate(layer_configuration.begin(), layer_configuration.end(), 0,
		[](int sum, const FontFaceLayer* layer) { return sum + layer->GetNumTextures(); });

	mesh_list.resize(num_geometries);

	for (size_t layer_index = 0; layer_index < layer_configuration.size(); ++layer_index)
	{
		FontFaceLayer* layer = layer_configuration[layer_index];

		ColourbPremultiplied layer_colour;
		if (layer == base_layer)
			layer_colour = colour;
		else
			layer_colour = layer->GetColour(opacity);

		const int num_textures = layer->GetNumTextures();
		if (num_textures == 0)
			continue;

		RMLUI_ASSERT(geometry_index + num_textures <= (int)mesh_list.size());

		line_width = 0;

		// Set the mesh and textures to the geometries.
		for (int tex_index = 0; tex_index < num_textures; ++tex_index)
			mesh_list[geometry_index + tex_index].texture = layer->GetTexture(render_manager, tex_index);

		mesh_list[geometry_index].mesh.indices.reserve(line.size() * 6);
		mesh_list[geometry_index].mesh.vertices.reserve(line.size() * 4);

		for (size_t i = 0; i < line.size(); ++i)
		{
			const LineGlyph& g = line[i];
			const ResolvedGlyph& r = resolved[i];
			if (r.bitmap)
			{
				ColourbPremultiplied glyph_color = layer_colour;
				// Use white vertex colors on RGB glyphs.
				if (layer == base_layer && r.bitmap->color_format == ColorFormat::RGBA8)
					glyph_color = ColourbPremultiplied(layer_colour.alpha, layer_colour.alpha);

				const Vector2f glyph_geometry_position = Vector2f{position.x + line_width + g.x_off, position.y + g.y_off};
				layer->GenerateGeometry(&mesh_list[geometry_index], r.index, r.character, r.is_cluster, glyph_geometry_position, glyph_color);
			}
			line_width += g.x_adv + text_shaping_context.letter_spacing;
		}

		geometry_index += num_textures;
	}

	return Rml::Math::Max((int)std::lround(line_width), 0);
}

bool FontFaceHandleHarfBuzz::UpdateLayersOnDirty()
{
	bool result = false;

	// If we are dirty, regenerate all the layers and increment the version
	if (is_layers_dirty && base_layer)
	{
		is_layers_dirty = false;
		++version;

		// Regenerate all the layers.
		// Note: The layer regeneration needs to happen in the order in which the layers were created,
		// otherwise we may end up cloning a layer which has not yet been regenerated. This means trouble!
		for (auto& pair : layers)
		{
			GenerateLayer(pair.layer.get());
		}

		result = true;
	}

	return result;
}

int FontFaceHandleHarfBuzz::GetVersion() const
{
	return version;
}

bool FontFaceHandleHarfBuzz::AppendGlyph(FontGlyphIndex glyph_index, Character character)
{
	bool result = FreeType::AppendGlyph(ft_face, metrics.size, glyph_index, character, glyphs);
	return result;
}

bool FontFaceHandleHarfBuzz::AppendFallbackGlyph(Character& character)
{
	const int num_fallback_faces = FontProvider::CountFallbackFontFaces();
	for (int i = 0; i < num_fallback_faces; i++)
	{
		FontFaceHandleHarfBuzz* fallback_face = FontProvider::GetFallbackFontFace(i, metrics.size);
		if (!fallback_face || fallback_face == this)
			continue;

		const FontGlyphIndex character_index = FreeType::GetGlyphIndexFromCharacter(fallback_face->ft_face, character);
		if (character_index == 0)
			continue;

		const FontGlyph* glyph = fallback_face->GetOrAppendGlyph(character_index, character, false);
		if (glyph)
		{
			// Insert the new glyph into our own set of fallback glyphs
			auto pair = fallback_glyphs.emplace(character, glyph->WeakCopy());
			if (pair.second)
				is_layers_dirty = true;
			return true;
		}
	}

	return false;
}

const FontGlyph* FontFaceHandleHarfBuzz::GetOrAppendGlyph(FontGlyphIndex glyph_index, Character& character, bool look_in_fallback_fonts)
{
	if (glyph_index == 0 && look_in_fallback_fonts && character != Character::Replacement)
	{
		auto fallback_glyph = GetOrAppendFallbackGlyph(character);
		if (fallback_glyph != nullptr)
			return fallback_glyph;
	}

	auto glyph_location = glyphs.find(glyph_index);
	if (glyph_location == glyphs.cend())
	{
		bool result = false;
		if (glyph_index != 0)
			result = AppendGlyph(glyph_index, character);

		if (result)
		{
			glyph_location = glyphs.find(glyph_index);
			if (glyph_location == glyphs.cend())
			{
				RMLUI_ERROR;
				return nullptr;
			}

			is_layers_dirty = true;
		}
		else
			return nullptr;
	}

	if (glyph_index == 0)
		character = Character::Replacement;
	else if (character != glyph_location->second.character)
		character = glyph_location->second.character;

	const FontGlyph* glyph = &glyph_location->second.bitmap;
	return glyph;
}

const FontGlyph* FontFaceHandleHarfBuzz::GetOrAppendFallbackGlyph(Character& character)
{
	auto fallback_glyph_location = fallback_glyphs.find(character);
	if (fallback_glyph_location != fallback_glyphs.cend())
		return &fallback_glyph_location->second;

	bool result = AppendFallbackGlyph(character);

	if (result)
	{
		fallback_glyph_location = fallback_glyphs.find(character);
		if (fallback_glyph_location == fallback_glyphs.cend())
		{
			RMLUI_ERROR;
			return nullptr;
		}

		is_layers_dirty = true;
	}
	else
		return nullptr;

	const FontGlyph* fallback_glyph = &fallback_glyph_location->second;
	return fallback_glyph;
}

FontFaceLayer* FontFaceHandleHarfBuzz::GetOrCreateLayer(const SharedPtr<const FontEffect>& font_effect)
{
	// Search for the font effect layer first, it may have been instanced before as part of a different configuration.
	const FontEffect* font_effect_ptr = font_effect.get();
	auto it =
		std::find_if(layers.begin(), layers.end(), [font_effect_ptr](const EffectLayerPair& pair) { return pair.font_effect == font_effect_ptr; });

	if (it != layers.end())
		return it->layer.get();

	// No existing effect matches, generate a new layer for the effect.
	layers.push_back(EffectLayerPair{font_effect_ptr, nullptr});
	auto& layer = layers.back().layer;

	layer = Rml::MakeUnique<FontFaceLayer>(font_effect);
	GenerateLayer(layer.get());

	return layer.get();
}

bool FontFaceHandleHarfBuzz::GenerateLayer(FontFaceLayer* layer)
{
	RMLUI_ASSERT(layer);
	const FontEffect* font_effect = layer->GetFontEffect();
	bool result = false;

	if (!font_effect)
	{
		result = layer->Generate(this);
	}
	else
	{
		// Determine which, if any, layer the new layer should copy its geometry and textures from.
		FontFaceLayer* clone = nullptr;
		bool clone_glyph_origins = true;
		String generation_key;
		size_t fingerprint = font_effect->GetFingerprint();

		if (!font_effect->HasUniqueTexture())
		{
			clone = base_layer;
			clone_glyph_origins = false;
		}
		else
		{
			auto cache_iterator = layer_cache.find(fingerprint);
			if (cache_iterator != layer_cache.end() && cache_iterator->second != layer)
				clone = cache_iterator->second;
		}

		// Create a new layer.
		result = layer->Generate(this, clone, clone_glyph_origins);

		// Cache the layer in the layer cache if it generated its own textures (ie, didn't clone).
		if (!clone)
			layer_cache[fingerprint] = layer;
	}

	return result;
}
