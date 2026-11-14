#pragma once

#include "ecs/commandbuffer.hpp"
#include "ecs/registry.hpp"
#include "ecs/resources.hpp"

namespace engine::ecs
{
	// Everything a system works on: the entities and their components, the one-of-a-kind
	// shared data, and a buffer for structural changes made while iterating.
	//
	// Per-call inputs owned elsewhere (dt, the keyboard, the renderer) are passed to systems
	// separately and never stored here, so nothing in a World can dangle between frames.
	struct World
	{
		Registry registry;
		Resources resources;
		CommandBuffer commands;
	};
} // namespace engine::ecs
