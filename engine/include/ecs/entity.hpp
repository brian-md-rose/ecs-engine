#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace engine::ecs
{
	// An entity is a handle: an index (id) plus a generation that is bumped every time the id is recycled,
	// so a handle to a destroyed entity is detectably stale.
	//
	// Generation wrap: after 2^32 recycles of the same id the generation wraps to 0, and a handle that old
	// would look alive again. That is accepted rather than retiring the id; no realistic game recycles one
	// slot four billion times while still holding the original handle.
	struct Entity
	{
		// Reserved id that Registry::Create() never hands out.
		static constexpr std::uint32_t kNullId = UINT32_MAX;

		// "No entity". Use for optional references such as a target or an owner. Defined below,
		// because Entity is still incomplete here.
		static const Entity Null;

		std::uint32_t id = kNullId;
		std::uint32_t generation = kNullId;

		[[nodiscard]] constexpr bool IsNull() const noexcept { return id == kNullId; }
		[[nodiscard]] constexpr bool operator==(const Entity&) const noexcept = default;
	};

	inline constexpr Entity Entity::Null{};

	static_assert(Entity{}.IsNull(), "a default-constructed Entity must be null");
} // namespace engine::ecs

template <> struct std::hash<engine::ecs::Entity>
{
	std::size_t operator()(const engine::ecs::Entity& entity) const noexcept
	{
		// Pack both fields into 64 bits: distinct entities never share a key before hashing.
		return std::hash<std::uint64_t>{}((std::uint64_t{entity.generation} << 32) | entity.id);
	}
};
