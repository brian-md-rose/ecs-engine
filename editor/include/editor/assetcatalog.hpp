#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

#include "ecs/typeregistry.hpp"

namespace engine::editor
{
	// One asset a field can name: what to type in the field, where it comes from and a short description.
	struct AssetEntry
	{
		std::string name;           // sprites: the sprite's name; fonts: the sheet's file name without extension;
		                            // sounds: the path inside the assets folder, with '/' (music/theme.ogg)
		std::filesystem::path file; // the manifest or sound file, as found
		std::string detail;         // "8 x 8, 2 frames", "95 glyphs", "Ogg Vorbis"
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
		[[nodiscard]] std::vector<std::string> Names(ecs::AssetKind kind) const;
		[[nodiscard]] bool Contains(ecs::AssetKind kind, const std::string& name) const;

	private:
		std::filesystem::path folder;
		std::vector<AssetEntry> sprites;
		std::vector<AssetEntry> fonts;
		std::vector<AssetEntry> sounds;
	};

	[[nodiscard]] const char* AssetKindName(ecs::AssetKind kind) noexcept;
} // namespace engine::editor
