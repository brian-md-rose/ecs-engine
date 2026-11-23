#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <utility>

#include "application/editorhost.hpp"
#include "audio/audiodevice.hpp"
#include "core/clock.hpp"
#include "graphics/display.hpp"
#include "graphics/renderer.hpp"
#include "graphics/screenshot.hpp"
#include "graphics/window.hpp"
#include "input/input.hpp"

// SDL types are only forward-declared here, as in the other platform headers.
struct SDL_Gamepad;

namespace engine::application
{
	struct ApplicationConfig
	{
		const char* title = "ecs-engine";
		int windowWidth = 1280;
		int windowHeight = 720;
		int logicalWidth = 320; // the game's fixed resolution, integer-scaled to the window
		int logicalHeight = 180;
		double updateRate = 60.0; // fixed game-logic updates per second
		// How the picture is scaled to the window, and the VSync and frame cap to start with; the game can
		// change them while it runs through DisplayRequest (graphics/display.hpp).
		graphics::Scaling scaling = graphics::Scaling::Integer;
		bool vsync = true;
		int frameCap = 0; // frames per second when vsync is off, 0 for no cap
		// The most fixed updates run in one frame, so a slow frame can't snowball into ever more catch-up
		// updates (see core::FixedTimestep::SetMaxUpdates). The default is what a stalled frame gets at normal
		// speed (the 0.25 s cap at 60 Hz), so it only matters if the game runs faster than real time. 0 for no limit.
		int maxUpdatesPerFrame = 15;
	};

	// The two resources Application reads and writes to change how the game is shown. Either can be null.
	struct DisplayLink
	{
		graphics::DisplayRequest* request = nullptr; // read, then cleared, after each frame
		graphics::DisplayInfo* info = nullptr;       // written: the settings in force and the supported sizes
	};

	// The two resources Application reads and writes for screenshots. Either can be null.
	struct ScreenshotLink
	{
		graphics::ScreenshotRequest* request = nullptr; // read, then cleared, after each frame
		graphics::ScreenshotResult* result = nullptr;   // written: the last file, the last error and a count
	};

	// What the game provides. Application calls these from its main loop.
	class Game
	{
	public:
		virtual ~Game() = default;

		// Called at a fixed rate (1 / ApplicationConfig::updateRate seconds, passed as dt),
		// zero or more times per frame. Run ECS systems here. The input's pressed/released
		// edges and typed text are cleared after each call, so each press is seen by exactly one update.
		// Mapping keys to game actions is the game's job. audio plays sound effects; it is null
		// when there is nothing to play them on (tests), and silent when there is no device.
		virtual void Update(double dt, const input::Input& input, audio::AudioDevice* audio) = 0;

		// Called once per frame: clear and draw. Application presents the frame afterwards.
		// alpha in [0, 1) is how far the game is into the next update, for optional
		// interpolation between the last two states.
		virtual void Render(graphics::Renderer& renderer, double alpha) = 0;

		// Asked after each update: true closes the application, as closing the window does (a
		// menu's Quit, for one). Games that never ask to quit needn't override it.
		[[nodiscard]] virtual bool WantsToQuit() const { return false; }

		// Asked each frame: false hides the operating system's mouse pointer over the window, for a game
		// that draws its own. Games that don't draw one needn't override it.
		[[nodiscard]] virtual bool ShowSystemCursor() const { return true; }

		// Asked after each frame: where the game keeps its DisplayRequest and DisplayInfo. Application applies a
		// pending request (window mode and size, VSync, frame cap, scaling), clears it and refreshes the info. A
		// game that never changes these needn't override it.
		[[nodiscard]] virtual DisplayLink Display() { return {}; }

		// Asked each frame: where the game keeps its ScreenshotRequest and ScreenshotResult. A game that never takes
		// a screenshot from code needn't override it; F9 works without it.
		[[nodiscard]] virtual ScreenshotLink Screenshot() { return {}; }
	};

	// Owns the platform (SDL, window, renderer, audio) and runs the main loop.
	class Application
	{
		// Starts SDL before the window exists and shuts it down after the window is gone.
		class SdlScope
		{
		public:
			SdlScope();
			~SdlScope();
			SdlScope(const SdlScope&) = delete;
			SdlScope& operator=(const SdlScope&) = delete;
			SdlScope(SdlScope&&) = delete;
			SdlScope& operator=(SdlScope&&) = delete;
		};

