#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "application/application.hpp"
#include "application/scheduledgame.hpp"
#include "ecs/entity.hpp"
#include "editor/editor.hpp"
#include "editor/entitypanels.hpp"
#include "editor/gridtool.hpp"
#include "editor/viewport.hpp"
#include "graphics/renderer.hpp"

struct ImDrawList;

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

		// A text file where the names of the scenes last opened or saved are kept, newest first, so the editor can
		// reopen the last one at the next start and list the rest under Open recent. Empty keeps none.
		std::filesystem::path recentFile = {};

		// A folder where unsaved changes are written every autosaveSeconds while there are any, one file per scene,
		// so a crash costs a minute at most. When a scene is opened and its autosave is newer than the file, the
		// editor offers to restore it. The autosave is deleted when the scene is saved, replaced or New is chosen.
		// Keep it outside the scene folder, which is often committed. Empty keeps no autosaves.
		std::filesystem::path autosaveFolder = {};
		double autosaveSeconds = 60.0;

		// A scene in sceneFolder to open at start instead of the last one (".json" is added if missing), such as one
		// named on the command line. If it can't be opened the message says so and the editor starts on a new scene.
		std::string openOnStart = {};

		// Called on the play world after its scene is loaded and afterLoad has run, before its first update, for a game
		// that needs to start itself on what the scene holds (Tangle Run starts on its maze when the scene is one).
		// Returns what is wrong, if the scene can't be played (shown in the menu bar, and Play does not start), or an
		// empty string. No hook leaves the game to start as it would.
		std::function<std::string(ecs::World&)> onPlay = nullptr;

		// Paints a scene made of grid cells (a maze). The Viewport shows its palette while editing a scene it applies
		// to.
		GridTool gridTool = {};
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
			SaveAs,
			Recover
		};

		Editor& editor;
		EditorGameConfig config;
		std::unique_ptr<application::ScheduledGame> edit;
		std::unique_ptr<application::ScheduledGame> play;
		std::shared_ptr<PanelState> panels;

		std::string sceneName; // the file name of the open scene, empty for a new one
		std::chrono::steady_clock::time_point lastAutosave = std::chrono::steady_clock::now();
		std::vector<std::string> recent; // scene file names last opened or saved, newest first
		std::string savedText;           // the scene as last saved or loaded, to see changes against
		bool dirty = false;
		int framesSinceCheck = 0;
		std::string message; // the last problem, shown in the menu bar

		// Everything the last load, save or Play start reported, for the Issues panel (the menu bar only has room for
		// the first). Errors first, then warnings.
		struct ShownIssue
		{
			bool error = false;
			std::string path;
			std::string message;
		};
		std::string issuesSource; // what the issues are about, such as "opening level1.json"
		std::vector<ShownIssue> shownIssues;
		Action pendingAction = Action::None;
		std::string pendingName;
		Dialog dialog = Dialog::None;
		bool openDialog = false; // the popup still has to be opened
		std::string saveAsName;

		// The viewport: the game is drawn into target (as big as the game's logical screen) and the Viewport panel
		// shows it, panned and zoomed. The renderer owns the texture, so it is never freed here.
		graphics::TextureId target = graphics::kNoTexture;
		int targetWidth = 0;
		int targetHeight = 0;
		SDL_Texture* targetHandle = nullptr;
		ViewTransform view;
		// The grid tool's brush: select (the usual click and drag), a palette entry (its index), or erase.
		static constexpr std::size_t kRecentScenes = 8; // how many scene names are kept
		static constexpr int kBrushSelect = -1;
		static constexpr int kBrushErase = -2;
		int brush = kBrushSelect;
		bool stroking = false;    // a paint or erase drag is in progress
		std::string gridProblems; // the grid tool's problem line, kept until the scene changes
		bool problemsStale = true;
		std::optional<GridCell> lastCell; // the cell the stroke last touched, so a fast drag leaves no gaps
		PlayMouse playMouse;              // the mouse the playing game sees, from the pointer over the viewport
		bool mouseFed = false;            // the viewport fed playMouse since the last game update
		// Undo: the scene as text after each finished change (a drag, a typed value, a menu click), oldest first;
		// historyPos is the one the world is at. Undo and Redo reload the neighbour, which re-creates the entities
		// (their ids change, the selection is kept by position).
		std::vector<std::string> history;
		std::size_t historyPos = 0;
		bool wasBusy = false; // the pointer or a widget was in use last frame

		void ResetHistory();
		void Commit(const std::string& text);
		bool GoTo(std::size_t position);
		bool movingSelected = false; // a drag that began on an entity is moving it
		struct DraggedEntity
		{
			ecs::Entity entity;
			graphics::Position start; // where it was when the drag began (MovablePosition)
		};
		std::vector<DraggedEntity> dragged;         // what a drag in the Viewport is moving
		graphics::Position dragPixels{};            // how far the pointer has moved since it began, in picture pixels
		std::optional<graphics::Position> boxStart; // where a box selection began, in picture pixels
		ecs::Entity collapseTo =
		    ecs::Entity::Null;            // a click on part of a multiple selection narrows to this on release
		std::vector<ecs::Entity> boxBase; // what was selected when the box began, kept if it adds
		bool snapToGrid = false;          // moves land on multiples of snapSize (Ctrl flips this while dragging)
		int snapSize = 8;
		bool viewportActive = false; // the Viewport had the pointer or focus last frame (arrow keys nudge only then)
		ecs::Entity shownSelection =
		    ecs::Entity::Null; // the selection the viewport last looked at, to scroll to a new one

		[[nodiscard]] graphics::Camera CurrentCamera();
		void DrawViewport();
		void DrawIssues();
		[[nodiscard]] std::filesystem::path AutosavePath() const;
		void DropAutosave();
		void OfferRecovery();
		void ShowIssues(const std::string& source, const ecs::Issues& issues);
		void PaintStroke(const GridInfo& grid, graphics::Position origin, ImDrawList* draw);

		[[nodiscard]] ecs::World& CurrentWorld() { return play ? play->world : edit->world; }
		[[nodiscard]] std::string SceneText();
		void ClearWorld(ecs::World& world);
		void Perform(Action action, const std::string& name);
		void Request(Action action, const std::string& name);
		void DrawMenuBar();
		void DrawDialogs();
		void SaveOrAsk();
		void HandleShortcuts();
		void Remember(std::string name);
		void CopySelection();
		void PasteClipboard();

	public:
		EditorGame(Editor& editorToUse, EditorGameConfig configToUse);
		// The Editor keeps callbacks into this object (menu bar, panels), so destroy it only once the Editor is no
		// longer drawing; the menu bar is unhooked here, the panels stay (they are the Editor's).
		~EditorGame() override;
		EditorGame(const EditorGame&) = delete;
		EditorGame& operator=(const EditorGame&) = delete;

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

		// ---- undo ----
		// Each change you finish (not each frame of a drag) is one step; at most 100 are kept, and New and Open start
		// again. Not while playing.
		bool Undo();
		bool Redo();
		[[nodiscard]] bool CanUndo() const noexcept { return !play && historyPos > 0; }
		[[nodiscard]] bool CanRedo() const noexcept { return !play && historyPos + 1 < history.size(); }

		// ---- play ----
		bool StartPlay();
		void StopPlay();
		[[nodiscard]] bool IsPlaying() const noexcept { return play != nullptr; }

		[[nodiscard]] ecs::World& EditWorld() { return edit->world; }
		[[nodiscard]] ecs::World* PlayWorld() { return play ? &play->world : nullptr; }
		[[nodiscard]] const std::string& Message() const noexcept { return message; }
		// ---- autosave ----
		// Writes the scene to the autosave folder if it has unsaved changes. True if a file was written.
		bool WriteAutosave();
		// True while the editor is asking whether to bring back an autosave that is newer than the scene's file.
		[[nodiscard]] bool RecoveryOffered() const noexcept { return dialog == Dialog::Recover; }
		bool AcceptRecovery();  // replaces the scene with the autosave, which stays unsaved; false if it can't load
		void DeclineRecovery(); // deletes the autosave

		// How many problems the Issues panel lists: those of the last open, save or Play start.
		[[nodiscard]] std::size_t IssueCount() const noexcept { return shownIssues.size(); }
		// True once the game has been drawn into the viewport's picture (false if the renderer couldn't make one).
		[[nodiscard]] bool HasViewportPicture() const noexcept { return targetHandle != nullptr; }
		[[nodiscard]] ViewTransform& View() noexcept { return view; }

		// Game:
		void Update(double dt, const input::Input& input, audio::AudioDevice* audio) override;
		void Render(graphics::Renderer& renderer, double alpha) override;
		[[nodiscard]] bool ShowSystemCursor() const override;
		[[nodiscard]] application::DisplayLink Display() override;
	};
} // namespace engine::editor
