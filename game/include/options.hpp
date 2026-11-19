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

	inline constexpr int kMinVolume = 0;
	inline constexpr int kMaxVolume = 10;
	inline constexpr int kDefaultVolume = 8;

	inline constexpr const char* kSoundKey = "sound";
	inline constexpr const char* kMusicKey = "music";
	inline constexpr const char* kVolumeKey = "volume";
	inline constexpr const char* kLivesKey = "lives";
	inline constexpr const char* kDisplayKey = "display";
	inline constexpr const char* kScalingKey = "scaling";

	// Resource: what the player has chosen.
	struct Settings
	{
		bool sound = true;           // false mutes every sound effect
		bool music = true;           // false stops the music
		int volume = kDefaultVolume; // the master volume, kMinVolume to kMaxVolume
		int lives = kStartingLives;  // tries at the start of a game
		engine::graphics::WindowMode display = engine::graphics::WindowMode::Windowed;
		engine::graphics::Scaling scaling = engine::graphics::Scaling::Integer; // whole-number or any-size scaling
	};

	enum class OptionItem : std::uint8_t
	{
		Sound,
		Music,
		Volume,
		Lives,
		Display, // windowed, borderless or fullscreen
		Scale,   // integer or fit scaling
		UiDemo,  // opens the UI demo screen (uidemo.hpp)
		Back,
		Count
	};

	inline constexpr std::size_t kOptionItems = static_cast<std::size_t>(OptionItem::Count);

	// The master volume as a 0 to 1 gain.
	[[nodiscard]] constexpr float VolumeGain(const Settings& settings) noexcept
	{
		return static_cast<float>(settings.volume) / static_cast<float>(kMaxVolume);
	}

	// The settings after pressing left (direction -1) or right or Enter (+1) on item. Sound
	// and Music toggle either way; Volume and Lives step and stops at their limits; Display cycles
	// through the window modes (wrapping) and Scale toggles; the rest change nothing.
	[[nodiscard]] Settings ChangeSetting(Settings settings, OptionItem item, int direction) noexcept;

	// The text of an item without its cursor: "SOUND: ON", "VOLUME: 8", "LIVES: 3", "BACK".
	[[nodiscard]] std::string OptionText(OptionItem item, const Settings& settings);

	// The settings saved in data; whatever is missing or out of range stays at its default.
	[[nodiscard]] Settings SettingsFrom(const engine::core::SaveData& data);

	// Writes the settings into data, under the keys above.
	void StoreSettings(engine::core::SaveData& data, const Settings& settings);
} // namespace game
