#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include "ecs/entity.hpp"
#include "graphics/transform.hpp"
#include "input/gamepad.hpp"

namespace engine::ui
{
	// A thing keyboard or pad focus can land on, and where its middle is (in the one space all candidates share).
	struct FocusCandidate
	{
		ecs::Entity entity = ecs::Entity::Null;
		graphics::Position centre{};
	};

	// The candidate to move focus to from candidates[from] when the player pushes direction: the nearest one that
	// lies that way, preferring ones in line over ones off to the side (a button below you beats a closer one
	// that is mostly to the right). nullopt if nothing lies that way, or from is out of range. Focus doesn't wrap.
	[[nodiscard]] std::optional<std::size_t> PickFocusNeighbour(std::span<const FocusCandidate> candidates,
	                                                            std::size_t from, input::PadDirection direction);
} // namespace engine::ui
