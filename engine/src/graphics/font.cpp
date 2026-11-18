#include "graphics/font.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace engine::graphics
{
	namespace
	{
		[[nodiscard]] std::optional<std::size_t> GlyphIndex(char character) noexcept
		{
			const auto code = static_cast<unsigned char>(character);
			if (code >= 128) return std::nullopt;
			return static_cast<std::size_t>(code);
		}

		// Calls emit(line) for each line of text wrapped to maxWidth, without allocating. See WrapText.
		template <typename Emit>
		void ForEachLine(const BitmapFont& font, std::string_view text, int maxWidth, int scale, Emit&& emit)
		{
			if (text.empty()) return;

			const int cell = std::max(font.advance * scale, 1);
			const bool wrapping = maxWidth > 0;
			const std::size_t maxChars = wrapping ? static_cast<std::size_t>(std::max(maxWidth / cell, 1))
			                                      : std::numeric_limits<std::size_t>::max();
			// Wrapped lines lose their trailing spaces, so alignment sees only the visible text; with no
			// width the text is left exactly as written.
			const auto emitLine = [&](std::string_view line)
			{
				if (wrapping)
				{
					while (!line.empty() && line.back() == ' ')
						line.remove_suffix(1);
				}
				emit(line);
			};

			std::size_t paragraphStart = 0;
			while (paragraphStart <= text.size())
			{
				std::size_t paragraphEnd = text.find('\n', paragraphStart);
				if (paragraphEnd == std::string_view::npos) paragraphEnd = text.size();
				const std::string_view paragraph = text.substr(paragraphStart, paragraphEnd - paragraphStart);
				paragraphStart = paragraphEnd + 1;

				// The line being built is paragraph[lineStart, lineEnd), words joined by single spaces
				// exactly as in the source, so every line is a piece of the text.
				std::size_t lineStart = 0;
				std::size_t lineEnd = 0;
				bool open = false;

				std::size_t pos = 0;
				while (pos <= paragraph.size())
				{
					std::size_t wordEnd = paragraph.find(' ', pos);
					if (wordEnd == std::string_view::npos) wordEnd = paragraph.size();
					const std::size_t length = wordEnd - pos;
					const std::size_t needed = open ? wordEnd - lineStart : length;
					if (needed <= maxChars)
					{
						if (!open)
						{
							lineStart = pos;
							open = true;
						}
						lineEnd = wordEnd;
					}
					else if (length > 0)
					{
						if (open) emitLine(paragraph.substr(lineStart, lineEnd - lineStart));
						std::size_t at = pos;
						while (wordEnd - at > maxChars)
						{
							emit(paragraph.substr(at, maxChars));
							at += maxChars;
						}
						lineStart = at;
						lineEnd = wordEnd;
						open = true;
					}
					pos = wordEnd + 1;
				}
				if (open) emitLine(paragraph.substr(lineStart, lineEnd - lineStart));
			}
		}
	} // namespace

	void BitmapFont::SetGlyph(char character, PixelRect source)
	{
		if (const std::optional<std::size_t> index = GlyphIndex(character)) glyphs[*index] = source;
	}

	const PixelRect* BitmapFont::Glyph(char character) const noexcept
	{
		const std::optional<std::size_t> index = GlyphIndex(character);
		if (!index || !glyphs[*index]) return nullptr;
		return &*glyphs[*index];
	}

	const PixelRect* BitmapFont::GlyphOrPlaceholder(char character) const noexcept
	{
		if (const PixelRect* glyph = Glyph(character)) return glyph;
		// Spaces and control characters are gaps; anything else unknown is made visible.
		if (static_cast<unsigned char>(character) <= ' ') return nullptr;
		if (placeholder) return &*placeholder;
		return Glyph('?');
	}

	void LayoutText(const BitmapFont& font, std::string_view text, Position origin, int scale,
	                std::vector<GlyphQuad>& quads)
	{
		LayoutText(font, text, origin, scale, 0, Align::Left, quads);
	}

	void LayoutText(const BitmapFont& font, std::string_view text, Position origin, int scale, int wrapWidth,
	                Align align, std::vector<GlyphQuad>& quads)
	{
		const auto advance = static_cast<float>(font.advance * scale);
		const auto lineHeight = static_cast<float>(font.lineHeight * scale);
		float top = origin.y;
		ForEachLine(font, text, wrapWidth, scale, [&](std::string_view line)
		{
			// Left keeps the origin as given, so a caller that snapped it keeps its position.
			const float left = align == Align::Left
			                       ? origin.x
			                       : AlignedX(origin.x, align, static_cast<int>(line.size()) * font.advance * scale);
			float x = left;
			for (const char character : line)
			{
				if (const PixelRect* glyph = font.GlyphOrPlaceholder(character))
				{
					quads.push_back({.screen = {x, top}, .source = *glyph});
				}
				x += advance;
			}
			top += lineHeight;
		});
	}

	std::vector<std::string_view> WrapText(const BitmapFont& font, std::string_view text, int maxWidth, int scale)
	{
		std::vector<std::string_view> lines;
		ForEachLine(font, text, maxWidth, scale, [&lines](std::string_view line)
		{
			lines.push_back(line);
		});
		return lines;
	}

	TextSize MeasureText(const BitmapFont& font, std::string_view text, int maxWidth, int scale)
	{
		std::size_t widest = 0;
		int count = 0;
		ForEachLine(font, text, maxWidth, scale, [&](std::string_view line)
		{
			widest = std::max(widest, line.size());
			++count;
		});
		return {.width = static_cast<int>(widest) * font.advance * scale, .height = count * font.lineHeight * scale};
	}

	int TextWidth(const BitmapFont& font, std::string_view text, int scale)
	{
		int widest = 0;
		int line = 0;
		for (const char character : text)
		{
			line = character == '\n' ? 0 : line + 1;
			widest = std::max(widest, line);
		}
		return widest * font.advance * scale;
	}

	int TextHeight(const BitmapFont& font, std::string_view text, int scale)
	{
		if (text.empty()) return 0;
		const auto lines = static_cast<int>(std::ranges::count(text, '\n')) + 1;
		return lines * font.lineHeight * scale;
	}

	float AlignedX(float anchorX, Align align, int width) noexcept
	{
		const auto pixels = static_cast<float>(width);
		switch (align)
		{
		case Align::Left:
			break;
		case Align::Centre:
			return std::floor(anchorX - pixels / 2.0f);
		case Align::Right:
			return std::floor(anchorX - pixels);
		}
		return std::floor(anchorX);
	}
} // namespace engine::graphics
