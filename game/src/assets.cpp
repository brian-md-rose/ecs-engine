#include "assets.hpp"

#include <string>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "core/text.hpp"
#include "graphics/image.hpp"
#include "graphics/sheetmanifest.hpp"

namespace game
{
	namespace
	{
		using engine::graphics::Clip;
		using engine::graphics::Image;
		using engine::graphics::PixelSheet;
		using engine::graphics::SheetSprite;

		// Seconds per frame of each animation.
		constexpr float kWalkFrameSeconds = 0.15f;
		constexpr float kDeathFrameSeconds = 0.3f; // four frames, over before Dying ends
		constexpr float kEnemyFrameSeconds = 0.2f;
		constexpr float kFrightenedFrameSeconds = 0.12f; // a nervous shiver
		constexpr float kReturningFrameSeconds = 0.1f;
		constexpr float kPowerPelletFrameSeconds = 0.3f;

		constexpr std::string_view kWallCore = "wall_core";
		constexpr std::array<std::string_view, 4> kWallEdges = {
		    {"wall_edge_up", "wall_edge_right", "wall_edge_down", "wall_edge_left"}};

		// The sprites of sprites.txt the game draws, besides the wall pieces.
		constexpr std::array<std::string_view, 19> kSpriteNames = {
		    {"player_right", "player_down", "player_up", "player_death", "grit", "snare", "clasp", "wisp", "frightened",
		     "frightened_flash", "returning", "pellet", "power_pellet", "door", "panel", "button", "button_hover",
		     "button_pressed", "cursor"}};

		// The sheet, or nothing with the error logged.
		[[nodiscard]] std::optional<PixelSheet> LoadSheet(const std::filesystem::path& file)
		{
			engine::graphics::PixelSheetResult result = engine::graphics::LoadSheetFile(file);
			if (!result.sheet)
			{
				const std::string message = engine::core::FormatError(engine::core::PathText(file), result.error);
				engine::core::LogError(message);
			}
			return std::move(result.sheet);
		}

		// folder/name.sheet (PNGs) if there is one, else folder/name.txt (the fallback).
		[[nodiscard]] std::filesystem::path SheetPath(const std::filesystem::path& folder, const char* name)
		{
			std::filesystem::path manifest = folder / (std::string(name) + ".sheet");
			if (std::filesystem::exists(manifest)) return manifest;
			return folder / (std::string(name) + ".txt");
		}

		// Where each sprite of the sprite sheet went in the atlas, by name.
		class Regions
		{
			const PixelSheet& sheet;
			const std::vector<engine::graphics::PixelRect>& regions; // in sheet order

		public:
			Regions(const PixelSheet& spriteSheet, const std::vector<engine::graphics::PixelRect>& sheetRegions)
			    : sheet(spriteSheet), regions(sheetRegions)
			{
			}

			// The sprite's frames as a clip; empty if the sheet has no such sprite.
			[[nodiscard]] Clip Get(std::string_view name, float secondsPerFrame, bool loop = true) const
			{
				for (std::size_t i = 0; i < sheet.sprites.size(); ++i)
				{
					const SheetSprite& sprite = sheet.sprites[i];
					if (sprite.name != name) continue;
					engine::graphics::PixelRect first = regions[i];
					first.width = sprite.frameWidth;
					return Clip{
					    .firstFrame = first, .frames = sprite.frames, .secondsPerFrame = secondsPerFrame, .loop = loop};
				}
				return Clip{};
			}
		};
	} // namespace

	std::vector<std::string> MissingSprites(const PixelSheet& spriteSheet)
	{
		std::vector<std::string> missing;
		const auto check = [&](std::string_view name)
		{
			if (spriteSheet.Find(name) == nullptr) missing.emplace_back(name);
		};
		for (const std::string_view name : kSpriteNames)
		{
			check(name);
		}
		check(kWallCore);
		for (const std::string_view name : kWallEdges)
		{
			check(name);
		}
		return missing;
	}

