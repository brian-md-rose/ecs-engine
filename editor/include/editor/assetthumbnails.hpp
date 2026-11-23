#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <unordered_map>

#include "graphics/renderer.hpp"

struct SDL_Texture;

namespace engine::editor
{
	// The pictures of sprites for the Assets panel: each `.sheet` is loaded once, the first time one of its sprites is
	// asked for, and every sprite is uploaded as a texture of the renderer. Needs a renderer (SetRenderer, each frame
	// is fine); without one, or for a sheet that does not load, there is no picture and Error says why.
	class AssetThumbnails
	{
	public:
		struct Picture
		{
			SDL_Texture* texture = nullptr; // the whole strip, frames side by side; null if there is no picture
			int width = 0;
			int height = 0;
			int frames = 1;
		};

		// The renderer that owns the textures. It must outlive every Picture handed out; the textures are the
		// renderer's, so nothing is freed when this is destroyed.
		void SetRenderer(graphics::Renderer* rendererToUse);

		// The picture of sprite name in manifest file; empty (null texture) if it can't be had.
		[[nodiscard]] Picture Get(const std::filesystem::path& file, const std::string& name);
		// Why the manifest did not load, or "" if it did (or has not been asked for).
		[[nodiscard]] const std::string& Error(const std::filesystem::path& file) const;
		// Asks for every loaded sheet and texture to be dropped, so Get reads the files again (after Rescan or a save).
		// The textures are destroyed at the next SetRenderer, not now, since this frame's drawing may still use them.
		void Forget();

	private:
		struct Sheet
		{
			std::string error;
			std::unordered_map<std::string, graphics::TextureId> textures;
			std::unordered_map<std::string, Picture> pictures;
		};

		Sheet& Load(const std::filesystem::path& file);

		graphics::Renderer* renderer = nullptr;
		bool dropPending = false;
		std::map<std::filesystem::path, Sheet> sheets;
	};
} // namespace engine::editor
