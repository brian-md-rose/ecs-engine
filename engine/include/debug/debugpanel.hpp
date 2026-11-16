#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

#include "core/framestats.hpp"
#include "ecs/entity.hpp"
#include "ecs/registry.hpp"
#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"
#include "graphics/color.hpp"
#include "input/keyboard.hpp"

// An on-screen readout for working on a game: frames per second and draw time, how many entities
// have each component, and how long each system takes. Drawn with the Renderer2D's bitmap font
// on top of everything, in the top-left corner, and toggled with a key (F3 by default).
//
// A game adds it once at setup, before its own Render systems, so the panel's text is up to
// date when the game draws:
//
//     engine::debug::AddDebugPanel(world, scheduler);
//     scheduler.Add(Phase::Render, "Render", RenderSystem);
namespace engine::debug
{
	inline constexpr std::uint8_t kDebugPanelLayer = 255; // above everything, and never tinted
	inline constexpr graphics::Color kDebugPanelBackground{0, 0, 0};
	inline constexpr graphics::Color kDebugPanelTextColor{120, 255, 120};
	inline constexpr float kDebugPanelMargin = 2.0f; // logical pixels around the text

	// Resource: the panel's settings and its entities while it is shown.
	struct DebugPanel
	{
		bool visible = false;
		input::Key toggleKey = input::Key::F3;
		std::size_t maxSystems = 12; // the slowest systems are listed, up to this many
		ecs::Entity background = ecs::Entity::Null;
		ecs::Entity text = ecs::Entity::Null;
	};

	// Everything the panel shows.
	struct DebugPanelView
	{
		const core::FrameStats* frames = nullptr; // nothing shown for frames without one
		std::size_t entities = 0;
		std::span<const ecs::Registry::ComponentCount> components{};
		std::span<const ecs::Scheduler::SystemTiming> systems{};
	};

	// The panel's text, in capitals (bitmap fonts often have no small letters), at most columns
	// characters wide: a line of frame stats, the entity count, the slowest maxSystems systems
	// with their average milliseconds per run (or OFF if they were skipped last time), and every
	// component with its count, two to a line. Pure.
	[[nodiscard]] std::string DebugPanelText(const DebugPanelView& view, std::size_t maxSystems, std::size_t columns);

	// Setup: adds the DebugPanel and a core::FrameStats resource (unless the game has one), and
	// the panel's two systems: DebugPanelToggleSystem in Input and DebugPanelSystem in Render.
	void AddDebugPanel(ecs::World& world, ecs::Scheduler& scheduler);

	// Input: shows or hides the panel when its toggle key is pressed.
	void DebugPanelToggleSystem(ecs::World& world, const ecs::FrameContext& context);

	// Render: counts the frame in FrameStats (draw time from the Renderer2D resource, if there
	// is one) and, while the panel is shown, rewrites its text and sizes its background.
	void DebugPanelSystem(ecs::World& world, const ecs::FrameContext& context);
} // namespace engine::debug
