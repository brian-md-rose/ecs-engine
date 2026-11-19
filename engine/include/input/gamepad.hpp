#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

// Game controllers, as plain data: no SDL, so tests and headless tools build one by hand. Application
// fills Input::pads from the platform's events (hotplug included), up to kMaxPads of them in the order
// they were plugged in.
//
// Buttons are named by position (South is the bottom one of the four on the right: A on an Xbox pad,
// Cross on a PlayStation one), so a game binds the position and shows the glyph for the family with
// PadButtonLabel. Sticks run from -1 to 1 with x to the right and y DOWN, like screen coordinates; they
// are deadzoned, so a resting stick reads exactly 0. Edges are latched like Keyboard's: EndUpdate() after
// each game update clears them.
namespace engine::input
{
	enum class PadButton : std::uint8_t
	{
		South,
		East,
		West,
		North,
		Start,
		Back,
		L1,
		R1,
		LStick, // pressing the stick in
		RStick,
		DUp,
		DDown,
		DLeft,
		DRight,
		Count
	};
	inline constexpr std::size_t kPadButtonCount = static_cast<std::size_t>(PadButton::Count);

	enum class PadAxis : std::uint8_t
	{
		LeftX,
		LeftY,
		RightX,
		RightY,
		LeftTrigger,
		RightTrigger
	};

	enum class PadFamily : std::uint8_t
	{
		Xbox,
		PlayStation,
		Switch,
		SteamDeck,
		Generic
	};

	enum class PadDirection : std::uint8_t
	{
		Up,
		Down,
		Left,
		Right
	};

	inline constexpr std::size_t kMaxPads = 4;
	inline constexpr float kDefaultDeadzone = 0.2f;
	inline constexpr float kPushThreshold = 0.5f; // how far a stick must go to count as a d-pad press

	struct Stick
	{
		float x = 0;
		float y = 0;
		friend bool operator==(const Stick&, const Stick&) = default;
	};

	// A stick position with the dead centre removed. Inside the deadzone (a circle, so diagonals are fair) it is 0;
	// outside, the magnitude is rescaled so it still runs from 0 to 1, and its direction is kept.
	[[nodiscard]] Stick ApplyDeadzone(float x, float y, float deadzone) noexcept;

	// A trigger value, 0 to 1, with the deadzone removed and the rest rescaled.
	[[nodiscard]] float ApplyTriggerDeadzone(float value, float deadzone) noexcept;

	// What to draw for a button on a pad of the given family: "A", "CROSS", "+", and so on. Upper case,
	// to suit a pixel font.
	[[nodiscard]] std::string_view PadButtonLabel(PadFamily family, PadButton button) noexcept;

	// True for a Steam Deck's built-in controller, by USB ids (Valve 0x28de, product 0x1205) or by a name that
	// has "Steam Deck" in it; SDL reports it as a generic pad.
	[[nodiscard]] bool IsSteamDeck(std::string_view name, std::uint16_t vendor, std::uint16_t product) noexcept;

	class Gamepad
	{
		std::array<bool, kPadButtonCount> down{};
		std::array<bool, kPadButtonCount> pressed{};
		std::array<bool, kPadButtonCount> released{};
		Stick rawLeft{};
		Stick rawRight{};
		Stick left{};
		Stick right{};
		std::array<bool, 4> leftPast{};   // whether the left stick is past kPushThreshold in each PadDirection
		std::array<bool, 4> leftPushed{}; // latched until EndUpdate, for StickWasPushed
		void LatchPushes() noexcept;
		float leftTrigger = 0;
		float rightTrigger = 0;

	public:
		bool connected = false;
		PadFamily family = PadFamily::Generic;
		float deadzone = kDefaultDeadzone;

		// Platform layer: a pad was plugged in (a fresh state) or removed (everything released).
		void Connect(PadFamily padFamily);
		void Disconnect();

		// Platform layer: report events. OnAxis takes -1 to 1 (0 to 1 for triggers, positive y down) and returns true
		// if, after the deadzone, the stick or trigger it belongs to is away from rest: real input, as opposed
		// to drift.
		void OnButton(PadButton button, bool isDown);
		bool OnAxis(PadAxis axis, float value);

		// Releases every button and centres the sticks, as a window losing focus does.
		void ReleaseAll();
		// Called after each game update: clears the edges it has seen.
		void EndUpdate();

		[[nodiscard]] bool IsDown(PadButton button) const;
		[[nodiscard]] bool WasPressed(PadButton button) const; // went down since the last update
		[[nodiscard]] bool WasReleased(PadButton button) const;

		[[nodiscard]] Stick Left() const noexcept { return left; }
		[[nodiscard]] Stick Right() const noexcept { return right; }
		[[nodiscard]] float LeftTrigger() const noexcept { return leftTrigger; }
		[[nodiscard]] float RightTrigger() const noexcept { return rightTrigger; }

		// The left stick is pushed past threshold in direction.
		[[nodiscard]] bool StickHeld(PadDirection direction, float threshold = kPushThreshold) const noexcept;
		// It went past threshold since the last update (it was not past it then).
		[[nodiscard]] bool StickWasPushed(PadDirection direction) const noexcept;

		// The d-pad or the left stick, whichever: held, and went down since the last update.
		[[nodiscard]] bool DirectionHeld(PadDirection direction) const;
		[[nodiscard]] bool DirectionWasPushed(PadDirection direction) const;
	};
} // namespace engine::input
