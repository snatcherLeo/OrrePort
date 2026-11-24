#pragma once

#ifdef AURORA_ENABLE_RMLUI

#include <aurora/input.hpp>

#include <RmlUi/Core/Context.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace aurora::rmlui {

enum class InputType {
  Text,
  Number,
};

inline constexpr const char* TouchStartEvent = "touchstart";
inline constexpr const char* TouchMoveEvent = "touchmove";
inline constexpr const char* TouchEndEvent = "touchend";
inline constexpr const char* TouchCancelEvent = "touchcancel";

Rml::Context* get_context() noexcept;
bool is_initialized() noexcept;
void set_input_type(InputType type) noexcept;
void set_ui_scale(float scale) noexcept;
float get_ui_scale() noexcept;
void set_glass_light_dir(float x, float y) noexcept;

struct InputResult {
  bool handled = false;           // An element stopped propagation
  Rml::Element* target = nullptr; // The element hit, if any
};
InputResult process_input(const input::InputEvent& event) noexcept;

struct RuntimeTexture {
  uint32_t width = 0;
  uint32_t height = 0;
  std::span<const std::byte> rgba8;
  bool premultipliedAlpha = false;
  bool generateMipmaps = false;
};

using TextureProvider = std::function<std::optional<RuntimeTexture>(std::string_view)>;

void register_texture_provider(std::string scheme, TextureProvider provider);
void unregister_texture_provider(std::string_view scheme) noexcept;

// aurora: change (OrrePort, 2026-11-13): the app's translation of page text. RmlUi passes every text it reads from a
// page (and the page title) through SystemInterface::TranslateString; with a translator set, Aurora's system interface
// hands it on, so a page can say "[hub.play]" and the app fills in the player's language. None set: text as written.
using TextTranslator = std::function<void(std::string& translated, const std::string& input)>;
void set_text_translator(TextTranslator translator);

} // namespace aurora::rmlui

#endif
