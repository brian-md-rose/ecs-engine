#pragma once
#include <cstddef>
#include <string>
#include <string_view>

// The name-entry screen's editing, as pure functions: no registry, no SDL. The player edits a
// row of slots arcade-style: up and down change the letter in a slot, left and right move
// between slots.
namespace game
{
	inline constexpr std::size_t kNameLength = 8;

	// What a slot can hold, in the order up cycles through them: A to Z, 0 to 9, and a space.
	inline constexpr std::string_view kNameAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";

	// What a name is when the player leaves every slot blank.
	inline constexpr std::string_view kDefaultName = "PLAYER";

	// Resource: the name being entered. It keeps the last name entered, so the next high score
	// starts from it.
	struct NameEntry
	{
		std::string slots = std::string("A") + std::string(kNameLength - 1, ' '); // always kNameLength long
		std::size_t cursor = 0;                                                   // the slot being edited
	};

	// This update's keys.
	struct NameInput
	{
		bool up = false;
		bool down = false;
		bool left = false;
		bool right = false;
	};

	// One update of editing: up moves the cursor's slot to the next character of kNameAlphabet
	// and down to the previous, wrapping round at either end; left and right move the cursor,
	// stopping at the first and last slot. A character that isn't in the alphabet counts as a
	// space. Pure.
	[[nodiscard]] NameEntry StepNameEntry(NameEntry entry, const NameInput& input);

	// The name the slots spell, without the spaces at either end; kDefaultName if it is blank.
	[[nodiscard]] std::string NameOf(const NameEntry& entry);

	// A name read from a save file made safe: capitals, digits and spaces only (anything else
	// becomes a space; lower case becomes upper), at most kNameLength characters, trimmed.
	[[nodiscard]] std::string CleanName(std::string_view text);

	// An entry that holds name, with the cursor on its first slot.
	[[nodiscard]] NameEntry EntryFor(std::string_view name);
} // namespace game
