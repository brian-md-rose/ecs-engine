#include "graphics/display.hpp"

#include <algorithm>
#include <cmath>

namespace engine::graphics
{
	DisplaySettings Sanitized(DisplaySettings settings) noexcept
	{
		settings.width = std::clamp(settings.width, kMinWindowSize, kMaxWindowSize);
		settings.height = std::clamp(settings.height, kMinWindowSize, kMaxWindowSize);
		settings.frameCap = std::clamp(settings.frameCap, 0, kMaxFrameCap);
		return settings;
	}

	PresentationRect Presentation(int logicalWidth, int logicalHeight, int windowWidth, int windowHeight,
	                              Scaling scaling) noexcept
	{
		if (logicalWidth <= 0 || logicalHeight <= 0 || windowWidth <= 0 || windowHeight <= 0) return {};

		const float fit = std::min(static_cast<float>(windowWidth) / static_cast<float>(logicalWidth),
		                           static_cast<float>(windowHeight) / static_cast<float>(logicalHeight));
		const float scale = scaling == Scaling::Fit ? fit : std::max(std::floor(fit), std::min(fit, 1.0f));

		PresentationRect rect;
		rect.scale = scale;
		rect.width = static_cast<float>(logicalWidth) * scale;
		rect.height = static_cast<float>(logicalHeight) * scale;
		rect.x = (static_cast<float>(windowWidth) - rect.width) / 2.0f;
		rect.y = (static_cast<float>(windowHeight) - rect.height) / 2.0f;
		return rect;
	}

	std::vector<Resolution> NormalizeResolutions(std::vector<Resolution> modes)
	{
		std::erase_if(modes, [](const Resolution& mode)
		{
			return mode.width <= 0 || mode.height <= 0;
		});
		std::ranges::sort(modes, [](const Resolution& a, const Resolution& b)
		{
			const long long areaA = static_cast<long long>(a.width) * a.height;
			const long long areaB = static_cast<long long>(b.width) * b.height;
			if (areaA != areaB) return areaA > areaB;
			return a.width > b.width;
		});
		modes.erase(std::unique(modes.begin(), modes.end()), modes.end());
		return modes;
	}

	double FrameDelaySeconds(double frameStart, double now, int frameCap) noexcept
	{
		if (frameCap <= 0) return 0.0;
		return std::max(0.0, 1.0 / static_cast<double>(frameCap) - (now - frameStart));
	}
} // namespace engine::graphics
