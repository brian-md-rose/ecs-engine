#include "nameentry.hpp"

#include <algorithm>

namespace game
{
	namespace
	{
		[[nodiscard]] std::size_t IndexOf(char c) noexcept
		{
			const std::size_t at = kNameAlphabet.find(c);
			return at == std::string_view::npos ? kNameAlphabet.size() - 1 : at; // unknown: a space
		}

		[[nodiscard]] std::string Trim(std::string_view text)
		{
			const std::size_t first = text.find_first_not_of(' ');
			if (first == std::string_view::npos) return {};
			const std::size_t last = text.find_last_not_of(' ');
			return std::string(text.substr(first, last - first + 1));
		}
	} // namespace

	NameEntry StepNameEntry(NameEntry entry, const NameInput& input)
	{
		entry.slots.resize(kNameLength, ' ');
		entry.cursor = std::min(entry.cursor, kNameLength - 1);

		if (input.left && !input.right && entry.cursor > 0) --entry.cursor;
		if (input.right && !input.left && entry.cursor + 1 < kNameLength) ++entry.cursor;

		if (input.up != input.down)
		{
			const std::size_t count = kNameAlphabet.size();
			const std::size_t at = IndexOf(entry.slots[entry.cursor]);
			entry.slots[entry.cursor] = kNameAlphabet[input.up ? (at + 1) % count : (at + count - 1) % count];
		}
		return entry;
	}

	std::string NameOf(const NameEntry& entry)
	{
		std::string name = Trim(entry.slots);
		return name.empty() ? std::string(kDefaultName) : name;
	}

	std::string CleanName(std::string_view text)
	{
		std::string name;
		for (const char c : text.substr(0, kNameLength))
		{
			const char upper = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
			name += kNameAlphabet.find(upper) == std::string_view::npos ? ' ' : upper;
		}
		return Trim(name);
	}

	NameEntry EntryFor(std::string_view name)
	{
		NameEntry entry;
		entry.slots = CleanName(name);
		entry.slots.resize(kNameLength, ' ');
		entry.cursor = 0;
		return entry;
	}
} // namespace game