		// Declaration order is construction order; destruction runs in reverse, so the audio
		// device and renderer go first, then the window, and SDL shuts down last.
		SdlScope sdl;
		graphics::Window window;
		graphics::Renderer renderer;
		audio::AudioDevice audio;
		input::Input input;
		// The open controllers: the SDL handle and the joystick instance id of each slot of Input::pads (id 0 for a
		// free slot). Declared before updateStep so the destructor body can close them with SDL still running.
		std::array<SDL_Gamepad*, input::kMaxPads> padHandles{};
		std::array<std::uint32_t, input::kMaxPads> padIds{};
		double updateStep;
		int maxUpdatesPerFrame;
		graphics::DisplaySettings display;                         // what is in force
		bool displayInfoStale = true;                              // DisplayInfo needs refreshing
		EditorHost* editor = nullptr;                              // the overlay, when there is one (not owned)
		std::filesystem::path screenshotFolder;                    // where screenshots go
		std::optional<input::Key> screenshotKey = input::Key::F9; // the key that takes one, if any
		bool screenshotAsked = false;                              // the key was pressed since the last frame
		int screenshotAfter = 0; // save the screenshot at this frame, then quit (0: off)
		int framesDrawn = 0;

		// Drains pending SDL events, feeding keyboard, text and mouse events to input. Returns false once the
		// window has been asked to close.
		bool PumpEvents();
		// Records the pointer at a logical-pixel position, and whether it is over the logical area.
		void SetMousePosition(float x, float y);
		// Gamepad hotplug: opens the pad with joystick id into the first free slot, or closes it and frees its slot.
		void OpenPad(std::uint32_t id);
		void ClosePad(std::uint32_t id);
		// The slot of the pad with joystick id, or kMaxPads if it is not open.
		[[nodiscard]] std::size_t SlotOfPad(std::uint32_t id) const;
		// Applies a pending DisplayRequest of game, and refreshes its DisplayInfo when something changed.
		void UpdateDisplay(Game& game);
		void ApplyDisplay(const graphics::DisplaySettings& wanted);
		// Saves the frame drawn so far as a PNG in screenshotFolder, and reports it to link.result. Called after the
		// frame is drawn and before it is presented.
		// Returns whether a file was written.
		[[nodiscard]] bool SaveScreenshot(const ScreenshotLink& link);

	public:
		explicit Application(const ApplicationConfig& config);
		~Application();

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;
		Application(Application&&) = delete;
		Application& operator=(Application&&) = delete;

		// Sets the editor overlay (not owned; it must outlive Run), or none with nullptr. See EditorHost.
		void SetEditorHost(EditorHost* host) noexcept { editor = host; }

		// Where screenshots go, made if it does not exist (see graphics::ScreenshotDirectory). Empty, the start state,
		// turns screenshots off.
		void SetScreenshotFolder(std::filesystem::path folder) { screenshotFolder = std::move(folder); }

		// The key that takes a screenshot (F9 to start with), or nullopt for none. With an editor overlay the picture
		// is the whole window, panels included; without one it is the game's picture at its logical size.
		void SetScreenshotKey(std::optional<input::Key> key) noexcept { screenshotKey = key; }

		// For scripts: takes a screenshot of frame number frames (counting from 1) and then closes the application. Run
		// returns 1 instead of 0 if the file could not be saved. 0 or less turns it off.
		void SetScreenshotAfter(int frames) noexcept { screenshotAfter = frames; }

		// Applies the command-line options of graphics::ParseScreenshotArgs: a directory replaces the screenshot
		// folder, and a frame count is SetScreenshotAfter. A bad option is logged and ignored.
		void SetScreenshotArgs(const graphics::ScreenshotArgs& args);

		// Takes a screenshot of the next frame, as the key does.
		void RequestScreenshot() noexcept { screenshotAsked = true; }

		// Runs until the window is closed or the game asks to quit. Returns 0 on a normal exit, 1 if the window or
		// renderer couldn't be created.
		int Run(Game& game);

		// As above, with a caller-supplied clock (for tests or replays).
		int Run(Game& game, const core::Clock& clock);
	};
} // namespace engine::application
