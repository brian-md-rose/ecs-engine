#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

#include "core/savedata.hpp"
#include "gameflow.hpp"
#include "graphics/display.hpp"

// The options screen's settings, as pure functions over a Settings value: no registry, no SDL.
// They are saved in save.txt next to the high score (see save.hpp).
namespace game
{
	inline constexpr int kMinLives = 1;
	inline constexpr int kMaxLives = 9;

	inline constexpr const char* kSoundKey = "sound";
	inline constexpr const char* kLivesKey = "lives";
	inline constexpr const char* kDisplayKey = "display";
	inline constexpr const char* kScalingKey = "scaling";

	// Resource: what the player has chosen.
	struct Settings
	{
		bool sound = true;          // false mutes every sound effect
		int lives = kStartingLives; // tries at the start of a game
		engine::graphics::WindowMode display = engine::graphics::WindowMode::Windowed;
		engine::graphics::Scaling scaling = engine::graphics::Scaling::Integer; // whole-number or any-size scaling
	};

	enum class OptionItem : std::uint8_t
	{
		Sound,
		Lives,
		Display, // windowed, borderless or fullscreen
		Scale,   // integer or fit scaling
		UiDemo,  // opens the UI demo screen (uidemo.hpp)
		Back,
		Count
	};

	inline constexpr std::size_t kOptionItems = static_cast<std::size_t>(OptionItem::Count);

	// The settings after pressing left (direction -1) or right or Enter (+1) on item. Sound
	// toggles either way; Lives steps and stops at kMinLives and kMaxLives; Display cycles
	// through the window modes (wrapping) and Scale toggles; the rest change nothing.
	[[nodiscard]] Settings ChangeSetting(Settings settings, OptionItem item, int direction) noexcept;

	// The text of an item without its cursor: "SOUND: ON", "LIVES: 3", "BACK".
	[[nodiscard]] std::string OptionText(OptionItem item, const Settings& settings);

	// The settings saved in data; whatever is missing or out of range stays at its default.
	[[nodiscard]] Settings SettingsFrom(const engine::core::SaveData& data);

	// Writes the settings into data, under kSoundKey and kLivesKey.
	void StoreSettings(engine::core::SaveData& data, const Settings& settings);
} // namespace game