	GameAssets BuildGameAssets(const PixelSheet& fontSheet, const PixelSheet& spriteSheet)
	{
		// The wall tiles, built from the pieces that are there.
		WallPieces pieces;
		const Image blank(8, 8);
		const SheetSprite* core = spriteSheet.Find(kWallCore);
		pieces.core = core != nullptr ? &core->image : &blank;
		for (std::size_t i = 0; i < kWallEdges.size(); ++i)
		{
			const SheetSprite* edge = spriteSheet.Find(kWallEdges[i]);
			pieces.edges[i] = edge != nullptr ? &edge->image : nullptr;
		}
		const std::array<Image, kWallTileCount> wallTiles = BuildWallTiles(pieces);

		// Everything into one atlas: the font, then the sprite sheet, then the wall tiles.
		std::vector<const Image*> images;
		for (const SheetSprite& sprite : fontSheet.sprites)
		{
			images.push_back(&sprite.image);
		}
		for (const SheetSprite& sprite : spriteSheet.sprites)
		{
			images.push_back(&sprite.image);
		}
		for (const Image& tile : wallTiles)
		{
			images.push_back(&tile);
		}

		GameAssets assets;
		assets.atlas = engine::graphics::PackAtlas(images);
		const std::vector<engine::graphics::PixelRect>& regions = assets.atlas.regions;

		assets.font.advance = kGlyphAdvance;
		assets.font.lineHeight = kGlyphLineHeight;
		for (std::size_t i = 0; i < fontSheet.sprites.size(); ++i)
		{
			const SheetSprite& sprite = fontSheet.sprites[i];
			if (sprite.name.size() == 1) assets.font.SetGlyph(sprite.name[0], regions[i]);
		}

		const std::vector<engine::graphics::PixelRect> spriteRegions(
		    regions.begin() + static_cast<std::ptrdiff_t>(fontSheet.sprites.size()),
		    regions.begin() + static_cast<std::ptrdiff_t>(fontSheet.sprites.size() + spriteSheet.sprites.size()));
		const Regions sprites(spriteSheet, spriteRegions);
		SpriteClips& clips = assets.clips;
		clips.playerRight = sprites.Get("player_right", kWalkFrameSeconds);
		clips.playerDown = sprites.Get("player_down", kWalkFrameSeconds);
		clips.playerUp = sprites.Get("player_up", kWalkFrameSeconds);
		clips.playerDeath = sprites.Get("player_death", kDeathFrameSeconds, false);
		for (std::size_t i = 0; i < kPersonalityCount; ++i)
		{
			clips.enemies[i] = sprites.Get(kPersonalities[i].sprite, kEnemyFrameSeconds);
		}
		clips.frightened = sprites.Get("frightened", kFrightenedFrameSeconds);
		clips.frightenedFlash = sprites.Get("frightened_flash", kFrightenedFrameSeconds);
		clips.returning = sprites.Get("returning", kReturningFrameSeconds);
		clips.pellet = sprites.Get("pellet", 0.0f);
		clips.powerPellet = sprites.Get("power_pellet", kPowerPelletFrameSeconds);
		clips.door = sprites.Get("door", 0.0f);
		clips.panel = sprites.Get("panel", 0.0f);
		clips.button = sprites.Get("button", 0.0f);
		clips.buttonHover = sprites.Get("button_hover", 0.0f);
		clips.buttonPressed = sprites.Get("button_pressed", 0.0f);
		clips.cursor = sprites.Get("cursor", 0.0f);

		const std::size_t firstWall = fontSheet.sprites.size() + spriteSheet.sprites.size();
		for (std::size_t mask = 0; mask < kWallTileCount; ++mask)
		{
			clips.walls[mask] = Clip{.firstFrame = regions[firstWall + mask], .frames = 1, .secondsPerFrame = 0.0f};
		}
		return assets;
	}

	std::optional<GameAssets> LoadGameAssets(const std::filesystem::path& folder)
	{
		const std::optional<PixelSheet> font = LoadSheet(SheetPath(folder / "sprites", "font"));
		const std::filesystem::path spriteFile = SheetPath(folder / "sprites", "sprites");
		const std::optional<PixelSheet> sprites = LoadSheet(spriteFile);
		if (!font || !sprites) return std::nullopt;

		const std::vector<std::string> missing = MissingSprites(*sprites);
		if (!missing.empty())
		{
			std::string list;
			for (const std::string& name : missing)
			{
				list += (list.empty() ? "" : ", ") + name;
			}
			engine::core::LogError(engine::core::PathText(spriteFile) + ": missing sprites: " + list);
			return std::nullopt;
		}

		const std::filesystem::path levels = folder / "levels.txt";
		DifficultyResult difficulty = LoadDifficulty(levels);
		if (!difficulty.table)
		{
			const std::string message = engine::core::FormatError(engine::core::PathText(levels), difficulty.error);
			engine::core::LogError(message);
			return std::nullopt;
		}

		const std::filesystem::path sceneFile = folder / "scenes" / "uidemo.json";
		engine::core::JsonResult scene = engine::core::LoadJson(sceneFile);
		if (!scene.value)
		{
			engine::core::LogError(engine::core::FormatError(engine::core::PathText(sceneFile), scene.error));
			return std::nullopt;
		}

		GameAssets assets = BuildGameAssets(*font, *sprites);
		assets.difficulty = std::move(*difficulty.table);
		assets.uiDemoScene = std::move(*scene.value);
		return assets;
	}
} // namespace game
