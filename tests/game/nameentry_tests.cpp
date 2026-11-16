#include <catch2/catch_test_macros.hpp>

#include <string>

#include "nameentry.hpp"

namespace
{
	using game::NameEntry;
	using game::NameInput;

	[[nodiscard]] NameEntry Step(NameEntry entry, NameInput input)
	{
		return game::StepNameEntry(std::move(entry), input);
	}
} // namespace

TEST_CASE("A new entry starts with an A in the first slot and the cursor on it", "[game][nameentry]")
{
	const NameEntry entry;
	CHECK(entry.slots == "A       ");
	CHECK(entry.slots.size() == game::kNameLength);
	CHECK(entry.cursor == 0);
}

TEST_CASE("Up and down cycle the slot through the alphabet", "[game][nameentry]")
{
	NameEntry entry;
	entry = Step(entry, {.up = true});
	CHECK(entry.slots[0] == 'B');
	entry = Step(entry, {.down = true});
	CHECK(entry.slots[0] == 'A');
	entry = Step(entry, {.down = true}); // down from A wraps to the last character, a space
	CHECK(entry.slots[0] == ' ');
	entry = Step(entry, {.down = true});
	CHECK(entry.slots[0] == '9');
	entry = Step(entry, {.up = true});
	entry = Step(entry, {.up = true}); // and up from the space wraps to A
	CHECK(entry.slots[0] == 'A');
}

TEST_CASE("The alphabet is A to Z, 0 to 9 and a space", "[game][nameentry]")
{
	NameEntry entry;
	std::string seen;
	for (std::size_t i = 0; i < game::kNameAlphabet.size(); ++i)
	{
		seen += entry.slots[0];
		entry = Step(entry, {.up = true});
	}
	CHECK(seen == game::kNameAlphabet);
	CHECK(entry.slots[0] == 'A'); // a full lap
}

TEST_CASE("Left and right move between slots and stop at the ends", "[game][nameentry]")
{
	NameEntry entry;
	entry = Step(entry, {.left = true});
	CHECK(entry.cursor == 0);
	entry = Step(entry, {.right = true});
	entry = Step(entry, {.right = true});
	CHECK(entry.cursor == 2);
	for (std::size_t i = 0; i < 2 * game::kNameLength; ++i)
	{
		entry = Step(entry, {.right = true});
	}
	CHECK(entry.cursor == game::kNameLength - 1);
	entry = Step(entry, {.left = true});
	CHECK(entry.cursor == game::kNameLength - 2);
}

TEST_CASE("Only the slot under the cursor changes", "[game][nameentry]")
{
	NameEntry entry;
	entry = Step(entry, {.right = true});
	entry = Step(entry, {.up = true}); // the second slot: a space becomes A
	CHECK(entry.slots == "AA      ");
	entry = Step(entry, {.up = true});
	CHECK(entry.slots == "AB      ");
}

TEST_CASE("Opposite keys at once cancel out", "[game][nameentry]")
{
	NameEntry entry;
	entry = Step(entry, {.up = true, .down = true, .left = true, .right = true});
	CHECK(entry.slots == "A       ");
	CHECK(entry.cursor == 0);
}

TEST_CASE("A damaged entry is put right before it is edited", "[game][nameentry]")
{
	NameEntry entry;
	entry.slots = "AB";
	entry.cursor = 99;
	entry = Step(entry, {});
	CHECK(entry.slots.size() == game::kNameLength);
	CHECK(entry.cursor == game::kNameLength - 1);

	entry.slots = "A#C     ";
	entry.cursor = 1;
	entry = Step(entry, {.up = true}); // '#' counts as a space, so up gives an A
	CHECK(entry.slots[1] == 'A');
}

TEST_CASE("NameOf trims the slots, and a blank name is PLAYER", "[game][nameentry]")
{
	NameEntry entry;
	entry.slots = "  ADA  X";
	CHECK(game::NameOf(entry) == "ADA  X");
	entry.slots = "        ";
	CHECK(game::NameOf(entry) == std::string(game::kDefaultName));
}

TEST_CASE("CleanName makes a saved name safe to show", "[game][nameentry]")
{
	CHECK(game::CleanName("Ada") == "ADA");
	CHECK(game::CleanName("  ada  ") == "ADA");
	CHECK(game::CleanName("a!b") == "A B");
	CHECK(game::CleanName("ABCDEFGHIJKL") == "ABCDEFGH"); // at most kNameLength characters
	CHECK(game::CleanName("").empty());
	CHECK(game::CleanName("!!!").empty());
}

TEST_CASE("EntryFor holds a name with the cursor on the first slot", "[game][nameentry]")
{
	const NameEntry entry = game::EntryFor("ada");
	CHECK(entry.slots == "ADA     ");
	CHECK(entry.cursor == 0);
	CHECK(game::NameOf(entry) == "ADA");
}
