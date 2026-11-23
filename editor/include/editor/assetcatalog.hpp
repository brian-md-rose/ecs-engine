#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ecs/typeregistry.hpp"

namespace engine::editor
{
	// The numbers of a `sprite` or `region` line of a manifest, which the Assets panel edits as "import settings".
	// x and y are only used by a region (where it starts in the image).
	struct SpriteSettings
	{
		int x = 0;
		int y = 0;
		int frameWidth = 0;
		int frameHeight = 0;
		int frames = 1;

		friend bool operator==(const SpriteSettings&, const SpriteSettings&) = default;
	};

	// One asset a field can name: what to type in the field, where it comes from and a short description.
	struct AssetEntry
	{
		std::string name;           // sprites: the sprite's name; fonts: the sheet's file name without extension;
		                            // sounds: the path inside the assets folder, with '/' (music/theme.ogg)
		std::filesystem::path file; // the manifest or sound file, as found
		std::string detail;         // "8 x 8, 2 frames", "95 glyphs", "Ogg Vorbis"
		// Sprites only: where the sprite is written, so its numbers can be edited.
		std::size_t line = 0; // 1-based line of the manifest, 0 if not a sprite
		bool region = false;  // a `region` line (cut from an image) rather than a `sprite` line (a PNG of its own)
		SpriteSettings settings{};
	};

	// The sprites, fonts and sounds found under an assets folder, for the Assets panel and for the Inspector's
	// pick-lists (FieldHints::assetKind). Found by scanning, not by asking the game, so a new file shows after
	// Rescan without code changes.
	//
	//  - Sprites: every `sprite` line of a `.sheet` manifest, and every `region` line of a sheet that is not a font.
	//  - Fonts: a `.sheet` whose regions are all single characters (a glyph sheet).
	//  - Sounds: `.ogg`, `.wav`, `.flac` and `.mp3` files, searched in every folder under the assets folder.
	//
	// Text sheets (`.txt`) are the game's fallback and are not listed.
	class AssetCatalog
	{
	public:
		AssetCatalog() = default;
		explicit AssetCatalog(std::filesystem::path assetsFolder);

		// Looks at the folder again. Entries are sorted by name.
		void Rescan();

		[[nodiscard]] const std::filesystem::path& Folder() const noexcept { return folder; }
		[[nodiscard]] const std::vector<AssetEntry>& Entries(ecs::AssetKind kind) const noexcept;
		// The names of Entries(kind), without repeats, kept from the last Rescan (cheap to call every frame).
		[[nodiscard]] const std::vector<std::string>& Names(ecs::AssetKind kind) const noexcept;
		[[nodiscard]] bool Contains(ecs::AssetKind kind, const std::string& name) const;

	private:
		std::filesystem::path folder;
		std::vector<std::string> spriteNames;
		std::vector<std::string> fontNames;
		std::vector<std::string> soundNames;
		std::vector<AssetEntry> sprites;
		std::vector<AssetEntry> fonts;
		std::vector<AssetEntry> sounds;
	};

	// The manifest text with line `line` (1-based, a sprite or region line) rewritten to use settings; the keyword,
	// name and PNG stay, and so do all other lines. nullopt if that line is not a sprite or region line.
	[[nodiscard]] std::optional<std::string> ApplySpriteSettings(std::string_view text, std::size_t line,
	                                                             const SpriteSettings& settings);

	// Writes settings into line `line` of the manifest file. Nothing is written, and the reason is returned, if the
	// line is not a sprite line or the edited manifest would not load (a rectangle outside the image, a frame size that
	// does not match the PNG). An empty string means it worked.
	[[nodiscard]] std::string SaveSpriteSettings(const std::filesystem::path& file, std::size_t line,
	                                             const SpriteSettings& settings);

	[[nodiscard]] const char* AssetKindName(ecs::AssetKind kind) noexcept;
} // namespace engine::editor
