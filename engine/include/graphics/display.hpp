#pragma once

#include <cstdint>
#include <optional>
#include <vector>

// How the game is shown: window mode, size, VSync, frame cap and how the logical picture is scaled to
// the window. Plain data and arithmetic, no SDL, so a game changes it through resources and the
// arithmetic is tested directly. Application applies it (see application/application.hpp).
//
//     auto& request = world.resources.Get<graphics::DisplayRequest>();
//     request.pending = graphics::DisplaySettings{.mode = WindowMode::Borderless, ...};
//
// A game never calls into Application: it sets DisplayRequest::pending, and after the frame
// Application applies it, clears it and refreshes DisplayInfo. A headless run just ignores it.
namespace engine::graphics
{
	// How the logical picture fills the window. Integer: the largest whole-number scale that fits, so
	// pixels stay square and sharp, with black bars around the rest. Fit: scaled by any amount to fill
	// as much of the window as the aspect ratio allows (letterbox), so pixels can be uneven.
	enum class Scaling : std::uint8_t
	{
		Integer,
		Fit
	};

	enum class WindowMode : std::uint8_t
	{
		Windowed,
		Borderless, // fills the desktop with no frame and does not change the display mode
		Fullscreen  // exclusive: the display switches to the closest mode to width x height
	};

	struct Resolution
	{
		int width = 0;
		int height = 0;
		friend bool operator==(const Resolution&, const Resolution&) = default;
	};

	struct DisplaySettings
	{
		WindowMode mode = WindowMode::Windowed;
		int width = 1280; // the window's size when Windowed, the display mode asked for when Fullscreen
		int height = 720; // (Borderless ignores both and uses the desktop)
		bool vsync = true;
		int frameCap = 0; // frames per second when vsync is off, 0 for no cap
		Scaling scaling = Scaling::Integer;
		friend bool operator==(const DisplaySettings&, const DisplaySettings&) = default;
	};

	// Resource: set pending to ask for a change. Application applies it after the frame, then clears it.
	struct DisplayRequest
	{
		std::optional<DisplaySettings> pending;
	};

	// Resource, written by Application: the settings in force and the sizes the display supports,
	// largest first (empty until the first frame or when there is no display).
	struct DisplayInfo
	{
		DisplaySettings current{};
		std::vector<Resolution> resolutions;
	};

	inline constexpr int kMinWindowSize = 160;
	inline constexpr int kMaxWindowSize = 16384;
	inline constexpr int kMaxFrameCap = 1000;

	// settings with the size inside kMinWindowSize and kMaxWindowSize and the frame cap inside 0 and
	// kMaxFrameCap, so a bad request can't make a window nobody can see.
	[[nodiscard]] DisplaySettings Sanitized(DisplaySettings settings) noexcept;

	// Where the logical picture lands in the window, in window pixels.
	struct PresentationRect
	{
		float x = 0;
		float y = 0;
		float width = 0;
		float height = 0;
		float scale = 1; // window pixels per logical pixel
		friend bool operator==(const PresentationRect&, const PresentationRect&) = default;
	};

	// The rectangle a logicalWidth x logicalHeight picture takes in a windowWidth x windowHeight window,
	// centred. Integer uses the largest whole scale that fits (but never below the fit scale, so a window
	// smaller than the picture still shows all of it), Fit the exact fit. Zero sizes give an empty rectangle.
	[[nodiscard]] PresentationRect Presentation(int logicalWidth, int logicalHeight, int windowWidth, int windowHeight,
	                                            Scaling scaling) noexcept;

	// The sizes in modes, without repeats or non-positive ones, largest first (by area, then width). A
	// display lists each size once per refresh rate and pixel format; a settings screen wants it once.
	[[nodiscard]] std::vector<Resolution> NormalizeResolutions(std::vector<Resolution> modes);

	// How long to sleep after a frame that began at frameStart, if now is the time it finished, so frames
	// last at least 1 / frameCap seconds. 0 when there is no cap (frameCap of 0 or less) or the frame was already
	// that long.
	[[nodiscard]] double FrameDelaySeconds(double frameStart, double now, int frameCap) noexcept;
} // namespace engine::graphics
