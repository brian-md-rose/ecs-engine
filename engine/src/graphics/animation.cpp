#include "graphics/animation.hpp"

#include <algorithm>
#include <cmath>

namespace engine::graphics
{
	int AnimationFrame(const Animation& animation) noexcept
	{
		const Clip& clip = animation.clip;
		if (clip.frames <= 1 || clip.secondsPerFrame <= 0.0f) return 0;
		const auto step = static_cast<long long>(std::floor(std::max(animation.elapsed, 0.0f) / clip.secondsPerFrame));
		if (clip.loop) return static_cast<int>(step % clip.frames);
		return static_cast<int>(std::min<long long>(step, clip.frames - 1));
	}

	PixelRect AnimationSource(const Animation& animation) noexcept
	{
		PixelRect source = animation.clip.firstFrame;
		source.x += AnimationFrame(animation) * source.width;
		return source;
	}

	Animation Advance(Animation animation, float seconds) noexcept
	{
		animation.elapsed += std::max(seconds, 0.0f);
		// Keep a looping clip's time small, so float precision never makes it stutter.
		const Clip& clip = animation.clip;
		const float length = static_cast<float>(clip.frames) * clip.secondsPerFrame;
		if (clip.loop && length > 0.0f) animation.elapsed = std::fmod(animation.elapsed, length);
		return animation;
	}

	Animation Play(Animation animation, const Clip& clip) noexcept
	{
		if (animation.clip == clip) return animation;
		return Animation{.clip = clip};
	}
} // namespace engine::graphics
