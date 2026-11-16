#pragma once

#include "ecs/scheduler.hpp"
#include "ecs/world.hpp"

namespace engine::graphics
{
	// Steps every Animation by context.dt and shows its frame in the entity's Sprite. Schedule it
	// in LateUpdate, after any system that picks clips (with Play), so the frame drawn is the one
	// for this update; give it a run condition to freeze animations while paused.
	void AnimationSystem(ecs::World& world, const ecs::FrameContext& context);
} // namespace engine::graphics
