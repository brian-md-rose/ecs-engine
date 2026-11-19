#include "displaysettings.hpp"

namespace game
{
	std::optional<engine::graphics::DisplaySettings> DisplayRequestFor(const Settings& settings,
	                                                                   const engine::graphics::DisplayInfo& info,
	                                                                   engine::graphics::Resolution windowed)
	{
		engine::graphics::DisplaySettings wanted = info.current;
		wanted.mode = settings.display;
		wanted.scaling = settings.scaling;
		if (wanted.mode == engine::graphics::WindowMode::Fullscreen && !info.resolutions.empty())
		{
			wanted.width = info.resolutions.front().width;
			wanted.height = info.resolutions.front().height;
		}
		if (wanted.mode == engine::graphics::WindowMode::Windowed && windowed.width > 0 && windowed.height > 0)
		{
			wanted.width = windowed.width;
			wanted.height = windowed.height;
		}
		if (wanted == info.current) return std::nullopt;
		return wanted;
	}

	void DisplaySystem(engine::ecs::World& world, const engine::ecs::FrameContext&)
	{
		const Settings& settings = world.resources.Get<Settings>();
		DisplayAsked& asked = world.resources.Get<DisplayAsked>();
		if (asked.any && asked.display == settings.display && asked.scaling == settings.scaling) return;

		const auto& info = world.resources.Get<engine::graphics::DisplayInfo>();
		if (info.current.mode == engine::graphics::WindowMode::Windowed)
		{
			asked.windowed = {.width = info.current.width, .height = info.current.height};
		}
		asked.any = true;
		asked.display = settings.display;
		asked.scaling = settings.scaling;
		world.resources.Get<engine::graphics::DisplayRequest>().pending =
		    DisplayRequestFor(settings, info, asked.windowed);
	}
} // namespace game
