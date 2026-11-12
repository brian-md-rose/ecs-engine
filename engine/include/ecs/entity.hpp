#pragma once
#include <cstdint>

namespace engine::ecs
{
	struct Entity
	{
		std::uint32_t id = 0;
		std::uint32_t generation = 0;
		bool operator==(const Entity& entity) const { return id == entity.id && generation == entity.generation; }
	};
}