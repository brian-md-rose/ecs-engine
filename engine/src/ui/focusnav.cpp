#include "ui/focusnav.hpp"

#include <cmath>

namespace engine::ui
{
	std::optional<std::size_t> PickFocusNeighbour(std::span<const FocusCandidate> candidates, std::size_t from,
	                                              input::PadDirection direction)
	{
		if (from >= candidates.size()) return std::nullopt;
		const graphics::Position origin = candidates[from].centre;

		std::optional<std::size_t> best;
		float bestScore = 0.0f;
		for (std::size_t i = 0; i < candidates.size(); ++i)
		{
			if (i == from) continue;
			const float dx = candidates[i].centre.x - origin.x;
			const float dy = candidates[i].centre.y - origin.y;

			// along: how far it is in the direction pushed; across: how far off to the side.
			float along = 0.0f;
			float across = 0.0f;
			switch (direction)
			{
			case input::PadDirection::Up:
				along = -dy;
				across = dx;
				break;
			case input::PadDirection::Down:
				along = dy;
				across = dx;
				break;
			case input::PadDirection::Left:
				along = -dx;
				across = dy;
				break;
			case input::PadDirection::Right:
				along = dx;
				across = dy;
				break;
			}
			if (along <= 0.0f) continue; // not that way (level with it counts as not that way)

			constexpr float kSideWeight = 2.0f; // a unit off to the side costs more than a unit further along
			const float score = along + kSideWeight * std::fabs(across);
			if (!best || score < bestScore)
			{
				best = i;
				bestScore = score;
			}
		}
		return best;
	}
} // namespace engine::ui
