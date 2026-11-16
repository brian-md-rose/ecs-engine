#include <catch2/catch_test_macros.hpp>

#include "core/savedata.hpp"
#include "options.hpp"

namespace
{
	using game::OptionItem;
	using game::Settings;
} // namespace

TEST_CASE("The default settings have sound on and the standard lives", "[game][options]")
{
	const Settings settings;
	CHECK(settings.sound);
	CHECK(settings.lives == game::kStartingLives);
}

TEST_CASE("Sound toggles whichever way it is changed", "[game][options]")
{
	Settings settings;
	settings = game::ChangeSetting(settings, OptionItem::Sound, 1);
	CHECK_FALSE(settings.sound);
	settings = game::ChangeSetting(settings, OptionItem::Sound, -1);
	CHECK(settings.sound);
}

TEST_CASE("Lives step up and down and stop at the limits", "[game][options]")
{
	Settings settings{.lives = 3};
	CHECK(game::ChangeSetting(settings, OptionItem::Lives, 1).lives == 4);
	CHECK(game::ChangeSetting(settings, OptionItem::Lives, -1).lives == 2);
	CHECK(game::ChangeSetting(Settings{.lives = game::kMaxLives}, OptionItem::Lives, 1).lives == game::kMaxLives);
	CHECK(game::ChangeSetting(Settings{.lives = game::kMinLives}, OptionItem::Lives, -1).lives == game::kMinLives);
}

TEST_CASE("Changing BACK, or another item, leaves the rest alone", "[game][options]")
{
	const Settings settings{.sound = false, .lives = 5};
	const Settings same = game::ChangeSetting(settings, OptionItem::Back, 1);
	CHECK_FALSE(same.sound);
	CHECK(same.lives == 5);
	CHECK(game::ChangeSetting(settings, OptionItem::Sound, 1).lives == 5);
	CHECK_FALSE(game::ChangeSetting(settings, OptionItem::Lives, 1).sound);
}

TEST_CASE("OptionText spells out each item", "[game][options]")
{
	const Settings settings{.sound = false, .lives = 4};
	CHECK(game::OptionText(OptionItem::Sound, settings) == "SOUND: OFF");
	CHECK(game::OptionText(OptionItem::Sound, Settings{}) == "SOUND: ON");
	CHECK(game::OptionText(OptionItem::Lives, settings) == "LIVES: 4");
	CHECK(game::OptionText(OptionItem::Back, settings) == "BACK");
}

TEST_CASE("Settings are stored in save data and read back", "[game][options]")
{
	engine::core::SaveData data;
	game::StoreSettings(data, Settings{.sound = false, .lives = 7});
	CHECK(data.GetInt(game::kSoundKey) == 0);
	CHECK(data.GetInt(game::kLivesKey) == 7);

	const Settings back = game::SettingsFrom(data);
	CHECK_FALSE(back.sound);
	CHECK(back.lives == 7);
}

TEST_CASE("Missing or out-of-range saved settings fall back to the defaults", "[game][options]")
{
	CHECK(game::SettingsFrom({}).sound);
	CHECK(game::SettingsFrom({}).lives == game::kStartingLives);

	engine::core::SaveData data;
	data.SetInt(game::kLivesKey, 0);
	CHECK(game::SettingsFrom(data).lives == game::kStartingLives);
	data.SetInt(game::kLivesKey, game::kMaxLives + 1);
	CHECK(game::SettingsFrom(data).lives == game::kStartingLives);
	data.SetInt(game::kLivesKey, game::kMaxLives);
	CHECK(game::SettingsFrom(data).lives == game::kMaxLives);
}
