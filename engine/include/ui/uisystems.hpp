#pragma once

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

namespace engine::ui
{
	// The systems that make the components in ui.hpp work. Add them to the Scheduler, in this order:
	//
	//     scheduler.Add(ecs::Phase::Input, "UiPointer", ui::UiPointerSystem);
	//     scheduler.Add(ecs::Phase::Input, "UiFocus", ui::UiFocusSystem);
	//     scheduler.Add(ecs::Phase::Input, "ScrollView", ui::ScrollViewSystem);
	//     scheduler.Add(ecs::Phase::LateUpdate, "NinePatch", ui::NinePatchSystem);
	//
	// and Emplace a PointerState resource (and a UiCamera if the UI is in the world) at setup. Systems
	// that react to clicks run after the Input phase, and see this update's PointerState and Clicked
	// events (events are delivered at the end of the phase).

	// Works out what the pointer is over from Hitbox, Transform and the mouse; sets Hovered and Pressed;
	// publishes Clicked when a button goes down and comes up on the same Interactable; and fills in
	// PointerState. Needs context.input.
	void UiPointerSystem(ecs::World& world, const ecs::FrameContext& context);

	// Tab moves Focused to the next FocusOrder entity, Shift+Tab to the previous, wrapping at the ends.
	void UiFocusSystem(ecs::World& world, const ecs::FrameContext& context);

	// Scrolls ScrollViews with the wheel and by dragging, clamps them, and positions their content.
	void ScrollViewSystem(ecs::World& world, const ecs::FrameContext& context);

	// Creates and updates the nine sprites of every NinePatch.
	void NinePatchSystem(ecs::World& world, const ecs::FrameContext& context);
} // namespace engine::ui
