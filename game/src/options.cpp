#include "options.hpp"

#include <algorithm>

namespace game
{
	Settings ChangeSetting(Settings settings, OptionItem item, int direction) noexcept
	{
		switch (item)
		{
		case OptionItem::Sound:
			settings.sound = !settings.sound;
			break;
		case OptionItem::Lives:
			settings.lives = std::clamp(settings.lives + (direction < 0 ? -1 : 1), kMinLives, kMaxLives);
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
		case OptionItem::Lives:
			return "LIVES: " + std::to_string(settings.lives);
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
		if (const std::optional<int> lives = data.GetInt(kLivesKey))
		{
			if (*lives >= kMinLives && *lives <= kMaxLives) settings.lives = *lives;
		}
		return settings;
	}

	void StoreSettings(engine::core::SaveData& data, const Settings& settings)
	{
		data.SetInt(kSoundKey, settings.sound ? 1 : 0);
		data.SetInt(kLivesKey, settings.lives);
	}
} // namespace game
