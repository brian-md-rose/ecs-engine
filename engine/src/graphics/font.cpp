#include "graphics/font.hpp"

#include <algorithm>
#include <cstddef>

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

	void LayoutText(const BitmapFont& font, std::string_view text, Position origin, int scale,
	                std::vector<GlyphQuad>& quads)
	{
		const auto advance = static_cast<float>(font.advance * scale);
		const auto lineHeight = static_cast<float>(font.lineHeight * scale);
		Position pen = origin;
		for (const char character : text)
		{
			if (character == '\n')
			{
				pen = {origin.x, pen.y + lineHeight};
				continue;
			}
			if (const PixelRect* glyph = font.Glyph(character))
			{
				quads.push_back({.screen = pen, .source = *glyph});
			}
			pen.x += advance;
		}
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
} // namespace engine::graphics
