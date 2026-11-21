#include "application/application.hpp"

#include <SDL3/SDL.h>
#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>

#include "core/fixedtimestep.hpp"
#include "core/sdllog.hpp"
#include "graphics/imagefile.hpp"
#include "input/input.hpp"
#include "input/mouse.hpp"

namespace engine::application
{
	namespace
	{
		struct KeyMapping
		{
			SDL_Scancode scancode;
			input::Key key;
		};

		// Built from the one key table (input/keys.hpp), so a new key needs no change here. A table
		// rather than a switch: SDL_Scancode has hundreds of enumerators and /Wall warns about every
		// one a switch leaves unhandled.
#define ENGINE_KEY_MAPPING(name, sdl) {SDL_SCANCODE_##sdl, input::Key::name},
		constexpr KeyMapping kKeyMap[] = {ENGINE_KEY_LIST(ENGINE_KEY_MAPPING)
		                                      ENGINE_KEY_ALIAS_LIST(ENGINE_KEY_MAPPING)};
#undef ENGINE_KEY_MAPPING

		// True if another physical key that counts as key (the other Shift, say) is still held, so
		// letting go of one of a pair doesn't release the key while the other is down.
		bool OtherPhysicalKeyHeld(input::Key key, SDL_Scancode released)
		{
			const bool* state = SDL_GetKeyboardState(nullptr);
			if (state == nullptr) return false;
			for (const KeyMapping& mapping : kKeyMap)
			{
				if (mapping.key == key && mapping.scancode != released && state[mapping.scancode]) return true;
			}
			return false;
		}

		std::optional<input::Key> ToKey(SDL_Scancode scancode)
		{
			for (const KeyMapping& mapping : kKeyMap)
			{
				if (mapping.scancode == scancode)
				{
					return mapping.key;
				}
			}
			return std::nullopt;
		}

		// The family a pad belongs to, from SDL's own idea of its type and, for the Steam Deck (which SDL calls
		// generic), its name and USB ids.
		input::PadFamily FamilyOf(SDL_Gamepad* pad)
		{
			const char* name = SDL_GetGamepadName(pad);
			if (input::IsSteamDeck(name != nullptr ? name : "", SDL_GetGamepadVendor(pad), SDL_GetGamepadProduct(pad)))
			{
				return input::PadFamily::SteamDeck;
			}
			// Compared one by one rather than switched on: MSVC wants every enumerator listed, even with a default.
			const SDL_GamepadType type = SDL_GetGamepadType(pad);
			if (type == SDL_GAMEPAD_TYPE_XBOX360 || type == SDL_GAMEPAD_TYPE_XBOXONE) return input::PadFamily::Xbox;
			if (type == SDL_GAMEPAD_TYPE_PS3 || type == SDL_GAMEPAD_TYPE_PS4 || type == SDL_GAMEPAD_TYPE_PS5)
				return input::PadFamily::PlayStation;
			if (type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO || type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT ||
			    type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT ||
			    type == SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR)
				return input::PadFamily::Switch;
			return input::PadFamily::Generic;
		}

		std::optional<input::PadButton> ToPadButton(Uint8 button)
		{
			switch (button)
			{
			case SDL_GAMEPAD_BUTTON_SOUTH:
				return input::PadButton::South;
			case SDL_GAMEPAD_BUTTON_EAST:
				return input::PadButton::East;
			case SDL_GAMEPAD_BUTTON_WEST:
				return input::PadButton::West;
			case SDL_GAMEPAD_BUTTON_NORTH:
				return input::PadButton::North;
			case SDL_GAMEPAD_BUTTON_START:
				return input::PadButton::Start;
			case SDL_GAMEPAD_BUTTON_BACK:
				return input::PadButton::Back;
			case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
				return input::PadButton::L1;
			case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
				return input::PadButton::R1;
			case SDL_GAMEPAD_BUTTON_LEFT_STICK:
				return input::PadButton::LStick;
			case SDL_GAMEPAD_BUTTON_RIGHT_STICK:
				return input::PadButton::RStick;
			case SDL_GAMEPAD_BUTTON_DPAD_UP:
				return input::PadButton::DUp;
			case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
				return input::PadButton::DDown;
			case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
				return input::PadButton::DLeft;
			case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
				return input::PadButton::DRight;
			default:
				return std::nullopt; // the Guide button and the extras are the system's
			}
		}

		std::optional<input::PadAxis> ToPadAxis(Uint8 axis)
		{
			switch (axis)
			{
			case SDL_GAMEPAD_AXIS_LEFTX:
				return input::PadAxis::LeftX;
			case SDL_GAMEPAD_AXIS_LEFTY:
				return input::PadAxis::LeftY;
			case SDL_GAMEPAD_AXIS_RIGHTX:
				return input::PadAxis::RightX;
			case SDL_GAMEPAD_AXIS_RIGHTY:
				return input::PadAxis::RightY;
			case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:
				return input::PadAxis::LeftTrigger;
			case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:
				return input::PadAxis::RightTrigger;
			default:
				return std::nullopt;
			}
		}

