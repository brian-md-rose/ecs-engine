#pragma once
#include <filesystem>
#include <string_view>

#include "graphics/pixelsheet.hpp"

// Sprites drawn in an image editor: a manifest (.sheet) names each sprite and says where its
// pixels are in a PNG. The result is the same PixelSheet a text sheet gives, so the rest of the
// engine can't tell them apart. The format (docs/pixel-sheets.md):
//
//     # A comment. Blank lines and comments may go between entries.
//     sprite player sprites/player.png 8 8 2    name, PNG file, frame width, frame height, frames (1)
//     image sprites/font.png                    the PNG that the regions below are cut from
//     region A 0 0 5 7 [frames]                 name, x, y, frame width, frame height, frames (1)
//
// A `sprite` is a PNG of its own, frames * frame width by frame height pixels. A `region` is
// that rectangle of the latest `image`. PNG paths are relative to the manifest.
namespace engine::graphics
{
	// Parses a manifest, reading its PNGs from folder. Sprite names must be unique.
	[[nodiscard]] PixelSheetResult ParseSheetManifest(std::string_view text, const std::filesystem::path& folder);

	// Reads a manifest file; its PNGs are looked up next to it.
	[[nodiscard]] PixelSheetResult LoadSheetManifest(const std::filesystem::path& file);

	// Loads either kind of sheet: a manifest if the file ends in .sheet, else a text pixel sheet.
	[[nodiscard]] PixelSheetResult LoadSheetFile(const std::filesystem::path& file);
} // namespace engine::graphics
