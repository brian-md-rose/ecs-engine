#include <catch2/catch_test_macros.hpp>

#include "displaysettings.hpp"
#include "ecs/world.hpp"
#include "graphics/display.hpp"
#include "options.hpp"

namespace
{
	using engine::graphics::DisplayInfo;
	using engine::graphics::DisplaySettings;
	using engine::graphics::Resolution;
	using engine::graphics::Scaling;
	using engine::graphics::WindowMode;

	[[nodiscard]] DisplayInfo Info()
	{
		DisplayInfo info;
		info.current = {.mode = WindowMode::Windowed, .width = 672, .height = 864};
		info.resolutions = {{2560, 1440}, {1920, 1080}, {1280, 720}};
		return info;
	}
} // namespace

TEST_CASE("The options' display and scaling change as the buttons say", "[game][display]")
{
	game::Settings settings;
	CHECK(settings.display == WindowMode::Windowed);
	settings = game::ChangeSetting(settings, game::OptionItem::Display, 1);
	CHECK(settings.display == WindowMode::Borderless);
	settings = game::ChangeSetting(settings, game::OptionItem::Display, 1);
	CHECK(settings.display == WindowMode::Fullscreen);
	settings = game::ChangeSetting(settings, game::OptionItem::Display, 1);
	CHECK(settings.display == WindowMode::Windowed); // wraps
	settings = game::ChangeSetting(settings, game::OptionItem::Display, -1);
	CHECK(settings.display == WindowMode::Fullscreen);

	CHECK(settings.scaling == Scaling::Integer);
	settings = game::ChangeSetting(settings, game::OptionItem::Scale, 1);
	CHECK(settings.scaling == Scaling::Fit);
	settings = game::ChangeSetting(settings, game::OptionItem::Scale, -1);
	CHECK(settings.scaling == Scaling::Integer);

	CHECK(game::OptionText(game::OptionItem::Display, game::Settings{}) == "DISPLAY: WINDOWED");
	CHECK(game::OptionText(game::OptionItem::Display, {.display = WindowMode::Borderless}) == "DISPLAY: BORDERLESS");
	CHECK(game::OptionText(game::OptionItem::Display, {.display = WindowMode::Fullscreen}) == "DISPLAY: FULLSCREEN");
	CHECK(game::OptionText(game::OptionItem::Scale, game::Settings{}) == "SCALING: INTEGER");
	CHECK(game::OptionText(game::OptionItem::Scale, {.scaling = Scaling::Fit}) == "SCALING: FIT");
}

TEST_CASE("The display choices are saved and loaded, and bad values keep the defaults", "[game][display]")
{
	engine::core::SaveData data;
	game::StoreSettings(data, {.display = WindowMode::Fullscreen, .scaling = Scaling::Fit});
	const game::Settings loaded = game::SettingsFrom(data);
	CHECK(loaded.display == WindowMode::Fullscreen);
	CHECK(loaded.scaling == Scaling::Fit);

	engine::core::SaveData bad;
	bad.SetInt(game::kDisplayKey, 7);
	bad.SetInt(game::kScalingKey, -1);
	const game::Settings kept = game::SettingsFrom(bad);
	CHECK(kept.display == WindowMode::Windowed);
	CHECK(kept.scaling == Scaling::Integer);
}

TEST_CASE("DisplayRequestFor asks only for what differs", "[game][display]")
{
	const DisplayInfo info = Info();
	CHECK_FALSE(game::DisplayRequestFor(game::Settings{}, info).has_value()); // already windowed, integer

	const auto fit = game::DisplayRequestFor({.scaling = Scaling::Fit}, info);
	REQUIRE(fit.has_value());
	CHECK(fit->scaling == Scaling::Fit);
	CHECK(fit->mode == WindowMode::Windowed);
	CHECK(fit->width == 672);

	const auto borderless = game::DisplayRequestFor({.display = WindowMode::Borderless}, info);
	REQUIRE(borderless.has_value());
	CHECK(borderless->mode == WindowMode::Borderless);

	const auto fullscreen = game::DisplayRequestFor({.display = WindowMode::Fullscreen}, info);
	REQUIRE(fullscreen.has_value());
	CHECK(fullscreen->width == 2560); // the display's own size
	CHECK(fullscreen->height == 1440);
}

TEST_CASE("Going back to windowed restores the window's old size", "[game][display]")
{
	DisplayInfo info = Info();
	info.current = {.mode = WindowMode::Fullscreen, .width = 2560, .height = 1440};
	const auto back = game::DisplayRequestFor({}, info, Resolution{672, 864});
	REQUIRE(back.has_value());
	CHECK(back->mode == WindowMode::Windowed);
	CHECK(back->width == 672);
	CHECK(back->height == 864);
}

TEST_CASE("DisplaySystem asks once per change, and not again if the change was refused", "[game][display]")
{
	engine::ecs::World world;
	world.resources.Emplace<game::Settings>();
	world.resources.Emplace<game::DisplayAsked>();
	world.resources.Emplace<engine::graphics::DisplayRequest>();
	world.resources.Emplace<DisplayInfo>(Info());
	auto& request = world.resources.Get<engine::graphics::DisplayRequest>();
	const engine::ecs::FrameContext context{};

	game::DisplaySystem(world, context); // the saved settings are the defaults: nothing to ask
	CHECK_FALSE(request.pending.has_value());

	world.resources.Get<game::Settings>().display = WindowMode::Fullscreen;
	game::DisplaySystem(world, context);
	REQUIRE(request.pending.has_value());
	CHECK(request.pending->mode == WindowMode::Fullscreen);

	// The application refused (info.current still windowed): no second request for the same choice.
	request.pending.reset();
	game::DisplaySystem(world, context);
	CHECK_FALSE(request.pending.has_value());

	// Back to windowed asks for the framed size again.
	world.resources.Get<game::Settings>().display = WindowMode::Windowed;
	world.resources.Get<game::Settings>().scaling = Scaling::Fit;
	game::DisplaySystem(world, context);
	REQUIRE(request.pending.has_value());
	CHECK(request.pending->scaling == Scaling::Fit);
}
