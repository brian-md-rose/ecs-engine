#include "ui/ninepatch.hpp"

namespace engine::ui
{
	namespace
	{
		// Splits total into a start and an end border of at most start and end, shrinking both in
		// proportion if they don't fit, and the middle that is left.
		struct Split
		{
			int first = 0;
			int middle = 0;
			int last = 0;
		};

		[[nodiscard]] Split SplitAxis(int total, int start, int end)
		{
			total = total < 0 ? 0 : total;
			start = start < 0 ? 0 : start;
			end = end < 0 ? 0 : end;
			if (start + end > total)
			{
				const int first = start + end == 0 ? 0 : total * start / (start + end);
				return {.first = first, .middle = 0, .last = total - first};
			}
			return {.first = start, .middle = total - start - end, .last = end};
		}
	} // namespace

	void NinePatchLayout(const NinePatch& patch, std::array<NinePatchPiece, 9>& pieces)
	{
		const Split across = SplitAxis(patch.width, patch.left, patch.right);
		const Split down = SplitAxis(patch.height, patch.top, patch.bottom);

		// The same cut of the picture, clamped so the borders never exceed it.
		const Split sourceAcross = SplitAxis(patch.source.width, patch.left, patch.right);
		const Split sourceDown = SplitAxis(patch.source.height, patch.top, patch.bottom);

		const std::array<int, 3> destX{0, across.first, across.first + across.middle};
		const std::array<int, 3> destWidth{across.first, across.middle, across.last};
		const std::array<int, 3> sourceX{patch.source.x, patch.source.x + sourceAcross.first,
		                                 patch.source.x + sourceAcross.first + sourceAcross.middle};
		const std::array<int, 3> sourceWidth{sourceAcross.first, sourceAcross.middle, sourceAcross.last};

		const std::array<int, 3> destY{0, down.first, down.first + down.middle};
		const std::array<int, 3> destHeight{down.first, down.middle, down.last};
		const std::array<int, 3> sourceY{patch.source.y, patch.source.y + sourceDown.first,
		                                 patch.source.y + sourceDown.first + sourceDown.middle};
		const std::array<int, 3> sourceHeight{sourceDown.first, sourceDown.middle, sourceDown.last};

		for (std::size_t row = 0; row < 3; ++row)
		{
			for (std::size_t column = 0; column < 3; ++column)
			{
				pieces[row * 3 + column] = {
				    .source = {.x = sourceX[column],
				               .y = sourceY[row],
				               .width = sourceWidth[column],
				               .height = sourceHeight[row]},
				    .offset = {static_cast<float>(destX[column]), static_cast<float>(destY[row])},
				    .width = destWidth[column],
				    .height = destHeight[row]};
			}
		}
	}
} // namespace engine::ui
