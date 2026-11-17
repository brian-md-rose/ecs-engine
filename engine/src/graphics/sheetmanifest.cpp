#include "graphics/sheetmanifest.hpp"

#include <array>
#include <utility>

#include "graphics/imagefile.hpp"

namespace engine::graphics
{
	namespace
	{
		using core::ParseError;
		using core::Word;

		constexpr int kMaxSide = 1024; // as in pixel sheets: keeps a typo from allocating gigabytes

		[[nodiscard]] PixelSheetResult Fail(std::size_t line, std::size_t column, std::string message)
		{
			return {.sheet = std::nullopt, .error = {.line = line, .column = column, .message = std::move(message)}};
		}

		[[nodiscard]] bool IsBlankOrComment(std::string_view line)
		{
			const std::size_t first = line.find_first_not_of(" \t");
			return first == std::string_view::npos || line[first] == '#';
		}

		[[nodiscard]] std::string Quote(std::string_view text)
		{
			return '\'' + std::string(text) + '\'';
		}

		struct Parser
		{
			std::filesystem::path folder;
			PixelSheet sheet{};
			std::optional<Image> current; // the latest `image`
			std::string currentName;

			// A whole number in [min, max] from a word, or the error.
			[[nodiscard]] static std::optional<ParseError> Number(std::size_t line, const Word& word, int min, int max,
			                                                      int& out)
			{
				const std::optional<int> value = core::ParseInt(word.text);
				if (!value || *value < min || *value > max)
				{
					return ParseError{line, word.column,
					                  "expected a whole number from " + std::to_string(min) + " to " +
					                      std::to_string(max) + ", not " + Quote(word.text)};
				}
				out = *value;
				return std::nullopt;
			}

			[[nodiscard]] std::optional<ParseError> Load(std::size_t line, const Word& file, Image& out) const
			{
				ImageResult result = LoadImage(folder / std::filesystem::path(std::string(file.text)));
				if (!result.image)
				{
					return ParseError{line, file.column, Quote(file.text) + ": " + result.error};
				}
				out = std::move(*result.image);
				return std::nullopt;
			}

			[[nodiscard]] std::optional<ParseError> Unique(std::size_t line, const Word& name) const
			{
				if (sheet.Find(name.text) == nullptr) return std::nullopt;
				return ParseError{line, name.column, "sprite " + Quote(name.text) + " is already defined"};
			}

			// sprite <name> <png> <frame width> <frame height> [frames]
			[[nodiscard]] std::optional<ParseError> Sprite(std::size_t line, const std::vector<Word>& words)
			{
				if (words.size() != 5 && words.size() != 6)
				{
					return ParseError{line, words[0].column,
					                  "expected: sprite <name> <png> <frame width> <frame height> [frames]"};
				}
				if (auto error = Unique(line, words[1])) return error;
				std::array<int, 3> sizes{0, 0, 1};
				for (std::size_t i = 3; i < words.size(); ++i)
				{
					if (auto error = Number(line, words[i], 1, kMaxSide, sizes[i - 3])) return error;
				}
				Image image;
				if (auto error = Load(line, words[2], image)) return error;

				const auto [frameWidth, frameHeight, frames] = sizes;
				if (image.Width() != frameWidth * frames || image.Height() != frameHeight)
				{
					return ParseError{line, words[2].column,
					                  Quote(words[2].text) + " is " + std::to_string(image.Width()) + " x " +
					                      std::to_string(image.Height()) + " pixels, but " + std::to_string(frames) +
					                      " frame(s) of " + std::to_string(frameWidth) + " x " +
					                      std::to_string(frameHeight) + " need " + std::to_string(frameWidth * frames) +
					                      " x " + std::to_string(frameHeight)};
				}
				sheet.sprites.push_back({.name = std::string(words[1].text),
				                         .frameWidth = frameWidth,
				                         .frameHeight = frameHeight,
				                         .frames = frames,
				                         .image = std::move(image)});
				return std::nullopt;
			}

			// image <png>
			[[nodiscard]] std::optional<ParseError> ImageLine(std::size_t line, const std::vector<Word>& words)
			{
				if (words.size() != 2) return ParseError{line, words[0].column, "expected: image <png>"};
				Image image;
				if (auto error = Load(line, words[1], image)) return error;
				current = std::move(image);
				currentName = std::string(words[1].text);
				return std::nullopt;
			}

			// region <name> <x> <y> <frame width> <frame height> [frames]
			[[nodiscard]] std::optional<ParseError> Region(std::size_t line, const std::vector<Word>& words)
			{
				if (words.size() != 6 && words.size() != 7)
				{
					return ParseError{line, words[0].column,
					                  "expected: region <name> <x> <y> <frame width> <frame height> [frames]"};
				}
				if (!current) return ParseError{line, words[0].column, "a region needs an 'image' line before it"};
				if (auto error = Unique(line, words[1])) return error;

				std::array<int, 5> numbers{0, 0, 0, 0, 1}; // x, y, frame width, frame height, frames
				for (std::size_t i = 2; i < words.size(); ++i)
				{
					if (auto error = Number(line, words[i], i < 4 ? 0 : 1, kMaxSide, numbers[i - 2])) return error;
				}
				const auto [x, y, frameWidth, frameHeight, frames] = numbers;
				const int width = frameWidth * frames;
				if (x + width > current->Width() || y + frameHeight > current->Height())
				{
					return ParseError{line, words[0].column,
					                  "the region is outside " + Quote(currentName) + ", which is " +
					                      std::to_string(current->Width()) + " x " + std::to_string(current->Height()) +
					                      " pixels"};
				}
				Image image(width, frameHeight);
				for (int row = 0; row < frameHeight; ++row)
				{
					for (int column = 0; column < width; ++column)
					{
						image.Set(column, row, current->At(x + column, y + row));
					}
				}
				sheet.sprites.push_back({.name = std::string(words[1].text),
				                         .frameWidth = frameWidth,
				                         .frameHeight = frameHeight,
				                         .frames = frames,
				                         .image = std::move(image)});
				return std::nullopt;
			}
		};
	} // namespace

	PixelSheetResult ParseSheetManifest(std::string_view text, const std::filesystem::path& folder)
	{
		Parser parser{.folder = folder};
		const std::vector<std::string_view> lines = core::SplitLines(text);
		for (std::size_t i = 0; i < lines.size(); ++i)
		{
			if (IsBlankOrComment(lines[i])) continue;

			const std::size_t line = i + 1;
			const std::vector<Word> words = core::SplitWords(lines[i]);
			std::optional<ParseError> error;
			if (words[0].text == "sprite")
			{
				error = parser.Sprite(line, words);
			}
			else if (words[0].text == "image")
			{
				error = parser.ImageLine(line, words);
			}
			else if (words[0].text == "region")
			{
				error = parser.Region(line, words);
			}
			else
			{
				return Fail(line, words[0].column,
				            "expected 'sprite', 'image' or 'region', not " + Quote(words[0].text));
			}
			if (error) return {.sheet = std::nullopt, .error = std::move(*error)};
		}
		return {.sheet = std::move(parser.sheet), .error = {}};
	}

	PixelSheetResult LoadSheetManifest(const std::filesystem::path& file)
	{
		const std::optional<std::string> text = core::ReadTextFile(file);
		if (!text) return Fail(0, 0, "can't read the file");
		return ParseSheetManifest(*text, file.parent_path());
	}

	PixelSheetResult LoadSheetFile(const std::filesystem::path& file)
	{
		if (file.extension() == ".sheet") return LoadSheetManifest(file);
		return LoadPixelSheet(file);
	}
} // namespace engine::graphics
