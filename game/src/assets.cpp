#include "assets.hpp"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "core/text.hpp"
#include "graphics/image.hpp"

namespace game
{
	namespace
	{
		using engine::graphics::Image;
		using engine::graphics::PixelSheet;
		using engine::graphics::SheetSprite;

		// The sheet, or nothing with the error printed to stderr.
		[[nodiscard]] std::optional<PixelSheet> LoadSheet(const std::filesystem::path& file)
		{
			engine::graphics::PixelSheetResult result = engine::graphics::LoadPixelSheet(file);
			if (!result.sheet)
			{
				const std::string message = engine::core::FormatError(engine::core::PathText(file), result.error);
				std::fprintf(stderr, "%s\n", message.c_str());
			}
			return std::move(result.sheet);
		}
	} // namespace

	GameAssets BuildGameAssets(const PixelSheet& fontSheet)
	{
		std::vector<const Image*> images;
		for (const SheetSprite& sprite : fontSheet.sprites)
		{
			images.push_back(&sprite.image);
		}

		GameAssets assets;
		assets.atlas = engine::graphics::PackAtlas(images);
		assets.font.advance = kGlyphAdvance;
		assets.font.lineHeight = kGlyphLineHeight;
		for (std::size_t i = 0; i < fontSheet.sprites.size(); ++i)
		{
			const SheetSprite& sprite = fontSheet.sprites[i];
			if (sprite.name.size() == 1) assets.font.SetGlyph(sprite.name[0], assets.atlas.regions[i]);
		}
		return assets;
	}

	std::optional<GameAssets> LoadGameAssets(const std::filesystem::path& folder)
	{
		const std::optional<PixelSheet> font = LoadSheet(folder / "sprites" / "font.txt");
		if (!font) return std::nullopt;
		return BuildGameAssets(*font);
	}
} // namespace game