		std::optional<input::MouseButton> ToMouseButton(Uint8 button)
		{
			switch (button)
			{
			case SDL_BUTTON_LEFT:
				return input::MouseButton::Left;
			case SDL_BUTTON_RIGHT:
				return input::MouseButton::Right;
			case SDL_BUTTON_MIDDLE:
				return input::MouseButton::Middle;
			default:
				return std::nullopt;
			}
		}
	} // namespace

	Application::SdlScope::SdlScope()
	{
		// Subsystems (video, audio) are started by the classes that use them;
		// this only initialises SDL's core so SDL_Quit below has something to pair with.
		core::RouteSdlLog(); // once the platform code starts, SDL's log goes through the engine's
		if (!SDL_Init(0))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
		}
	}

	Application::SdlScope::~SdlScope()
	{
		SDL_Quit();
	}

	Application::Application(const ApplicationConfig& config)
	    : window(config.title, config.windowWidth, config.windowHeight),
	      renderer(window, config.logicalWidth, config.logicalHeight, config.scaling),
	      updateStep(1.0 / config.updateRate), maxUpdatesPerFrame(config.maxUpdatesPerFrame),
	      display(graphics::Sanitized({.mode = graphics::WindowMode::Windowed,
	                                   .width = config.windowWidth,
	                                   .height = config.windowHeight,
	                                   .vsync = config.vsync,
	                                   .frameCap = config.frameCap,
	                                   .scaling = config.scaling}))
	{
		if (!config.vsync && !renderer.SetVSync(false)) display.vsync = true; // the driver insists on it

		// Controllers already plugged in arrive as events once this is on.
		if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD))
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_InitSubSystem(GAMEPAD) failed, no controllers: %s",
			            SDL_GetError());
		}
	}

	Application::~Application()
	{
		for (SDL_Gamepad* handle : padHandles)
		{
			if (handle != nullptr) SDL_CloseGamepad(handle);
		}
		SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
	}

	std::size_t Application::SlotOfPad(std::uint32_t id) const
	{
		for (std::size_t i = 0; i < input::kMaxPads; ++i)
		{
			if (padIds[i] == id && padHandles[i] != nullptr) return i;
		}
		return input::kMaxPads;
	}

	void Application::OpenPad(std::uint32_t id)
	{
		if (SlotOfPad(id) != input::kMaxPads) return; // already open
		std::size_t slot = 0;
		while (slot < input::kMaxPads && padHandles[slot] != nullptr)
		{
			++slot;
		}
		if (slot == input::kMaxPads) return; // all slots busy: this one is ignored

		SDL_Gamepad* handle = SDL_OpenGamepad(id);
		if (handle == nullptr)
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_OpenGamepad failed: %s", SDL_GetError());
			return;
		}
		padHandles[slot] = handle;
		padIds[slot] = id;
		const char* name = SDL_GetGamepadName(handle);
		input.pads[slot].Connect(FamilyOf(handle));
		SDL_Log("controller %zu connected: %s", slot, name != nullptr ? name : "(unnamed)");
	}

	void Application::ClosePad(std::uint32_t id)
	{
		const std::size_t slot = SlotOfPad(id);
		if (slot == input::kMaxPads) return;
		SDL_CloseGamepad(padHandles[slot]);
		padHandles[slot] = nullptr;
		padIds[slot] = 0;
		input.pads[slot].Disconnect();
		SDL_Log("controller %zu disconnected", slot);
	}

	int Application::Run(Game& game)
	{
		const core::SystemClock clock;
		return Run(game, clock);
	}

	int Application::Run(Game& game, const core::Clock& clock)
	{
		if (!window.IsValid() || !renderer.IsValid())
		{
			return 1;
		}

		// SDL3 sends no SDL_EVENT_TEXT_INPUT until text input is started on the window.
		if (!SDL_StartTextInput(window.GetWindowHandle()))
		{
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "SDL_StartTextInput failed, no typed text: %s", SDL_GetError());
		}

		core::FixedTimestep timestep(updateStep);
		timestep.SetMaxUpdates(maxUpdatesPerFrame);
		double previous = clock.Now();
		bool cursorShown = true;

		if (editor != nullptr) editor->Attach({window.GetWindowHandle(), renderer.GetRendererHandle()});

		UpdateDisplay(game); // so the game sees the display's settings and sizes from its first update

		bool automationDone = false; // the --screenshot-after picture has been taken
		int exitCode = 0;
		while (PumpEvents() && !game.WantsToQuit() && !automationDone)
		{
			const double now = clock.Now();
			const int updates = timestep.Advance(now - previous);
			previous = now;

			for (int i = 0; i < updates; ++i)
			{
				game.Update(timestep.Step(), input, &audio);
				input.EndUpdate();
			}
			audio.Update();

			const bool wantCursor = game.ShowSystemCursor() || (editor != nullptr && editor->WantsSystemCursor());
			if (wantCursor != cursorShown)
			{
				cursorShown = wantCursor;
				if (!(wantCursor ? SDL_ShowCursor() : SDL_HideCursor()))
				{
					SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "changing the mouse pointer failed: %s", SDL_GetError());
				}
			}

			game.Render(renderer, timestep.Alpha());
			if (editor != nullptr) editor->Draw();
			++framesDrawn;
			const bool automatic = screenshotAfter > 0 && framesDrawn == screenshotAfter;
			if (const ScreenshotLink screenshot = game.Screenshot();
			    automatic || screenshotAsked || (screenshot.request != nullptr && screenshot.request->pending))
			{
				const bool saved = SaveScreenshot(screenshot);
				if (automatic)
				{
					automationDone = true;
					if (!saved) exitCode = 1;
				}
			}
			renderer.Present();

			UpdateDisplay(game);
			if (!display.vsync)
			{
				// Without VSync nothing paces the loop, so wait out the rest of the frame.
				const double delay = graphics::FrameDelaySeconds(now, clock.Now(), display.frameCap);
				if (delay > 0.0) SDL_DelayNS(static_cast<Uint64>(delay * 1e9));
			}
		}

		if (editor != nullptr) editor->Detach();
		if (!cursorShown) SDL_ShowCursor(); // the pointer belongs to the user after the game
		return exitCode;
	}

	void Application::SetScreenshotArgs(const graphics::ScreenshotArgs& args)
	{
		if (!args.error.empty()) SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "bad option: %s", args.error.c_str());
		if (!args.directory.empty()) screenshotFolder = args.directory;
		if (args.afterFrames > 0) screenshotAfter = args.afterFrames;
	}

	bool Application::SaveScreenshot(const ScreenshotLink& link)
	{
		screenshotAsked = false;
		if (link.request != nullptr) link.request->pending = false;

		const auto fail = [&link](const std::string& why)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "screenshot failed: %s", why.c_str());
			if (link.result != nullptr) link.result->lastError = why;
		};
		if (screenshotFolder.empty())
		{
			fail("no folder to save screenshots in");
			return false;
		}
		std::error_code error;
		std::filesystem::create_directories(screenshotFolder, error);
		if (error)
		{
			fail("can't make " + screenshotFolder.string() + ": " + error.message());
			return false;
		}

		// With the editor the picture is the whole window, panels included; otherwise the game at its own size.
		const graphics::Image picture = editor != nullptr ? renderer.ReadWindowPixels() : renderer.ReadLogicalPixels();
		if (picture.IsEmpty())
		{
			fail("the frame could not be read back");
			return false;
		}

		graphics::ScreenshotTime time;
		SDL_Time now = 0;
		SDL_DateTime date{};
		if (SDL_GetCurrentTime(&now) && SDL_TimeToDateTime(now, &date, true))
		{
			time = {.year = date.year,
			        .month = date.month,
			        .day = date.day,
			        .hour = date.hour,
			        .minute = date.minute,
			        .second = date.second};
		}
		const std::filesystem::path file = graphics::UniqueScreenshotPath(screenshotFolder, time);
		if (!graphics::SaveImage(picture, file))
		{
			fail("can't write " + file.string());
			return false;
		}
		SDL_Log("screenshot saved: %s", file.string().c_str());
		if (link.result != nullptr)
		{
			link.result->lastFile = file;
			link.result->lastError.clear();
			++link.result->count;
		}
		return true;
	}

	void Application::ApplyDisplay(const graphics::DisplaySettings& wanted)
	{
		using graphics::WindowMode;

		const bool sizeMatters = wanted.mode != WindowMode::Borderless;
		if (wanted.mode != display.mode ||
		    (sizeMatters && (wanted.width != display.width || wanted.height != display.height)))
		{
			if (window.SetMode(wanted.mode, wanted.width, wanted.height))
			{
				display.mode = wanted.mode;
				display.width = wanted.width;
				display.height = wanted.height;
			}
		}
		if (wanted.scaling != display.scaling && renderer.SetScaling(wanted.scaling)) display.scaling = wanted.scaling;
		if (wanted.vsync != display.vsync && renderer.SetVSync(wanted.vsync)) display.vsync = wanted.vsync;
		display.frameCap = wanted.frameCap;
	}

	void Application::UpdateDisplay(Game& game)
	{
		const DisplayLink link = game.Display();
		if (link.request != nullptr && link.request->pending)
		{
			const graphics::DisplaySettings wanted = graphics::Sanitized(*link.request->pending);
			link.request->pending.reset();
			ApplyDisplay(wanted);
			displayInfoStale = true;
		}
		if (link.info != nullptr && displayInfoStale)
		{
			link.info->current = display;
			link.info->resolutions = window.Resolutions();
			displayInfoStale = false;
		}
	}

	void Application::SetMousePosition(float x, float y)
	{
		int width = 0;
		int height = 0;
		SDL_GetRenderLogicalPresentation(renderer.GetRendererHandle(), &width, &height, nullptr);
		const bool inside = x >= 0 && y >= 0 && x < static_cast<float>(width) && y < static_cast<float>(height);
		input.mouse.OnMove({x, y}, inside);
	}

	bool Application::PumpEvents()
	{
		bool open = true;
		SDL_Event event{};
		while (SDL_PollEvent(&event))
		{
			if (editor != nullptr && editor->OnEvent(&event)) continue;
			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				open = false;
				break;
			case SDL_EVENT_KEY_DOWN:
				if (const std::optional<input::Key> key = ToKey(event.key.scancode))
				{
					if (screenshotKey && *key == *screenshotKey && !event.key.repeat) screenshotAsked = true;
					input.keyboard.OnKeyDown(*key);
					input.lastUsed = input::Device::Keyboard;
				}
				break;
			case SDL_EVENT_KEY_UP:
				if (const std::optional<input::Key> key = ToKey(event.key.scancode))
				{
					if (!OtherPhysicalKeyHeld(*key, event.key.scancode)) input.keyboard.OnKeyUp(*key);
				}
				break;
			case SDL_EVENT_TEXT_INPUT:
				if (event.text.text != nullptr) input.OnText(event.text.text);
				break;
			case SDL_EVENT_MOUSE_MOTION:
				// Window pixels to logical pixels, so letterboxing and scaling are handled.
				SDL_ConvertEventToRenderCoordinates(renderer.GetRendererHandle(), &event);
				SetMousePosition(event.motion.x, event.motion.y);
				input.lastUsed = input::Device::Mouse;
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				SDL_ConvertEventToRenderCoordinates(renderer.GetRendererHandle(), &event);
				SetMousePosition(event.button.x, event.button.y);
				if (const std::optional<input::MouseButton> button = ToMouseButton(event.button.button))
				{
					input.mouse.OnButton(*button, event.button.down);
					if (event.button.down) input.lastUsed = input::Device::Mouse;
				}
				break;
			case SDL_EVENT_MOUSE_WHEEL:
				// Notches, positive away from the user, whatever the OS's "natural scrolling" says.
				input.mouse.OnWheel(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.y : event.wheel.y);
				input.lastUsed = input::Device::Mouse;
				break;
			case SDL_EVENT_GAMEPAD_ADDED:
				OpenPad(event.gdevice.which);
				break;
			case SDL_EVENT_GAMEPAD_REMOVED:
				ClosePad(event.gdevice.which);
				break;
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
			{
				const std::size_t slot = SlotOfPad(event.gbutton.which);
				const std::optional<input::PadButton> button = ToPadButton(event.gbutton.button);
				if (slot == input::kMaxPads || !button) break;
				input.pads[slot].OnButton(*button, event.gbutton.down);
				if (event.gbutton.down) input.lastUsed = input::Device::Pad;
				break;
			}
			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			{
				const std::size_t slot = SlotOfPad(event.gaxis.which);
				const std::optional<input::PadAxis> axis = ToPadAxis(event.gaxis.axis);
				if (slot == input::kMaxPads || !axis) break;
				// Sticks run -32768 to 32767, triggers 0 to 32767; both to -1..1 and 0..1.
				const float value = std::clamp(static_cast<float>(event.gaxis.value) / 32767.0f, -1.0f, 1.0f);
				if (input.pads[slot].OnAxis(*axis, value)) input.lastUsed = input::Device::Pad;
				break;
			}
			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				input.mouse.inside = false;
				break;
			case SDL_EVENT_WINDOW_RESIZED:
				// A framed window the player resizes by hand is the window size now.
				if (display.mode == graphics::WindowMode::Windowed)
				{
					display.width = event.window.data1;
					display.height = event.window.data2;
					displayInfoStale = true;
				}
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				// Key-ups for keys released while unfocused never arrive; don't leave them stuck.
				input.ReleaseAll();
				break;
			default:
				break;
			}
		}
		return open;
	}
} // namespace engine::application
