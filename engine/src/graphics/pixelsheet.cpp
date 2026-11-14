#include "graphics/pixelsheet.hpp"

#include <array>
#include <utility>

namespace engine::graphics
{
	namespace
	{
		using core::ParseError;
		using core::Word;

		constexpr char kTransparent = '.';
		constexpr int kMaxSide = 1024; // larger than any sprite; keeps a typo from allocating gigabytes

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
			std::vector<std::string_view> lines;
			std::size_t next = 0; // index of the next line to read
			std::array<std::optional<Rgba>, 256> palette{};
			PixelSheet sheet;

			[[nodiscard]] std::size_t LineNumber() const noexcept { return next; } // of the line just read

			// A colour line; nothing on success.
			[[nodiscard]] std::optional<ParseError> Colour(const std::vector<Word>& words)
			{
				const std::size_t line = LineNumber();
				if (words.size() != 5 && words.size() != 6)
				{
					return ParseError{line, words[0].column,
					                  "expected: color <character> <red> <green> <blue> [alpha]"};
				}
				const Word& key = words[1];
				if (key.text.size() != 1)
				{
					return ParseError{line, key.column, "a colour is one character, not " + Quote(key.text)};
				}
				if (key.text[0] == kTransparent)
				{
					return ParseError{line, key.column, "'.' is always transparent and can't be given a colour"};
				}
				std::optional<Rgba>& slot = palette[static_cast<unsigned char>(key.text[0])];
				if (slot)
				{
					return ParseError{line, key.column, "colour " + Quote(key.text) + " is already defined"};
				}

				std::array<std::uint8_t, 4> channels{0, 0, 0, 255};
				for (std::size_t i = 2; i < words.size(); ++i)
				{
					const std::optional<int> value = core::ParseInt(words[i].text);
					if (!value || *value < 0 || *value > 255)
					{
						return ParseError{line, words[i].column,
						                  "expected a channel from 0 to 255, not " + Quote(words[i].text)};
					}
					channels[i - 2] = static_cast<std::uint8_t>(*value);
				}
				slot = Rgba{channels[0], channels[1], channels[2], channels[3]};
				return std::nullopt;
			}

			// A sprite header and its rows; nothing on success.
			[[nodiscard]] std::optional<ParseError> Sprite(const std::vector<Word>& words)
			{
				const std::size_t line = LineNumber();
				if (words.size() != 4 && words.size() != 5)
				{
					return ParseError{line, words[0].column,
					                  "expected: sprite <name> <frame width> <frame height> [frames]"};
				}
				const Word& name = words[1];
				if (sheet.Find(name.text) != nullptr)
				{
					return ParseError{line, name.column, "sprite " + Quote(name.text) + " is already defined"};
				}

				std::array<int, 3> sizes{0, 0, 1};
				for (std::size_t i = 2; i < words.size(); ++i)
				{
					const std::optional<int> value = core::ParseInt(words[i].text);
					if (!value || *value < 1 || *value > kMaxSide)
					{
						return ParseError{line, words[i].column,
						                  "expected a whole number from 1 to " + std::to_string(kMaxSide) + ", not " +
						                      Quote(words[i].text)};
					}
					sizes[i - 2] = *value;
				}
				const auto [frameWidth, frameHeight, frames] = sizes;
				const int width = frameWidth * frames;
				if (width > kMaxSide)
				{
					return ParseError{line, words[0].column,
					                  "the sprite is wider than " + std::to_string(kMaxSide) +
					                      " pixels; use fewer or narrower frames"};
				}

				Image image(width, frameHeight);
				for (int y = 0; y < frameHeight; ++y)
				{
					if (next >= lines.size())
					{
						return ParseError{line, 0,
						                  "sprite " + Quote(name.text) + " needs " + std::to_string(frameHeight) +
						                      " rows; the file ends after " + std::to_string(y)};
					}
					const std::string_view row = lines[next++];
					const std::size_t rowLine = LineNumber();
					if (row.size() != static_cast<std::size_t>(width))
					{
						return ParseError{rowLine, 0,
						                  "a row of sprite " + Quote(name.text) + " must be " + std::to_string(width) +
						                      " characters, not " + std::to_string(row.size())};
					}
					for (int x = 0; x < width; ++x)
					{
						const char character = row[static_cast<std::size_t>(x)];
						if (character == kTransparent) continue;
						const std::optional<Rgba>& colour = palette[static_cast<unsigned char>(character)];
						if (!colour)
						{
							return ParseError{rowLine, static_cast<std::size_t>(x) + 1,
							                  "no colour is defined for " + Quote(std::string_view(&character, 1))};
						}
						image.Set(x, y, *colour);
					}
				}

				sheet.sprites.push_back({.name = std::string(name.text),
				                         .frameWidth = frameWidth,
				                         .frameHeight = frameHeight,
				                         .frames = frames,
				                         .image = std::move(image)});
				return std::nullopt;
			}
		};
	} // namespace

	const SheetSprite* PixelSheet::Find(std::string_view name) const noexcept
	{
		for (const SheetSprite& sprite : sprites)
		{
			if (sprite.name == name) return &sprite;
		}
		return nullptr;
	}

	PixelSheetResult ParsePixelSheet(std::string_view text)
	{
		Parser parser{.lines = core::SplitLines(text)};
		while (parser.next < parser.lines.size())
		{
			const std::string_view line = parser.lines[parser.next++];
			if (IsBlankOrComment(line)) continue;

			const std::vector<Word> words = core::SplitWords(line);
			std::optional<ParseError> error;
			if (words[0].text == "color")
			{
				error = parser.Colour(words);
			}
			else if (words[0].text == "sprite")
			{
				error = parser.Sprite(words);
			}
			else
			{
				return Fail(parser.LineNumber(), words[0].column,
				            "expected 'color' or 'sprite', not " + Quote(words[0].text));
			}
			if (error) return {.sheet = std::nullopt, .error = std::move(*error)};
		}
		return {.sheet = std::move(parser.sheet), .error = {}};
	}

	PixelSheetResult LoadPixelSheet(const std::filesystem::path& file)
	{
		const std::optional<std::string> text = core::ReadTextFile(file);
		if (!text) return Fail(0, 0, "can't read the file");
		return ParsePixelSheet(*text);
	}
} // namespace engine::graphics
