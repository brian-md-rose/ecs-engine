#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "ecs/entity.hpp"
#include "editor/editor.hpp"
#include "editor/entitypanels.hpp"

namespace engine::editor
{
	// What a game gives the standalone editor.
	struct EditorGameConfig
	{
		// Makes a fresh game, as it would start. Called for the edit world once, and again for each Play. Return
		// null if it couldn't be made. The editor empties the new game's world of entities first and fills it from
		// the scene, so the game's resources and systems are all that is used. The game should not write to disk
		// by itself (a save file), since several are made and thrown away.
		std::function<std::unique_ptr<application::ScheduledGame>()> makeGame;

		// Called after a scene is loaded into a world (the edit world on New and Open, the play world on Play),
		// for whatever a game does with its entities after loading, such as looking up atlas pictures by name.
		std::function<void(ecs::World&)> afterLoad;

		// Where scenes (.json files) are listed for Open, and written for Save.
		std::filesystem::path sceneFolder;
	};

	// The standalone editor as a Game: it holds an edit world, a copy of the game with the scene being edited and
	// no systems running, and draws it. Play saves the entities to a snapshot, makes a second copy of the game with
	// the snapshot loaded, and runs that; Stop throws it away, so the edit world is as it was. The Hierarchy and
	// Inspector show whichever world is current (changes made while playing are lost at Stop).
	//
	// Scene menu: New, Open (the .json files in sceneFolder), Save, Save as; a Play or Stop item; and a "*" beside the
	// scene's name when it has changes that are not saved. Give the Editor to Application::SetEditorHost, make it
	// visible, and run this as the Game.
	class EditorGame final : public application::Game
	{
		enum class Action
		{
			None,
			New,
			Open
		};
		enum class Dialog
		{
			None,
			ConfirmDiscard,
			SaveAs
		};

		Editor& editor;
		EditorGameConfig config;
		std::unique_ptr<application::ScheduledGame> edit;
		std::unique_ptr<application::ScheduledGame> play;
		std::shared_ptr<PanelState> panels;

		std::string sceneName; // the file name of the open scene, empty for a new one
		std::string savedText; // the scene as last saved or loaded, to see changes against
		bool dirty = false;
		int framesSinceCheck = 0;
		std::string message; // the last problem, shown in the menu bar
		Action pendingAction = Action::None;
		std::string pendingName;
		Dialog dialog = Dialog::None;
		bool openDialog = false; // the popup still has to be opened
		std::string saveAsName;

		[[nodiscard]] ecs::World& CurrentWorld() { return play ? play->world : edit->world; }
		[[nodiscard]] std::string SceneText();
		void ClearWorld(ecs::World& world);
		void Perform(Action action, const std::string& name);
		void Request(Action action, const std::string& name);
		void DrawMenuBar();
		void DrawDialogs();

	public:
		EditorGame(Editor& editorToUse, EditorGameConfig configToUse);

		// False if the game couldn't be made; don't run it.
		[[nodiscard]] bool IsValid() const noexcept { return edit != nullptr; }

		// ---- the scene (edit mode) ----
		void NewScene();
		// Loads folder/name into the edit world. False, with the reason in Message(), and the world untouched, if
		// it can't be read or has errors.
		bool OpenScene(const std::string& name);
		bool SaveScene();                          // to the open scene's file; false if there is none yet
		bool SaveSceneAs(const std::string& name); // ".json" is added if missing
		[[nodiscard]] const std::string& SceneName() const noexcept { return sceneName; }
		// True if the scene has changes since it was saved or loaded. Checked every few frames while editing.
		[[nodiscard]] bool IsDirty() const noexcept { return dirty; }
		void CheckDirty(); // looks now, instead of waiting for the next check

		// ---- play ----
		bool StartPlay();
		void StopPlay();
		[[nodiscard]] bool IsPlaying() const noexcept { return play != nullptr; }

		[[nodiscard]] ecs::World& EditWorld() { return edit->world; }
		[[nodiscard]] ecs::World* PlayWorld() { return play ? &play->world : nullptr; }
		[[nodiscard]] const std::string& Message() const noexcept { return message; }

		// Game:
		void Update(double dt, const input::Input& input, audio::AudioDevice* audio) override;
		void Render(graphics::Renderer& renderer, double alpha) override;
		[[nodiscard]] bool ShowSystemCursor() const override;
		[[nodiscard]] application::DisplayLink Display() override;
	};
} // namespace engine::editor
