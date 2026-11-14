#pragma once

#include "ecs/commandbuffer.hpp"
#include "ecs/events.hpp"
#include "ecs/registry.hpp"
#include "ecs/resources.hpp"

namespace engine::ecs
{
	// Everything a system works on: the entities and their components, the one-of-a-kind
	// shared data, a buffer for structural changes made while iterating, and the events
	// published this phase.
	//
	// Per-call inputs owned elsewhere (dt, the keyboard, the renderer) are passed to systems
	// separately and never stored here, so nothing in a World can dangle between frames.
	struct World
	{
		Registry registry;
		Resources resources;
		CommandBuffer commands;
		EventBus events;
	};
} // namespace engine::ecs
