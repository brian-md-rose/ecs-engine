#pragma once

// A hook for an editor overlay drawn on top of a running game (see docs/editor.md). Application calls the
// host, if one is set; a game without one pays nothing. The interface has no SDL types, so a header that
// includes it needs no SDL include path: the window, renderer and events cross it as opaque pointers, and
// the host (the engine::editor library, which links SDL itself) casts them back.

namespace engine::application
{
	// The platform objects behind an Application, as opaque pointers.
	struct EditorSurface
	{
		void* window = nullptr;   // an SDL_Window*
		void* renderer = nullptr; // the SDL_Renderer* the game draws with
	};

	class EditorHost
	{
	public:
		virtual ~EditorHost() = default;

		// Called once, right before the main loop starts, with SDL up and the window open.
		virtual void Attach(const EditorSurface& surface) = 0;

		// Called once after the main loop ends, while SDL is still running.
		virtual void Detach() = 0;

		// Called for every SDL event (an SDL_Event*) before the game sees it. Returns true when the host used the
		// event and the game must not see it, such as a click on one of the editor's windows. Close requests
		// should always return false.
		virtual bool OnEvent(const void* sdlEvent) = 0;

		// Asked each frame: true shows the operating system's mouse pointer even when the game hides it, so the
		// editor's windows can be used.
		[[nodiscard]] virtual bool WantsSystemCursor() const { return false; }

		// Called once per frame after the game has drawn and before the frame is presented. The host draws
		// its windows here, on top of the game.
		virtual void Draw() = 0;
	};
} // namespace engine::application
