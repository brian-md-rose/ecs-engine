#include "editor/assetcatalog.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/text.hpp"

namespace engine::editor
{
	namespace
	{
		using ecs::AssetKind;

		const std::vector<AssetEntry> kNone;

		std::string Lower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](char c)
			{
				return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
			});
			return text;
		}

		std::string Slashes(const std::filesystem::path& path)
		{
			return path.generic_string();
		}

		std::string FrameDetail(std::string_view width, std::string_view height, std::string_view frames)
		{
			std::string text = std::string(width) + " x " + std::string(height);
			const auto count = core::ParseInt(frames);
			if (count && *count > 1) text += ", " + std::string(frames) + " frames";
			return text;
		}

		// The entries of one manifest, added to sprites or fonts.
		void ReadSheet(const std::filesystem::path& file, std::vector<AssetEntry>& sprites,
		               std::vector<AssetEntry>& fonts)
		{
			const auto text = core::ReadTextFile(file);
			if (!text) return;
			std::vector<AssetEntry> found;
			bool allGlyphs = true;
			std::size_t regions = 0;
			for (const std::string_view line : core::SplitLines(*text))
			{
				const std::vector<core::Word> words = core::SplitWords(line);
				if (words.size() < 2 || words[0].text.starts_with('#')) continue;
				if (words[0].text == "sprite" && words.size() >= 5)
				{
					found.push_back(
					    {.name = std::string(words[1].text),
					     .file = file,
					     .detail = FrameDetail(words[3].text, words[4].text, words.size() > 5 ? words[5].text : "1")});
					allGlyphs = false;
				}
				else if (words[0].text == "region" && words.size() >= 6)
				{
					found.push_back(
					    {.name = std::string(words[1].text),
					     .file = file,
					     .detail = FrameDetail(words[4].text, words[5].text, words.size() > 6 ? words[6].text : "1")});
					++regions;
					if (words[1].text.size() != 1) allGlyphs = false;
				}
			}
			if (found.empty()) return;
			if (regions == found.size() && allGlyphs)
			{
				fonts.push_back(
				    {.name = file.stem().string(), .file = file, .detail = std::to_string(found.size()) + " glyphs"});
				return;
			}
			for (AssetEntry& entry : found)
				sprites.push_back(std::move(entry));
		}

		const char* SoundFormat(const std::string& extension)
		{
			if (extension == ".ogg") return "Ogg Vorbis";
			if (extension == ".wav") return "WAV";
			if (extension == ".flac") return "FLAC";
			if (extension == ".mp3") return "MP3";
			return nullptr;
		}

		void SortByName(std::vector<AssetEntry>& entries)
		{
			std::stable_sort(entries.begin(), entries.end(), [](const AssetEntry& a, const AssetEntry& b)
			{
				return a.name < b.name;
			});
		}

		// The names of sorted entries, each once (two sheets may name a sprite the same).
		std::vector<std::string> UniqueNames(const std::vector<AssetEntry>& entries)
		{
			std::vector<std::string> names;
			for (const AssetEntry& entry : entries)
				if (names.empty() || names.back() != entry.name) names.push_back(entry.name);
			return names;
		}
	} // namespace

	AssetCatalog::AssetCatalog(std::filesystem::path assetsFolder) : folder(std::move(assetsFolder))
	{
		Rescan();
	}

	void AssetCatalog::Rescan()
	{
		sprites.clear();
		fonts.clear();
		sounds.clear();
		spriteNames.clear();
		fontNames.clear();
		soundNames.clear();
		std::error_code error;
		if (folder.empty() || !std::filesystem::is_directory(folder, error)) return;
		for (std::filesystem::recursive_directory_iterator it(folder, error), end; !error && it != end;
		     it.increment(error))
		{
			if (!it->is_regular_file(error)) continue;
			const std::filesystem::path& file = it->path();
			const std::string extension = Lower(file.extension().string());
			if (extension == ".sheet")
			{
				ReadSheet(file, sprites, fonts);
			}
			else if (const char* format = SoundFormat(extension))
			{
				sounds.push_back({.name = Slashes(file.lexically_relative(folder)), .file = file, .detail = format});
			}
		}
		SortByName(sprites);
		SortByName(fonts);
		SortByName(sounds);
		spriteNames = UniqueNames(sprites);
		fontNames = UniqueNames(fonts);
		soundNames = UniqueNames(sounds);
	}

	const std::vector<AssetEntry>& AssetCatalog::Entries(AssetKind kind) const noexcept
	{
		switch (kind)
		{
		case AssetKind::Sprite:
			return sprites;
		case AssetKind::Font:
			return fonts;
		case AssetKind::Sound:
			return sounds;
		case AssetKind::None:
			break;
		}
		return kNone;
	}

	const std::vector<std::string>& AssetCatalog::Names(AssetKind kind) const noexcept
	{
		static const std::vector<std::string> none;
		switch (kind)
		{
		case AssetKind::Sprite:
			return spriteNames;
		case AssetKind::Font:
			return fontNames;
		case AssetKind::Sound:
			return soundNames;
		case AssetKind::None:
			break;
		}
		return none;
	}

	bool AssetCatalog::Contains(AssetKind kind, const std::string& name) const
	{
		const std::vector<AssetEntry>& entries = Entries(kind);
		return std::any_of(entries.begin(), entries.end(), [&name](const AssetEntry& e)
		{
			return e.name == name;
		});
	}

	const char* AssetKindName(AssetKind kind) noexcept
	{
		switch (kind)
		{
		case AssetKind::Sprite:
			return "Sprites";
		case AssetKind::Font:
			return "Fonts";
		case AssetKind::Sound:
			return "Sounds";
		case AssetKind::None:
			break;
		}
		return "";
	}
} // namespace engine::editor
