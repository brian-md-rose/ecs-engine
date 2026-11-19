#pragma once
#include <optional>

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/display.hpp"
#include "options.hpp"

// Carries the options screen's DISPLAY and SCALING choices to the engine (graphics/display.hpp): when
// they change, or on the first update to apply the saved ones, it puts a DisplayRequest in the world,
// and Application applies it after the frame.
namespace game
{
	// Resource: the display choices last asked for, so a refused one (a mode the display can't do) is
	// not asked for again every update.
	struct DisplayAsked
	{
		bool any = false;
		engine::graphics::WindowMode display = engine::graphics::WindowMode::Windowed;
		engine::graphics::Scaling scaling = engine::graphics::Scaling::Integer;
		engine::graphics::Resolution windowed{}; // the window's size the last time it was framed, to go back to
	};

	// The request for settings given the display as it is now: the same VSync and frame cap, with the chosen mode
	// and scaling. Fullscreen asks for the biggest size the display lists (its own resolution); Windowed for
	// the size of the window as it was last framed (windowed, when it has a size). Empty when that is how things
	// already are.
	[[nodiscard]] std::optional<engine::graphics::DisplaySettings> DisplayRequestFor(
	    const Settings& settings, const engine::graphics::DisplayInfo& info,
	    engine::graphics::Resolution windowed = {});

	// LateUpdate: sends DisplayRequestFor to DisplayRequest when the choices differ from DisplayAsked.
	void DisplaySystem(engine::ecs::World& world, const engine::ecs::FrameContext& context);
} // namespace game
