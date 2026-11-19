#include "options.hpp"

#include <algorithm>

namespace game
{
	namespace
	{
		using engine::graphics::Scaling;
		using engine::graphics::WindowMode;
	} // namespace

	Settings ChangeSetting(Settings settings, OptionItem item, int direction) noexcept
	{
		switch (item)
		{
		case OptionItem::Sound:
			settings.sound = !settings.sound;
			break;
		case OptionItem::Music:
			settings.music = !settings.music;
			break;
		case OptionItem::Volume:
			settings.volume = std::clamp(settings.volume + (direction < 0 ? -1 : 1), kMinVolume, kMaxVolume);
			break;
		case OptionItem::Lives:
			settings.lives = std::clamp(settings.lives + (direction < 0 ? -1 : 1), kMinLives, kMaxLives);
			break;
		case OptionItem::Display:
		{
			constexpr int kModes = 3;
			const int next = (static_cast<int>(settings.display) + (direction < 0 ? kModes - 1 : 1)) % kModes;
			settings.display = static_cast<WindowMode>(next);
			break;
		}
		case OptionItem::Scale:
			settings.scaling = settings.scaling == Scaling::Integer ? Scaling::Fit : Scaling::Integer;
			break;
		case OptionItem::UiDemo:
		case OptionItem::Back:
		case OptionItem::Count:
			break;
		}
		return settings;
	}

	std::string OptionText(OptionItem item, const Settings& settings)
	{
		switch (item)
		{
		case OptionItem::Sound:
			return settings.sound ? "SOUND: ON" : "SOUND: OFF";
		case OptionItem::Music:
			return settings.music ? "MUSIC: ON" : "MUSIC: OFF";
		case OptionItem::Volume:
			return "VOLUME: " + std::to_string(settings.volume);
		case OptionItem::Lives:
			return "LIVES: " + std::to_string(settings.lives);
		case OptionItem::Display:
			switch (settings.display)
			{
			case WindowMode::Windowed:
				return "DISPLAY: WINDOWED";
			case WindowMode::Borderless:
				return "DISPLAY: BORDERLESS";
			case WindowMode::Fullscreen:
				return "DISPLAY: FULLSCREEN";
			}
			break;
		case OptionItem::Scale:
			return settings.scaling == Scaling::Integer ? "SCALING: INTEGER" : "SCALING: FIT";
		case OptionItem::UiDemo:
			return "UI DEMO";
		case OptionItem::Back:
			return "BACK";
		case OptionItem::Count:
			break;
		}
		return {};
	}

	Settings SettingsFrom(const engine::core::SaveData& data)
	{
		Settings settings;
		if (const std::optional<int> sound = data.GetInt(kSoundKey)) settings.sound = *sound != 0;
		if (const std::optional<int> music = data.GetInt(kMusicKey)) settings.music = *music != 0;
		if (const std::optional<int> volume = data.GetInt(kVolumeKey))
		{
			if (*volume >= kMinVolume && *volume <= kMaxVolume) settings.volume = *volume;
		}
		if (const std::optional<int> lives = data.GetInt(kLivesKey))
		{
			if (*lives >= kMinLives && *lives <= kMaxLives) settings.lives = *lives;
		}
		if (const std::optional<int> display = data.GetInt(kDisplayKey))
		{
			if (*display >= 0 && *display <= static_cast<int>(WindowMode::Fullscreen))
			{
				settings.display = static_cast<WindowMode>(*display);
			}
		}
		if (const std::optional<int> scaling = data.GetInt(kScalingKey))
		{
			if (*scaling >= 0 && *scaling <= static_cast<int>(Scaling::Fit))
				settings.scaling = static_cast<Scaling>(*scaling);
		}
		return settings;
	}

	void StoreSettings(engine::core::SaveData& data, const Settings& settings)
	{
		data.SetInt(kSoundKey, settings.sound ? 1 : 0);
		data.SetInt(kMusicKey, settings.music ? 1 : 0);
		data.SetInt(kVolumeKey, settings.volume);
		data.SetInt(kLivesKey, settings.lives);
		data.SetInt(kDisplayKey, static_cast<int>(settings.display));
		data.SetInt(kScalingKey, static_cast<int>(settings.scaling));
	}
} // namespace game
