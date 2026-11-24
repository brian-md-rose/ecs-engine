#include "editor/editorgame.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <numbers>
#include <optional>
#include <system_error>
#include <utility>

#include "core/files.hpp"
#include "core/json.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/assetbrowser.hpp"
#include "editor/assetdrag.hpp"
#include "editor/consolepanel.hpp"
#include "editor/entityops.hpp"
#include "editor/prefabs.hpp"
#include "editor/profilerpanel.hpp"
#include "graphics/renderer2d.hpp"

namespace engine::editor
{
	constexpr double kPlayStep = 1.0 / 60.0; // what one Step advances the game by
	namespace
	{
		constexpr int kFramesBetweenDirtyChecks = 20;
		constexpr float kRotateSnap = 15.0F; // degrees; what Snap rounds a rotate drag to
		constexpr float kScaleSnap = 0.25F;  // what Snap rounds a scale drag to
		constexpr std::size_t kMaxHistory = 100;

		// The world's scene settings, made (empty) if it has none yet.
		ecs::SceneSettings& SettingsOf(ecs::World& world)
		{
			if (ecs::SceneSettings* settings = world.resources.TryGet<ecs::SceneSettings>()) return *settings;
			return world.resources.Emplace<ecs::SceneSettings>();
		}

		ecs::TypeRegistry& EnsureTypes(ecs::World& world)
		{
			if (ecs::TypeRegistry* types = world.resources.TryGet<ecs::TypeRegistry>()) return *types;
			auto& types = world.resources.Emplace<ecs::TypeRegistry>();
			ecs::RegisterEngineComponents(types);
			return types;
		}

		std::string WithJsonExtension(std::string name)
		{
			if (name.size() < 5 || name.substr(name.size() - 5) != ".json") name += ".json";
			return name;
		}

		// A bare file name: no folders, so a scene can only be written inside the scene folder.
		bool IsPlainName(const std::string& name)
		{
			return !name.empty() && name.find_first_of("/\\:") == std::string::npos && name != "." && name != ".." &&
			       name.find("..") == std::string::npos;
		}

		std::string FirstProblem(const ecs::Issues& issues)
		{
			if (issues.errors.empty()) return "unknown problem";
			const ecs::Issue& issue = issues.errors.front();
			return issue.path.empty() ? issue.message : issue.path + ": " + issue.message;
		}
	} // namespace

	EditorGame::~EditorGame()
	{
		editor.SetMenuBar({});
	}

	EditorGame::EditorGame(Editor& editorToUse, EditorGameConfig configToUse)
	    : editor(editorToUse), config(std::move(configToUse))
	{
		if (!config.makeGame) return;
		edit = config.makeGame();
		if (edit == nullptr) return;
		EnsureTypes(edit->world);
		ClearWorld(edit->world);
		savedText = SceneText();
		ResetHistory();

		panels = AddEntityPanels(editor, [this]() -> ecs::World&
		{
			return CurrentWorld();
		});
		panels->inspectors = config.inspectors;
		panels->componentCommands = config.componentCommands;
		panels->entityCommands = config.entityCommands;
		if (!config.assetFolder.empty())
		{
			thumbnails = std::make_shared<AssetThumbnails>();
			panels->assets = AddAssetBrowser(editor, config.assetFolder, thumbnails, config.sceneFolder);
		}
		AddConsolePanel(editor);
		profiler = AddProfilerPanel(editor, [this]() -> ecs::World&
		{
			return CurrentWorld();
		},
		                           [this]() -> const ecs::Scheduler*
		{
			return play ? &play->scheduler : nullptr; // the edit world runs no systems
		});
		if (config.spriteRect)
			panels->spriteRect = [this](const std::string& name)
			{
				return config.spriteRect(CurrentWorld(), name);
			};
		editor.SetMenuBar(
		    [this]
		{
			DrawMenuBar();
		}, [this]
		{
			DrawDialogs();
		});
		editor.AddPanel("Viewport", [this]
		{
			DrawViewport();
		});
		editor.AddPanel("Game", [this]
		{
			DrawGame();
		});
		editor.AddPanel("Issues", [this]
		{
			DrawIssues();
		});
		editor.AddPanel("Scene", [this]
		{
			DrawSceneSettings();
		});
		editor.SetPanelSize("Viewport", 700, 480);
		editor.SetPanelDock("Viewport", Editor::DockArea::Center);
		editor.SetPanelSize("Game", 480, 360);
		editor.SetPanelDock("Game", Editor::DockArea::Center);
		editor.SetPanelDock("Issues", Editor::DockArea::Bottom);
		editor.SetPanelDock("Scene", Editor::DockArea::Right);
		editor.SetPanelOpen("Viewport", true);
		editor.SetVisible(true);

		const bool asked = !config.openOnStart.empty();
		if (!config.recentFile.empty())
		{
			std::ifstream in(config.recentFile);
			std::string line;
			while (recent.size() < kRecentScenes && std::getline(in, line))
			{
				if (!line.empty() && line.back() == '\r') line.pop_back();
				if (IsPlainName(line) && std::find(recent.begin(), recent.end(), line) == recent.end())
					recent.push_back(line);
			}
		}
		if (asked)
		{
			(void)OpenScene(WithJsonExtension(config.openOnStart)); // a failure leaves a new scene and says why
		}
		else if (!recent.empty())
		{
			// The scene that was open last time, if it is still there; a failure leaves a new scene and no message.
			if (!OpenScene(recent.front()))
			{
				message.clear();
				shownIssues.clear();
				editor.SetPanelOpen("Issues", false); // ShowIssues opened it; a silent start should stay silent
			}
		}
		if (sceneName.empty()) OfferRecovery(); // changes to a new scene that was never saved
	}

	// Replaces what the Issues panel lists. Nothing to list clears it; anything to list opens it.
	void EditorGame::ShowIssues(const std::string& source, const ecs::Issues& issues)
	{
		shownIssues.clear();
		issuesSource = source;
		for (const ecs::Issue& issue : issues.errors)
			shownIssues.push_back({true, issue.path, issue.message});
		for (const ecs::Issue& issue : issues.warnings)
			shownIssues.push_back({false, issue.path, issue.message});
		if (!shownIssues.empty()) editor.SetPanelOpen("Issues", true);
	}

	void EditorGame::DrawSceneSettings()
	{
		ecs::SceneSettings& settings = SettingsOf(edit->world);
		if (!Editing()) ImGui::BeginDisabled();
		ImGui::TextDisabled("How the scene is drawn. Left off, the game decides.");
		bool hasCamera = settings.camera.has_value();
		if (ImGui::Checkbox("Camera", &hasCamera))
			settings.camera = hasCamera ? std::optional(ecs::SceneCamera{}) : std::nullopt;
		if (settings.camera)
		{
			ecs::SceneCamera& camera = *settings.camera;
			ImGui::DragFloat("x", &camera.x, 0.5F);
			ImGui::DragFloat("y", &camera.y, 0.5F);
			ImGui::SliderInt("zoom", &camera.zoom, 1, 8);
		}
		bool hasBackground = settings.background.has_value();
		if (ImGui::Checkbox("Background colour", &hasBackground))
			settings.background = hasBackground ? std::optional(std::array<std::uint8_t, 3>{}) : std::nullopt;
		if (settings.background)
		{
			std::array<std::uint8_t, 3>& color = *settings.background;
			float channels[3] = {static_cast<float>(color[0]) / 255.0F, static_cast<float>(color[1]) / 255.0F,
			                     static_cast<float>(color[2]) / 255.0F};
			if (ImGui::ColorEdit3("##background", channels))
			{
				for (std::size_t i = 0; i < 3; ++i)
					color[i] = static_cast<std::uint8_t>(std::clamp(channels[i], 0.0F, 1.0F) * 255.0F + 0.5F);
			}
		}
		if (!Editing()) ImGui::EndDisabled();
	}

	void EditorGame::DrawIssues()
	{
		if (shownIssues.empty())
		{
			ImGui::TextDisabled("No problems reported by the last open, save or Play.");
			return;
		}
		ImGui::TextUnformatted(issuesSource.c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Clear")) shownIssues.clear();
		ImGui::Separator();
		for (const ShownIssue& issue : shownIssues)
		{
			const ImVec4 color = issue.error ? ImVec4(1.0F, 0.4F, 0.4F, 1.0F) : ImVec4(1.0F, 0.8F, 0.3F, 1.0F);
			ImGui::TextColored(color, issue.error ? "error" : "warning");
			ImGui::SameLine();
			ImGui::TextWrapped("%s%s%s", issue.path.c_str(), issue.path.empty() ? "" : ": ", issue.message.c_str());
		}
	}

	std::filesystem::path EditorGame::AutosavePath() const
	{
		if (config.autosaveFolder.empty()) return {};
		return config.autosaveFolder / (sceneName.empty() ? std::string("_untitled_.json") : sceneName);
	}

	void EditorGame::DropAutosave()
	{
		const std::filesystem::path file = AutosavePath();
		if (file.empty()) return;
		std::error_code ignored;
		std::filesystem::remove(file, ignored);
	}

	bool EditorGame::WriteAutosave()
	{
		lastAutosave = std::chrono::steady_clock::now();
		const std::filesystem::path file = AutosavePath();
		if (file.empty() || !Editing()) return false;
		// Compared directly, not through CheckDirty, which would also add an undo step in the middle of an edit.
		const std::string text = SceneText();
		if (text == savedText)
		{
			DropAutosave(); // changes undone or saved: nothing left to bring back
			autosaveWritten = false;
			return false;
		}
		std::error_code ignored;
		std::filesystem::create_directories(file.parent_path(), ignored);
		// Atomically, so a crash in the middle leaves the last whole autosave and not half of one.
		autosaveWritten = core::WriteFileAtomic(file, text + '\n');
		return autosaveWritten;
	}

	// After a scene is opened (or the editor starts on a new one): an autosave newer than the file is a crash or a
	// close with unsaved changes, so ask.
	void EditorGame::OfferRecovery()
	{
		const std::filesystem::path file = AutosavePath();
		if (file.empty()) return;
		std::error_code error;
		const auto saved = std::filesystem::last_write_time(file, error);
		if (error) return;
		if (!sceneName.empty())
		{
			const auto scene = std::filesystem::last_write_time(config.sceneFolder / sceneName, error);
			if (!error && saved <= scene)
			{
				DropAutosave(); // older than the file: the changes were saved after it
				return;
			}
		}
		dialog = Dialog::Recover;
		openDialog = true;
	}

	bool EditorGame::AcceptRecovery()
	{
		dialog = Dialog::None;
		const std::filesystem::path file = AutosavePath();
		if (file.empty()) return false;
		ecs::World& world = edit->world;
		const std::vector<ecs::Entity> before = world.registry.AllEntities();
		ecs::SceneSettings loadedSettings;
		const ecs::SceneLoadResult loaded =
		    ecs::LoadSceneFile(world.registry, EnsureTypes(world), file, &loadedSettings);
		if (!loaded.Ok())
		{
			message = "can't restore the autosave: " + FirstProblem(loaded.issues);
			ShowIssues("restoring the autosave", loaded.issues);
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
		SettingsOf(world) = loadedSettings;
		if (config.afterLoad) config.afterLoad(world);
		ResetHistory();
		autosaveWritten = true; // the file is still there; it goes when the scene is clean again
		CheckDirty();           // against the saved text, so the restored scene shows as unsaved
		message.clear();
		panels->Select(ecs::Entity::Null);
		return true;
	}

	void EditorGame::DeclineRecovery()
	{
		dialog = Dialog::None;
		DropAutosave();
	}

	void EditorGame::Remember(std::string name) // by value: it may be an element of recent
	{
		recent.erase(std::remove(recent.begin(), recent.end(), name), recent.end());
		recent.insert(recent.begin(), name);
		if (recent.size() > kRecentScenes) recent.resize(kRecentScenes);
		if (config.recentFile.empty()) return;
		std::error_code ignored;
		std::filesystem::create_directories(config.recentFile.parent_path(), ignored);
		std::ofstream out(config.recentFile, std::ios::trunc);
		for (const std::string& item : recent)
			out << item << '\n';
	}

	std::string EditorGame::SceneText()
	{
		ecs::Issues issues;
		core::JsonValue scene = ecs::SaveScene(edit->world.registry, EnsureTypes(edit->world), issues);
		ecs::WriteSceneSettings(scene, SettingsOf(edit->world));
		return core::FormatJson(scene);
	}

	void EditorGame::ClearWorld(ecs::World& world)
	{
		for (const ecs::Entity entity : world.registry.AllEntities())
			world.registry.Destroy(entity);
	}

	void EditorGame::CheckDirty()
	{
		framesSinceCheck = 0;
		const std::string text = SceneText();
		dirty = text != savedText;
		if (!dirty && autosaveWritten)
		{
			DropAutosave(); // changes undone or saved: nothing left to bring back
			autosaveWritten = false;
		}
		if (Editing() && !history.empty() && text != history[historyPos]) problemsStale = true;
		if (Editing() && !history.empty() && text != history[historyPos]) Commit(text);
	}

	void EditorGame::ResetHistory()
	{
		problemsStale = true;
		history.assign(1, SceneText());
		historyPos = 0;
	}

	void EditorGame::Commit(const std::string& text)
	{
		history.resize(historyPos + 1); // a new change ends the redo line
		history.push_back(text);
		++historyPos;
		if (history.size() > kMaxHistory)
		{
			history.erase(history.begin());
			--historyPos;
		}
	}

	bool EditorGame::GoTo(std::size_t position)
	{
		if (!Editing() || position >= history.size()) return false;
		const core::JsonResult parsed = core::ParseJson(history[position]);
		if (!parsed.value) return false;
		ecs::World& world = edit->world;
		const std::vector<ecs::Entity> before = world.registry.AllEntities();
		// The selection is kept by position in the entity list, since undo gives every entity a new id.
		std::vector<std::size_t> selectedIndexes;
		for (const ecs::Entity chosen : panels->All())
		{
			const auto at = std::find(before.begin(), before.end(), chosen);
			if (at != before.end()) selectedIndexes.push_back(static_cast<std::size_t>(at - before.begin()));
		}

		problemsStale = true;
		const ecs::SceneLoadResult loaded = ecs::LoadScene(world.registry, EnsureTypes(world), *parsed.value);
		if (!loaded.Ok())
		{
			message = "can't go back: " + FirstProblem(loaded.issues);
			ShowIssues("going back in the history", loaded.issues);
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
		ecs::Issues ignoredSettingsIssues;
		SettingsOf(world) = ecs::ReadSceneSettings(*parsed.value, ignoredSettingsIssues);
		if (config.afterLoad) config.afterLoad(world);

		panels->Select(ecs::Entity::Null);
		const std::vector<ecs::Entity> after = world.registry.AllEntities();
		for (const std::size_t index : selectedIndexes)
		{
			if (index >= after.size()) continue;
			if (panels->selected.IsNull())
				panels->selected = after[index];
			else
				panels->extra.push_back(after[index]);
		}

		historyPos = position;
		history[position] = SceneText(); // as this world writes it, so the next check sees no change
		dirty = history[position] != savedText;
		framesSinceCheck = 0;
		message.clear();
		return true;
	}

	bool EditorGame::Undo()
	{
		if (!Editing()) return false;
		CheckDirty(); // a change not yet looked at becomes a step first, so undoing it is possible
		return historyPos > 0 && GoTo(historyPos - 1);
	}

	bool EditorGame::Redo()
	{
		if (!Editing()) return false;
		CheckDirty();
		return GoTo(historyPos + 1);
	}

	void EditorGame::NewScene()
	{
		if (play) StopPlay();
		DropAutosave(); // the changes were given up (or saved) by now
		ClearWorld(edit->world);
		SettingsOf(edit->world) = ecs::SceneSettings{};
		if (config.afterLoad) config.afterLoad(edit->world);
		sceneName.clear();
		savedText = SceneText();
		ResetHistory();
		dirty = false;
		message.clear();
		shownIssues.clear();
		panels->Select(ecs::Entity::Null);
	}

	bool EditorGame::OpenScene(const std::string& name)
	{
		if (play) StopPlay();
		if (!IsPlainName(name))
		{
			message = "not a scene file name: " + name;
			return false;
		}
		ecs::World& world = edit->world;
		const std::vector<ecs::Entity> before = world.registry.AllEntities();
		ecs::SceneSettings loadedSettings;
		const ecs::SceneLoadResult loaded =
		    ecs::LoadSceneFile(world.registry, EnsureTypes(world), config.sceneFolder / name, &loadedSettings);
		if (!loaded.Ok())
		{
			message = "can't open " + name + ": " + FirstProblem(loaded.issues);
			ShowIssues("opening " + name, loaded.issues);
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
		SettingsOf(world) = loadedSettings;
		if (config.afterLoad) config.afterLoad(world);
		if (dirty || !sceneName.empty()) DropAutosave(); // the old scene's changes were given up
		sceneName = name;
		Remember(name);
		savedText = SceneText();
		ResetHistory();
		dirty = false;
		message = loaded.issues.warnings.empty()
		              ? std::string()
		              : std::to_string(loaded.issues.warnings.size()) + " warning(s) opening " + name;
		ShowIssues("opening " + name, loaded.issues);
		panels->Select(ecs::Entity::Null);
		OfferRecovery();
		return true;
	}

	bool EditorGame::SaveScene()
	{
		if (sceneName.empty()) return false;
		return SaveSceneAs(sceneName);
	}

	bool EditorGame::SaveSceneAs(const std::string& name)
	{
		const std::string file = WithJsonExtension(name);
		if (!IsPlainName(file))
		{
			message = "not a scene file name: " + name;
			return false;
		}
		std::error_code ignored;
		std::filesystem::create_directories(config.sceneFolder, ignored);
		ecs::Issues issues;
		if (!ecs::SaveSceneFile(edit->world.registry, EnsureTypes(edit->world), config.sceneFolder / file, issues,
		                        &SettingsOf(edit->world)))
		{
			message = "can't save " + file + ": " + FirstProblem(issues);
			ShowIssues("saving " + file, issues);
			return false;
		}
		DropAutosave(); // of the name it had
		sceneName = file;
		DropAutosave(); // and of the name it has now
		Remember(file);
		savedText = SceneText();
		dirty = false;
		message.clear();
		ShowIssues("saving " + file, issues); // a save with warnings lists them; a clean one clears the list
		return true;
	}

	bool EditorGame::StartPlay()
	{
		if (play) return true;
		playFrom.reset();
		return BeginPlay();
	}

	bool EditorGame::StartPlayFrom(GridCell cell)
	{
		if (play || !config.onPlayFrom) return false;
		playFrom = cell;
		return BeginPlay();
	}

	bool EditorGame::BeginPlay()
	{
		playPaused = false;
		playSteps = 0;
		CheckDirty(); // so the prompt on New and Open while playing knows about edits made before Play
		ecs::Issues issues;
		core::JsonValue snapshot = ecs::SaveScene(edit->world.registry, EnsureTypes(edit->world), issues);
		if (!issues.Ok())
		{
			message = "can't play: " + FirstProblem(issues);
			ShowIssues("starting Play", issues);
			return false;
		}
		std::unique_ptr<application::ScheduledGame> game = config.makeGame();
		if (game == nullptr)
		{
			message = "can't play: the game could not be made";
			return false;
		}
		ClearWorld(game->world);
		SettingsOf(game->world) = SettingsOf(edit->world);
		const ecs::SceneLoadResult loaded = ecs::LoadScene(game->world.registry, EnsureTypes(game->world), snapshot);
		if (!loaded.Ok())
		{
			message = "can't play: " + FirstProblem(loaded.issues);
			ShowIssues("starting Play", loaded.issues);
			return false;
		}
		if (config.afterLoad) config.afterLoad(game->world);
		if (config.onPlay)
		{
			const std::string problem = config.onPlay(game->world);
			if (!problem.empty())
			{
				message = "can't play: " + problem;
				ecs::Issues reported;
				reported.errors.push_back({"", problem});
				ShowIssues("starting Play", reported);
				return false;
			}
		}
		if (playFrom && config.onPlayFrom)
		{
			const std::string problem = config.onPlayFrom(game->world, *playFrom);
			if (!problem.empty())
			{
				message = "can't play from there: " + problem;
				playFrom.reset();
				return false;
			}
		}
		play = std::move(game);
		playMouse.Reset();
		message.clear();
		ShowIssues("starting Play", loaded.issues);
		panels->Select(ecs::Entity::Null);
		return true;
	}

	void EditorGame::StopPlay()
	{
		if (!play) return;
		playMouse.Reset();
		play.reset();
		panels->Select(ecs::Entity::Null);
	}

	void EditorGame::Update(double dt, const input::Input& input, audio::AudioDevice* audio)
	{
		if (!play) return; // an edited scene is still
		// The editor window takes the real mouse events, so the game gets the pointer over the viewport instead.
		// If the viewport was not drawn (closed, behind another tab), nothing is held or hovered for the game.
		if (!mouseFed) playMouse.Feed({}, view, {0, 0}, targetWidth, targetHeight);
		mouseFed = false;
		input::Input gameInput = input;
		gameInput.mouse = playMouse.mouse;
		// Paused: nothing runs, except one fixed step per Step.
		double stepDt = dt * static_cast<double>(playSpeed);
		if (playPaused)
		{
			if (playSteps <= 0) return;
			--playSteps;
			stepDt = kPlayStep;
		}
		const auto updateStart = std::chrono::steady_clock::now();
		play->Update(stepDt, gameInput, playMuted ? nullptr : audio);
		if (profiler)
			profiler->AddUpdateMs(
			    std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - updateStart).count());
		playMouse.mouse.EndUpdate();
		if (play->WantsToQuit())
		{
			StopPlay(); // the game's Quit ends the run, not the editor
			message = "the game quit, so Play stopped";
		}
	}

	bool EditorGame::RestartPlay()
	{
		if (!play) return false;
		const std::optional<GridCell> from = playFrom;
		StopPlay();
		playFrom = from;
		return BeginPlay();
	}

	namespace
	{
		graphics::Camera CameraOf(ecs::World& world)
		{
			if (const auto* renderer2D = world.resources.TryGet<graphics::Renderer2D>()) return renderer2D->camera;
			return {};
		}

		// Makes the picture a view shows when the game's logical screen changes size.
		void FitTarget(graphics::Renderer& renderer, const graphics::Camera& camera, graphics::TextureId& target, int& width,
		               int& height, SDL_Texture*& handle)
		{
			if (camera.logicalWidth <= 0 || camera.logicalHeight <= 0 ||
			    (camera.logicalWidth == width && camera.logicalHeight == height))
				return;
			renderer.DestroyTexture(target);
			target = renderer.CreateRenderTarget(camera.logicalWidth, camera.logicalHeight);
			width = camera.logicalWidth;
			height = camera.logicalHeight;
			handle = renderer.GetTextureHandle(target);
		}
	} // namespace

	graphics::Camera EditorGame::CurrentCamera()
	{
		return CameraOf(CurrentWorld());
	}

	void EditorGame::Render(graphics::Renderer& renderer, double alpha)
	{
		if (thumbnails) thumbnails->SetRenderer(&renderer);
		// The Game panel is shown (it says so each time it is drawn): the game plays there and the scene stays here.
		const bool wasSplit = split;
		split = play != nullptr && gameDrawn;
		gameDrawn = false;
		if (split != wasSplit) panels->Select(ecs::Entity::Null); // the selection belongs to the other world
		application::ScheduledGame& current = play && !split ? *play : *edit;
		if (Editing())
		{
			// Look when nothing is being dragged or typed, so a drag is one step, not one per frame.
			const bool busy =
			    ImGui::GetCurrentContext() != nullptr &&
			    (ImGui::IsAnyItemActive() || ImGui::IsMouseDown(ImGuiMouseButton_Left) ||
			     ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle));
			if (busy)
				wasBusy = true;
			else if (wasBusy || ++framesSinceCheck >= kFramesBetweenDirtyChecks)
			{
				wasBusy = false;
				CheckDirty();
			}
			if (!busy && config.autosaveSeconds > 0 && dirty && dialog != Dialog::Recover &&
			    std::chrono::duration<double>(std::chrono::steady_clock::now() - lastAutosave).count() >=
			        config.autosaveSeconds)
				(void)WriteAutosave();
		}

		// Into a picture the viewport shows (made again only when the game's screen size changes, so a failure is
		// logged once). If none can be made, the game is drawn straight to the window.
		FitTarget(renderer, CurrentCamera(), target, targetWidth, targetHeight, targetHandle);
		if (split)
			FitTarget(renderer, CameraOf(play->world), gameTarget, gameTargetWidth, gameTargetHeight, gameTargetHandle);
		if (target != graphics::kNoTexture && renderer.SetRenderTarget(target))
		{
			current.Render(renderer, alpha);
			(void)renderer.SetRenderTarget(graphics::kNoTexture);
			if (split && gameTarget != graphics::kNoTexture && renderer.SetRenderTarget(gameTarget))
			{
				play->Render(renderer, alpha);
				(void)renderer.SetRenderTarget(graphics::kNoTexture);
			}
			renderer.Clear(30, 30, 34); // behind the panels
			return;
		}
		current.Render(renderer, alpha);
	}

	// The grid over the picture, the cell under the pointer, and the paint or erase drag: the cells the pointer crossed
	// while the left button is held get the brush, then the tool refreshes what depends on them.
	void EditorGame::PaintStroke(const GridInfo& grid, graphics::Position origin, ImDrawList* draw)
	{
		const ImGuiIO& io = ImGui::GetIO();
		const auto toWindow = [&](float x, float y)
		{
			const graphics::Position at = view.ToWindow({x, y}, origin);
			return ImVec2(at.x, at.y);
		};
		const float width = static_cast<float>(grid.columns) * grid.cellWidth;
		const float height = static_cast<float>(grid.rows) * grid.cellHeight;
		const ImU32 lineColor = IM_COL32(255, 255, 255, 40);
		for (int column = 0; column <= grid.columns; ++column)
		{
			const float x = grid.x + static_cast<float>(column) * grid.cellWidth;
			draw->AddLine(toWindow(x, grid.y), toWindow(x, grid.y + height), lineColor);
		}
		for (int row = 0; row <= grid.rows; ++row)
		{
			const float y = grid.y + static_cast<float>(row) * grid.cellHeight;
			draw->AddLine(toWindow(grid.x, y), toWindow(grid.x + width, y), lineColor);
		}

		const std::optional<GridCell> under = CellAt(grid, view.ToPicture({io.MousePos.x, io.MousePos.y}, origin));
		if (under && ImGui::IsItemHovered())
		{
			const float x = grid.x + static_cast<float>(under->column) * grid.cellWidth;
			const float y = grid.y + static_cast<float>(under->row) * grid.cellHeight;
			draw->AddRect(toWindow(x, y), toWindow(x + grid.cellWidth, y + grid.cellHeight),
			              IM_COL32(255, 200, 40, 255));
		}

		const GridTool& tool = config.gridTool;
		const auto kindOf = [&](GridCell cell)
		{
			if (!tool.kindAt) return -1;
			const std::optional<std::size_t> kind = tool.kindAt(edit->world, cell);
			return kind ? static_cast<int>(*kind) : -1;
		};
		const auto putDown = [&](GridCell cell, int kind)
		{
			if (kind >= 0 && static_cast<std::size_t>(kind) < tool.palette.size() && tool.paint)
				tool.paint(edit->world, static_cast<std::size_t>(kind), cell);
			else if (kind < 0 && tool.erase)
				tool.erase(edit->world, cell);
		};
		// Puts the brush (a palette entry, or the eraser) into every cell.
		const auto applyBrush = [&](const std::vector<GridCell>& cells)
		{
			for (const GridCell& cell : cells)
				putDown(cell, brush >= 0 ? brush : -1);
			if (tool.refresh) tool.refresh(edit->world);
			problemsStale = true;
		};
		const auto highlight = [&](GridCell cell, ImU32 color)
		{
			const float x = grid.x + static_cast<float>(cell.column) * grid.cellWidth;
			const float y = grid.y + static_cast<float>(cell.row) * grid.cellHeight;
			draw->AddRectFilled(toWindow(x, y), toWindow(x + grid.cellWidth, y + grid.cellHeight), color);
		};
		const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
		const bool released = !ImGui::IsMouseDown(ImGuiMouseButton_Left);
		const bool corners = brush == kBrushCopy ||
		                     (brush != kBrushStamp && gridShape != GridShape::Freehand && gridShape != GridShape::Fill);

		if (brush == kBrushStamp)
		{
			// The copied cells follow the pointer; a click puts them down.
			if (under && !stamp.Empty() && ImGui::IsItemHovered())
			{
				const std::vector<StampedCell> cells = StampCells(stamp, grid, *under);
				for (const StampedCell& placed : cells)
					highlight(placed.cell, placed.kind >= 0 ? IM_COL32(255, 200, 40, 90) : IM_COL32(255, 80, 80, 60));
				if (clicked)
				{
					for (const StampedCell& placed : cells)
						putDown(placed.cell, placed.kind);
					if (tool.refresh) tool.refresh(edit->world);
					problemsStale = true;
				}
			}
		}
		else if (corners)
		{
			// Press at one corner, drag to the other; the cells are put down when the button comes up over the grid.
			if (clicked && under) shapeStart = under;
			if (shapeStart)
			{
				const GridShape shape = brush == kBrushCopy ? GridShape::FilledRectangle : gridShape;
				std::vector<GridCell> cells;
				if (under) cells = ShapeCells(shape, *shapeStart, *under);
				for (const GridCell& cell : cells)
					highlight(cell, brush == kBrushCopy ? IM_COL32(80, 160, 255, 90) : IM_COL32(255, 200, 40, 90));
				if (released)
				{
					if (under && brush == kBrushCopy)
						stamp = CaptureStamp(*shapeStart, *under, kindOf);
					else if (under)
						applyBrush(cells);
					if (under && brush == kBrushCopy && !stamp.Empty()) brush = kBrushStamp;
					shapeStart.reset();
				}
			}
		}
		else if (gridShape == GridShape::Fill)
		{
			if (clicked && under) applyBrush(FloodCells(grid, *under, kindOf));
		}
		else if (ImGui::IsItemActive())
		{
			stroking = true;
			if (under)
			{
				const std::vector<GridCell> cells =
				    lastCell ? CellLine(*lastCell, *under) : std::vector<GridCell>{*under};
				for (const GridCell& cell : cells)
					putDown(cell, brush >= 0 ? brush : -1);
				if (!lastCell || *lastCell != *under)
				{
					if (tool.refresh) tool.refresh(edit->world);
					problemsStale = true;
				}
			}
			lastCell = under;
		}
		else if (stroking)
		{
			stroking = false;
			lastCell.reset();
		}
		if (!corners) shapeStart.reset();
		// A stroke ends when the button is up, whatever else changed meanwhile (the brush or shape picked mid-drag).
		if (released)
		{
			stroking = false;
			lastCell.reset();
		}
	}

	void EditorGame::TakeAssetDrop(graphics::Position origin)
	{
		const auto dropped = AssetDropTarget({DragKind::Sprite, DragKind::Scene, DragKind::Prefab});
		if (!dropped) return;
		if (dropped->kind == DragKind::Scene)
		{
			Request(Action::Open, dropped->name); // asks first if there are unsaved changes
			return;
		}
		if (dropped->kind == DragKind::Prefab)
		{
			const ImGuiIO& io = ImGui::GetIO();
			graphics::Position at = view.ToPicture({io.MousePos.x, io.MousePos.y}, origin);
			if (snapToGrid != io.KeyCtrl)
			{
				const auto step = static_cast<float>(snapSize);
				at = {std::round(at.x / step) * step, std::round(at.y / step) * step};
			}
			PutPrefab(dropped->name, at);
			return;
		}
		const auto rect = config.spriteRect ? config.spriteRect(CurrentWorld(), dropped->name) : std::nullopt;
		if (!rect)
		{
			message = "the game can't draw " + dropped->name;
			return;
		}
		const ImGuiIO& io = ImGui::GetIO();
		graphics::Position at = view.ToPicture({io.MousePos.x, io.MousePos.y}, origin);
		if (snapToGrid != io.KeyCtrl)
		{
			const auto step = static_cast<float>(snapSize);
			at = {std::round(at.x / step) * step, std::round(at.y / step) * step};
		}
		panels->Select(CreateSpriteEntity(CurrentWorld().registry, dropped->name, *rect, at));
		message.clear();
	}

	// The Game panel: the playing game in a picture of its own, with the mouse and keys, while the Viewport stays the
	// scene being edited.
	void EditorGame::DrawGame()
	{
		gameDrawn = true;
		if (!play || !split || gameTargetHandle == nullptr || gameTarget == graphics::kNoTexture)
		{
			ImGui::TextDisabled(play ? "Starting the game view..."
			                         : "Press Play (F5) to see the game here. The Viewport keeps showing the scene, so\nyou can "
			                           "keep editing it while the game runs (changes are not carried into the run).");
			return;
		}
		if (ImGui::Button("Reset view")) gameView = ViewTransform{};
		ImGui::SameLine();
		ImGui::Text("%.0f%%   the game has the mouse and keys; hold Ctrl to zoom and pan",
		            static_cast<double>(gameView.zoom) * 100.0);
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.x = std::max(size.x, 16.0F);
		size.y = std::max(size.y, 16.0F);
		ImGui::InvisibleButton("##game", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool hovered = ImGui::IsItemHovered();
		gameActive = hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const graphics::Position origin{min.x, min.y};
		const ImGuiIO& io = ImGui::GetIO();
		if (hovered && io.KeyCtrl && io.MouseWheel != 0.0F)
			gameView.ZoomAt(std::pow(1.15F, io.MouseWheel), {io.MousePos.x, io.MousePos.y}, origin);
		if (hovered && io.KeyCtrl &&
		    (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F) || ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0F)))
		{
			gameView.panX += io.MouseDelta.x;
			gameView.panY += io.MouseDelta.y;
		}
		PointerSample sample;
		sample.window = {io.MousePos.x, io.MousePos.y};
		sample.hovered = hovered && !io.KeyCtrl;
		for (std::size_t i = 0; i < sample.down.size(); ++i)
			sample.down[i] = ImGui::IsMouseDown(static_cast<int>(i));
		sample.wheel = io.MouseWheel;
		playMouse.Feed(sample, gameView, origin, gameTargetWidth, gameTargetHeight);
		mouseFed = true;

		ImDrawList* const draw = ImGui::GetWindowDrawList();
		draw->PushClipRect(min, max, true);
		draw->AddRectFilled(min, max, IM_COL32(20, 20, 24, 255));
		const graphics::Position topLeft = gameView.ToWindow({0, 0}, origin);
		const graphics::Position bottomRight = gameView.ToWindow(
		    {static_cast<float>(gameTargetWidth), static_cast<float>(gameTargetHeight)}, origin);
		draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(gameTargetHandle)), ImVec2(topLeft.x, topLeft.y),
		               ImVec2(bottomRight.x, bottomRight.y));
		draw->AddRect(ImVec2(topLeft.x, topLeft.y), ImVec2(bottomRight.x, bottomRight.y), IM_COL32(90, 90, 100, 255));
		draw->PopClipRect();
	}

	void EditorGame::DrawViewport()
	{
		// Playing over this view: the game has its mouse. With the Game panel shown the game is there instead, and this
		// view stays the scene.
		const bool playing = play != nullptr && !split;
		if (targetHandle == nullptr || target == graphics::kNoTexture)
		{
			ImGui::TextDisabled("The game has not drawn a picture yet.");
			return;
		}
		if (ImGui::Button("Reset view")) view = ViewTransform{};
		ImGui::SameLine();
		ImGui::Text(playing ? "%.0f%%   the game has the mouse; hold Ctrl to zoom and pan"
		                 : "%.0f%%   wheel zooms, middle or right drag pans, click selects",
		            static_cast<double>(view.zoom) * 100.0);

		if (Editing())
		{
			ImGui::SameLine();
			const std::array<std::pair<const char*, GizmoTool>, 3> tools{
			    {{"Move", GizmoTool::Translate}, {"Rotate", GizmoTool::Rotate}, {"Scale", GizmoTool::Scale}}};
			for (const auto& [label, which] : tools)
			{
				if (ImGui::RadioButton(label, gizmoTool == which)) gizmoTool = which;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("The gizmo on the selection: W moves, E rotates, R scales (Viewport focused).");
				ImGui::SameLine();
			}
			ImGui::Checkbox("Snap", &snapToGrid);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(
				    "Dragging and nudging land on multiples of the grid size; rotating on 15 degrees and\n"
				    "scaling on 0.25.\nHold Ctrl while dragging to flip it; Shift keeps one axis.");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(56.0F);
			ImGui::InputInt("grid", &snapSize, 0);
			snapSize = std::clamp(snapSize, 1, 256);
			ImGui::SameLine();
			ImGui::Checkbox("Gizmos", &showGizmos);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show the gizmo on the selection.");
			ImGui::SameLine();
			ImGui::Checkbox("Grid lines", &showSnapGrid);
			if (ImGui::IsItemHovered()) ImGui::SetTooltip("Draw a line at every grid size over the scene.");
		}
		// The grid tool's palette, when the scene being edited has a grid (a maze).
		std::optional<GridInfo> grid;
		if (Editing() && config.gridTool.grid) grid = config.gridTool.grid(edit->world);
		if (grid)
		{
			const GridTool& tool = config.gridTool;
			if (ImGui::RadioButton("Select", brush == kBrushSelect)) brush = kBrushSelect;
			for (std::size_t i = 0; i < tool.palette.size(); ++i)
			{
				ImGui::SameLine();
				if (ImGui::RadioButton(tool.palette[i].c_str(), brush == static_cast<int>(i)))
					brush = static_cast<int>(i);
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Erase", brush == kBrushErase)) brush = kBrushErase;
			if (tool.kindAt)
			{
				ImGui::SameLine();
				if (ImGui::RadioButton("Copy", brush == kBrushCopy)) brush = kBrushCopy;
				if (ImGui::IsItemHovered()) ImGui::SetTooltip("Drag over cells to copy them as a stamp.");
				ImGui::SameLine();
				if (ImGui::RadioButton("Stamp", brush == kBrushStamp)) brush = kBrushStamp;
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("Click to put the copied cells down (top-left at the pointer).");
			}
			if (brush >= 0 || brush == kBrushErase)
			{
				const std::array<std::pair<const char*, GridShape>, 5> shapes{{{"Freehand", GridShape::Freehand},
				                                                               {"Line", GridShape::Line},
				                                                               {"Rectangle", GridShape::Rectangle},
				                                                               {"Filled", GridShape::FilledRectangle},
				                                                               {"Fill", GridShape::Fill}}};
				for (const auto& [label, which] : shapes)
				{
					if (which == GridShape::Fill && !tool.kindAt) continue;
					if (ImGui::RadioButton(label, gridShape == which)) gridShape = which;
					ImGui::SameLine();
				}
				ImGui::NewLine();
			}
			if (tool.problems)
			{
				if (problemsStale)
				{
					gridProblems = tool.problems(edit->world);
					problemsStale = false;
				}
				const std::string& problems = gridProblems;
				if (!problems.empty()) ImGui::TextColored(ImVec4(1.0F, 0.6F, 0.3F, 1.0F), "%s", problems.c_str());
			}
		}
		else
		{
			brush = kBrushSelect;
		}
		const bool painting = grid && brush != kBrushSelect;

		ImVec2 size = ImGui::GetContentRegionAvail();
		size.x = std::max(size.x, 16.0F);
		size.y = std::max(size.y, 16.0F);
		ImGui::InvisibleButton("##viewport", size, ImGuiButtonFlags_MouseButtonLeft);
		const bool hovered = ImGui::IsItemHovered();
		viewportActive = hovered || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const graphics::Position origin{min.x, min.y};
		const ImGuiIO& io = ImGui::GetIO();
		if (Editing()) TakeAssetDrop(origin);

		// While playing, the game gets the wheel and the buttons; hold Ctrl to zoom and pan the view instead.
		const bool viewKeys = !playing || io.KeyCtrl;
		if (hovered && viewKeys && io.MouseWheel != 0.0F)
			view.ZoomAt(std::pow(1.15F, io.MouseWheel), {io.MousePos.x, io.MousePos.y}, origin);
		if (hovered && viewKeys &&
		    (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F) ||
		     ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0F)))
		{
			view.panX += io.MouseDelta.x;
			view.panY += io.MouseDelta.y;
		}

		if (playing)
		{
			PointerSample sample;
			sample.window = {io.MousePos.x, io.MousePos.y};
			sample.hovered = hovered && !io.KeyCtrl;
			for (std::size_t i = 0; i < sample.down.size(); ++i)
				sample.down[i] = ImGui::IsMouseDown(static_cast<int>(i));
			sample.wheel = io.MouseWheel;
			playMouse.Feed(sample, view, origin, targetWidth, targetHeight);
			mouseFed = true;
		}

		ImDrawList* const draw = ImGui::GetWindowDrawList();
		draw->PushClipRect(min, max, true);
		draw->AddRectFilled(min, max, IM_COL32(20, 20, 24, 255));
		const graphics::Position topLeft = view.ToWindow({0, 0}, origin);
		const graphics::Position bottomRight =
		    view.ToWindow({static_cast<float>(targetWidth), static_cast<float>(targetHeight)}, origin);
		draw->AddImage(ImTextureRef(reinterpret_cast<ImTextureID>(targetHandle)), ImVec2(topLeft.x, topLeft.y),
		               ImVec2(bottomRight.x, bottomRight.y));
		draw->AddRect(ImVec2(topLeft.x, topLeft.y), ImVec2(bottomRight.x, bottomRight.y), IM_COL32(90, 90, 100, 255));

		ecs::World& world = CurrentWorld();
		const graphics::Camera camera = CurrentCamera();
		if (showSnapGrid && snapSize > 0 && !playing)
		{
			// A line at every grid size in world units, as far as the picture shows (not when they would be a haze).
			const auto cell = static_cast<float>(snapSize);
			const auto zoom = static_cast<float>(camera.EffectiveZoom());
			if (cell * zoom * view.zoom >= 4.0F)
			{
				const graphics::Position shown = camera.ViewSize();
				const ImU32 lines = IM_COL32(255, 255, 255, 28);
				for (float x = std::floor(camera.position.x / cell) * cell; x <= camera.position.x + shown.x; x += cell)
				{
					const graphics::Position top = view.ToWindow(camera.WorldToScreen({x, camera.position.y}), origin);
					draw->AddLine(ImVec2(top.x, topLeft.y), ImVec2(top.x, bottomRight.y), lines);
				}
				for (float y = std::floor(camera.position.y / cell) * cell; y <= camera.position.y + shown.y; y += cell)
				{
					const graphics::Position left = view.ToWindow(camera.WorldToScreen({camera.position.x, y}), origin);
					draw->AddLine(ImVec2(topLeft.x, left.y), ImVec2(bottomRight.x, left.y), lines);
				}
			}
		}
		// The translate gizmo sits at the middle of the selection (window pixels). A press on one of its handles moves
		// the selection along that axis, or freely from the square; it goes first, so it wins over picking.
		const auto gizmoPivot = [&]() -> std::optional<graphics::Position>
		{
			if (!showGizmos || !Editing() || painting) return std::nullopt;
			const auto centre = SelectionCentre(world.registry, SelectionRoots(world.registry, *panels), camera);
			if (!centre) return std::nullopt;
			return view.ToWindow(*centre, origin);
		};
		// The rotate and scale gizmos follow the turn of the entity the Inspector shows, so a turned entity's axes lie
		// along its own.
		float gizmoRotation = 0;
		if (const auto* shown = world.registry.TryGetComponent<graphics::Transform>(panels->selected))
			gizmoRotation = shown->rotation;
		// Everything a drag from here puts back: where each selected entity is, and how it is turned and stretched.
		const auto grabSelection = [&]
		{
			dragged.clear();
			for (const ecs::Entity entity : SelectionRoots(world.registry, *panels))
			{
				if (const auto at = MovablePosition(world.registry, entity))
					dragged.push_back({entity, *at, MovableRotation(world.registry, entity).value_or(0.0F),
					                   MovableScale(world.registry, entity).value_or(graphics::Scale{})});
			}
			dragPixels = {};
			dragWindow = {};
			dragDegrees = 0;
		};
		gizmoHover = GizmoHandle::None;
		if (const auto pivot = gizmoPivot(); pivot && hovered)
		{
			const graphics::Position mouse{io.MousePos.x, io.MousePos.y};
			if (gizmoTool == GizmoTool::Rotate)
				gizmoHover = PickRotateGizmo(*pivot, mouse);
			else
				gizmoHover = PickGizmo(*pivot, mouse, {}, gizmoTool == GizmoTool::Scale ? gizmoRotation : 0.0F);
		}
		if (Editing() && !painting && gizmoHover != GizmoHandle::None && ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			boxStart.reset();
			gizmoHandle = gizmoHover;
			movingSelected = true;
			collapseTo = ecs::Entity::Null;
			grabSelection();
		}
		// Press selects what is under the pointer; dragging from there moves it (and the rest of the selection it is
		// part of), so no separate handle is needed. Ctrl or Shift adds to the selection or takes the entity out of it.
		// A press on empty space starts a box: what it covers when released is selected (added to the selection with
		// Ctrl or Shift).
		if (Editing() && !painting && gizmoHover == GizmoHandle::None && ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			const graphics::Position picture = view.ToPicture({io.MousePos.x, io.MousePos.y}, origin);
			const ecs::Entity picked = PickEntity(world.registry, camera, picture);
			const bool add = io.KeyCtrl || io.KeyShift;
			boxStart.reset();
			movingSelected = false;
			if (add && !picked.IsNull())
				panels->Toggle(picked);
			else if (!picked.IsNull() && !panels->IsSelected(picked))
				panels->Select(picked);
			else if (picked.IsNull())
			{
				boxStart = picture;
				boxBase = add ? panels->All() : std::vector<ecs::Entity>{};
				if (!add) panels->Select(ecs::Entity::Null);
			}
			movingSelected = !picked.IsNull() && panels->IsSelected(picked);
			dragged.clear();
			if (movingSelected) grabSelection();
			dragPixels = {};
			// A plain click on part of a multiple selection keeps it, so the group can be dragged; if the button comes
			// up without a drag, the click narrows the selection to that entity.
			collapseTo = !add && movingSelected && !panels->extra.empty() ? picked : ecs::Entity::Null;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			if (!collapseTo.IsNull() && dragPixels.x == 0.0F && dragPixels.y == 0.0F && panels->IsSelected(collapseTo))
				panels->Select(collapseTo);
			collapseTo = ecs::Entity::Null;
			movingSelected = false;
			gizmoHandle = GizmoHandle::None;
		}
		// A selection made elsewhere (the Hierarchy) that is off-screen is scrolled into view, so it can be seen. While
		// playing the selection is only noted, so Stop does not scroll to a change made during Play.
		if (panels->selected != shownSelection)
		{
			shownSelection = panels->selected;
			if (Editing() && shownSelection != ecs::Entity::Null && world.registry.IsAlive(shownSelection))
			{
				if (const auto rect = EntityScreenRect(world.registry, shownSelection, camera))
					(void)view.ScrollIntoView(*rect, max.x - min.x, max.y - min.y);
			}
		}
		if (Editing() && !painting && movingSelected && ImGui::IsItemActive() &&
		    ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0F))
		{
			const bool snapping = snapToGrid != io.KeyCtrl;
			const graphics::Position mouse{io.MousePos.x, io.MousePos.y};
			if (gizmoHandle == GizmoHandle::Ring)
			{
				// Turns every selected entity by how far the pointer has gone round the pivot, each about its own
				// centre. Snap rounds the result to 15 degrees.
				if (const auto pivot = gizmoPivot())
					dragDegrees += AngleBetween(*pivot, {mouse.x - io.MouseDelta.x, mouse.y - io.MouseDelta.y}, mouse);
				for (const DraggedEntity& item : dragged)
					(void)SetMovableRotation(world.registry, item.entity,
					                         TurnedRotation(item.rotation, dragDegrees, snapping ? kRotateSnap : 0.0F));
			}
			else if (gizmoTool == GizmoTool::Scale && gizmoHandle != GizmoHandle::None)
			{
				// Multiplies the scale of every selected entity by what the pointer's travel along the arrow comes to.
				// Snap rounds the result to 0.25.
				dragWindow.x += io.MouseDelta.x;
				dragWindow.y += io.MouseDelta.y;
				const float factor = ScaleFactor(gizmoHandle, gizmoRotation, dragWindow);
				const float step = snapping ? kScaleSnap : 0.0F;
				for (const DraggedEntity& item : dragged)
				{
					graphics::Scale scale = item.scale;
					if (gizmoHandle != GizmoHandle::Y) scale.x = ScaledValue(item.scale.x, factor, step);
					if (gizmoHandle != GizmoHandle::X) scale.y = ScaledValue(item.scale.y, factor, step);
					(void)SetMovableScale(world.registry, item.entity, scale);
				}
			}
			else
			{
				// The pointer's movement over the picture, in the picture's pixels. Each entity moves by that in its
				// own units (and in its parent's axes, if the parent is turned or stretched). Shift keeps one axis; the
				// grid applies when snapping is on, and Ctrl flips that for the drag. A gizmo arrow lets only its own
				// axis through, and the other axis stays exactly where it began.
				const graphics::Position step = gizmoHandle == GizmoHandle::None
				                                    ? graphics::Position{io.MouseDelta.x, io.MouseDelta.y}
				                                    : GizmoMove(gizmoHandle, {io.MouseDelta.x, io.MouseDelta.y});
				dragPixels.x += step.x / view.zoom;
				dragPixels.y += step.y / view.zoom;
				for (const DraggedEntity& item : dragged)
				{
					const float units = UnitsPerPixel(world.registry, item.entity, camera);
					const graphics::Position moved =
					    DragTarget({}, {dragPixels.x * units, dragPixels.y * units}, io.KeyShift, 0.0F);
					graphics::Position local = LocalDelta(world.registry, item.entity, moved);
					const bool axisArrow = gizmoHandle == GizmoHandle::X || gizmoHandle == GizmoHandle::Y;
					const bool turnedFrame = local.x != moved.x || local.y != moved.y;
					graphics::Position moveTo{};
					if (axisArrow && turnedFrame)
					{
						// The parent's axes are not the arrow's: snap how far it went along the arrow, in world units,
						// and then turn that into the entity's own, so it stays on the arrow's line.
						graphics::Position along = moved;
						if (snapping)
						{
							const auto cellSize = static_cast<float>(snapSize);
							along.x = std::round(along.x / cellSize) * cellSize;
							along.y = std::round(along.y / cellSize) * cellSize;
						}
						local = LocalDelta(world.registry, item.entity, along);
						moveTo = {item.start.x + local.x, item.start.y + local.y};
					}
					else
					{
						moveTo = DragTarget(item.start, local, false, snapping ? static_cast<float>(snapSize) : 0.0F);
						if (gizmoHandle == GizmoHandle::X && local.y == 0.0F) moveTo.y = item.start.y;
						if (gizmoHandle == GizmoHandle::Y && local.x == 0.0F) moveTo.x = item.start.x;
					}
					(void)SetMovablePosition(world.registry, item.entity, moveTo);
				}
			}
		}
		if (boxStart)
		{
			const graphics::Position here = view.ToPicture({io.MousePos.x, io.MousePos.y}, origin);
			const ScreenRect box{std::min(boxStart->x, here.x), std::min(boxStart->y, here.y),
			                     std::abs(here.x - boxStart->x), std::abs(here.y - boxStart->y)};
			if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				const graphics::Position a = view.ToWindow({box.x, box.y}, origin);
				const graphics::Position b = view.ToWindow({box.x + box.width, box.y + box.height}, origin);
				draw->AddRectFilled(ImVec2(a.x, a.y), ImVec2(b.x, b.y), IM_COL32(255, 200, 40, 40));
				draw->AddRect(ImVec2(a.x, a.y), ImVec2(b.x, b.y), IM_COL32(255, 200, 40, 200));
			}
			else
			{
				std::vector<ecs::Entity> chosen = boxBase;
				for (const ecs::Entity entity : EntitiesInBox(world.registry, camera, box))
					if (std::find(chosen.begin(), chosen.end(), entity) == chosen.end()) chosen.push_back(entity);
				panels->Select(ecs::Entity::Null);
				for (const ecs::Entity entity : chosen)
				{
					if (panels->selected.IsNull())
						panels->selected = entity;
					else
						panels->extra.push_back(entity);
				}
				boxStart.reset();
			}
		}
		if (painting) PaintStroke(*grid, origin, draw);
		// A right click that did not pan: a menu to Play with the game starting at that cell.
		if (Editing() && grid && config.onPlayFrom && hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		    ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 4.0F).x == 0.0F &&
		    ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 4.0F).y == 0.0F)
		{
			contextCell = CellAt(*grid, view.ToPicture({io.MousePos.x, io.MousePos.y}, origin));
			if (contextCell) ImGui::OpenPopup("play-from-here");
		}
		if (ImGui::BeginPopup("play-from-here"))
		{
			if (contextCell && ImGui::MenuItem("Play from here")) pendingPlayFrom = contextCell;
			ImGui::EndPopup();
		}
		panels->Prune(world.registry);
		for (const ecs::Entity entity : panels->All())
		{
			if (const auto corners = EntityScreenCorners(world.registry, entity, camera))
			{
				std::array<ImVec2, 4> points{};
				for (std::size_t i = 0; i < points.size(); ++i)
				{
					const graphics::Position at = view.ToWindow((*corners)[i], origin);
					points[i] = ImVec2(at.x, at.y);
				}
				// The primary selection, the one the Inspector shows, is brighter than the rest.
				const ImU32 color =
				    entity == panels->selected ? IM_COL32(255, 200, 40, 255) : IM_COL32(255, 160, 40, 150);
				for (std::size_t i = 0; i < points.size(); ++i)
					draw->AddLine(points[i], points[(i + 1) % points.size()], color, 2.0F);
			}
		}
		if (const auto pivot = gizmoPivot())
		{
			const GizmoShape shape;
			const GizmoHandle lit = gizmoHandle != GizmoHandle::None ? gizmoHandle : gizmoHover;
			const auto tint = [&](GizmoHandle handle, ImU32 plain, ImU32 bright)
			{
				return handle == lit ? bright : plain;
			};
			const ImU32 red = tint(GizmoHandle::X, IM_COL32(220, 60, 60, 255), IM_COL32(255, 150, 120, 255));
			const ImU32 green = tint(GizmoHandle::Y, IM_COL32(70, 190, 70, 255), IM_COL32(150, 255, 130, 255));
			const ImU32 white = tint(GizmoHandle::Free, IM_COL32(230, 230, 230, 255), IM_COL32(255, 230, 90, 255));
			const ImVec2 at(pivot->x, pivot->y);
			const auto square = [&](ImVec2 middle, float half, ImU32 color)
			{
				draw->AddRectFilled(ImVec2(middle.x - half, middle.y - half), ImVec2(middle.x + half, middle.y + half),
				                    IM_COL32(20, 20, 24, 200));
				draw->AddRect(ImVec2(middle.x - half, middle.y - half), ImVec2(middle.x + half, middle.y + half), color,
				              0.0F, 2.0F);
			};
			if (gizmoTool == GizmoTool::Translate)
			{
				const float tip = shape.arrowLength;
				draw->AddLine(at, ImVec2(at.x + tip, at.y), red, 2.0F);
				draw->AddTriangleFilled(ImVec2(at.x + tip + 8, at.y), ImVec2(at.x + tip - 4, at.y - 5),
				                        ImVec2(at.x + tip - 4, at.y + 5), red);
				draw->AddLine(at, ImVec2(at.x, at.y + tip), green, 2.0F);
				draw->AddTriangleFilled(ImVec2(at.x, at.y + tip + 8), ImVec2(at.x - 5, at.y + tip - 4),
				                        ImVec2(at.x + 5, at.y + tip - 4), green);
				square(at, shape.centre, white);
			}
			else if (gizmoTool == GizmoTool::Scale)
			{
				// The arrows lie along the shown entity's own axes, each ending in a square.
				const float radians = gizmoRotation * (std::numbers::pi_v<float> / 180.0F);
				const ImVec2 along(std::cos(radians), std::sin(radians));
				const ImVec2 down(-along.y, along.x);
				const float tip = shape.arrowLength;
				const ImVec2 xEnd(at.x + along.x * tip, at.y + along.y * tip);
				const ImVec2 yEnd(at.x + down.x * tip, at.y + down.y * tip);
				draw->AddLine(at, xEnd, red, 2.0F);
				draw->AddLine(at, yEnd, green, 2.0F);
				square(ImVec2(xEnd.x - along.x * 5.0F, xEnd.y - along.y * 5.0F), 5.0F, red);
				square(ImVec2(yEnd.x - down.x * 5.0F, yEnd.y - down.y * 5.0F), 5.0F, green);
				square(at, shape.centre, white);
			}
			else
			{
				const ImU32 ring = lit == GizmoHandle::Ring ? IM_COL32(255, 230, 90, 255) : IM_COL32(90, 150, 255, 255);
				draw->AddCircle(at, shape.ringRadius, ring, 64, 2.0F);
				// A tick on the ring where the shown entity points now, so a turn can be seen against it.
				const float radians = gizmoRotation * (std::numbers::pi_v<float> / 180.0F);
				draw->AddLine(
				    at,
				    ImVec2(at.x + std::cos(radians) * shape.ringRadius, at.y + std::sin(radians) * shape.ringRadius),
				    IM_COL32(90, 150, 255, 120), 1.0F);
				if (gizmoHandle == GizmoHandle::Ring)
				{
					char text[32];
					// What the first dragged entity is being set to (its own rotation, not the world one it is drawn
					// at).
					const float shown = dragged.empty() ? gizmoRotation
					                                    : TurnedRotation(dragged.front().rotation, dragDegrees,
					                                                     snapToGrid != io.KeyCtrl ? kRotateSnap : 0.0F);
					(void)std::snprintf(text, sizeof text, "%.0f deg", static_cast<double>(shown));
					draw->AddText(ImVec2(at.x + shape.ringRadius + 8.0F, at.y - 8.0F), IM_COL32(255, 230, 90, 255),
					              text);
				}
			}
		}
		draw->PopClipRect();
	}

	bool EditorGame::ShowSystemCursor() const
	{
		return play ? play->ShowSystemCursor() : true;
	}

	application::DisplayLink EditorGame::Display()
	{
		return play ? play->Display() : application::DisplayLink{};
	}

	void EditorGame::Request(Action action, const std::string& name)
	{
		if (dirty)
		{
			pendingAction = action;
			pendingName = name;
			dialog = Dialog::ConfirmDiscard;
			openDialog = true;
			return;
		}
		Perform(action, name);
	}

	void EditorGame::Perform(Action action, const std::string& name)
	{
		if (action == Action::New) NewScene();
		if (action == Action::Open) (void)OpenScene(name);
	}

	void EditorGame::SaveOrAsk()
	{
		if (SaveScene()) return;
		saveAsName = sceneName;
		dialog = Dialog::SaveAs;
		openDialog = true;
	}

	// Ctrl+C copy, Ctrl+V paste, Ctrl+Z undo, Ctrl+Y or Ctrl+Shift+Z redo, Ctrl+S save, Ctrl+Shift+S save as, Ctrl+N
	// new, Ctrl+D duplicate and Delete remove the selected entity (with everything under it), F5 Play or Stop, F6
	// restart Play. A text box being typed in keeps its keys, and nothing happens in the middle of a drag.
	void EditorGame::HandleShortcuts()
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool inViewport = viewportActive; // set again by the Viewport if it is drawn this frame
		viewportActive = false;
		// The game gets the keys while it is played over the Viewport. ImGui's own keyboard navigation is off then, or
		// the arrow keys would also move its focus around the panels.
		// With the Game panel shown the keys go to the game only while that panel has the pointer or focus.
		const bool inGame = gameActive;
		gameActive = false;
		const bool gameHasKeys = play != nullptr && (split ? inGame : inViewport);
		editor.SetGameKeys(gameHasKeys);
		if (gameHasKeys && (io.ConfigFlags & ImGuiConfigFlags_NavEnableKeyboard) != 0)
		{
			io.ConfigFlags &= ~ImGuiConfigFlags_NavEnableKeyboard;
			navigationClearedForPlay = true;
		}
		else if (!gameHasKeys && navigationClearedForPlay)
		{
			io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
			navigationClearedForPlay = false;
		}
		if (pendingPlayFrom)
		{
			const GridCell cell = *pendingPlayFrom;
			pendingPlayFrom.reset();
			(void)StartPlayFrom(cell);
		}
		if (io.WantTextInput || wasBusy || dialog != Dialog::None) return;

		if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
		{
			if (play)
				StopPlay();
			else
				(void)StartPlay();
			return;
		}
		if (play)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_F6, false)) (void)RestartPlay();
			if (ImGui::IsKeyPressed(ImGuiKey_F7, false)) playPaused = !playPaused;
			if (ImGui::IsKeyPressed(ImGuiKey_F8, true)) StepPlay();
			if (!split || gameHasKeys) return; // the rest edit the scene, which the game's keys must not
		}
		if (inViewport && gizmoHandle == GizmoHandle::None && !io.KeyCtrl && !io.KeyAlt && !io.KeyShift)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) gizmoTool = GizmoTool::Translate;
			if (ImGui::IsKeyPressed(ImGuiKey_E, false)) gizmoTool = GizmoTool::Rotate;
			if (ImGui::IsKeyPressed(ImGuiKey_R, false)) gizmoTool = GizmoTool::Scale;
		}

		if (io.KeyCtrl)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift)
				(void)Undo();
			else if (ImGui::IsKeyPressed(ImGuiKey_Y) || (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyShift))
				(void)Redo();
			else if (ImGui::IsKeyPressed(ImGuiKey_S, false))
			{
				if (io.KeyShift)
				{
					saveAsName = sceneName;
					dialog = Dialog::SaveAs;
					openDialog = true;
				}
				else
					SaveOrAsk();
			}
			else if (ImGui::IsKeyPressed(ImGuiKey_N, false))
				Request(Action::New, {});
			else if (ImGui::IsKeyPressed(ImGuiKey_C, false))
				CopySelection();
			else if (ImGui::IsKeyPressed(ImGuiKey_V, false))
				PasteClipboard();
		}

		ecs::Registry& registry = edit->world.registry;
		panels->Prune(registry);
		if (panels->selected.IsNull()) return;
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.KeyCtrl && !io.KeyShift && !io.KeyAlt)
		{
			DeleteSelection(registry, *panels);
		}
		else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
		{
			DuplicateSelection(registry, EnsureTypes(edit->world), *panels);
		}
		else if (!io.KeyCtrl && !io.KeyAlt && inViewport)
		{
			// Arrow keys nudge the selection one unit (Shift, or Snap on: one grid step, landing on the grid). Only
			// while the Viewport has the pointer or focus, so arrowing through a list or tree elsewhere moves nothing.
			const float step = io.KeyShift || snapToGrid ? static_cast<float>(snapSize) : 1.0F;
			graphics::Position by{};
			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) by.x -= step;
			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) by.x += step;
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) by.y -= step;
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) by.y += step;
			if (by.x != 0.0F || by.y != 0.0F)
			{
				for (const ecs::Entity entity : SelectionRoots(registry, *panels))
				{
					if (const auto at = MovablePosition(registry, entity))
						(void)SetMovablePosition(registry, entity,
						                         DragTarget(*at, LocalDelta(registry, entity, by), false,
						                                    snapToGrid ? static_cast<float>(snapSize) : 0.0F));
				}
			}
		}
	}

	// Copy puts the selected entities, each with what is under it, on the clipboard as scene text; Paste creates them
	// in the open scene, which may not be the one they came from.
	void EditorGame::CopySelection()
	{
		ecs::Registry& registry = edit->world.registry;
		panels->Prune(registry);
		const std::vector<ecs::Entity> roots = SelectionRoots(registry, *panels);
		if (roots.empty()) return;
		ecs::Issues issues;
		const std::string text = CopyTrees(registry, EnsureTypes(edit->world), roots, issues);
		if (text.empty())
			panels->status = "can't copy: " + (issues.errors.empty()
			                                       ? std::string("nothing to copy")
			                                       : issues.errors.front().path + ": " + issues.errors.front().message);
		else
			ImGui::SetClipboardText(text.c_str());
	}

	void EditorGame::PasteClipboard()
	{
		const char* text = ImGui::GetClipboardText();
		if (text == nullptr || *text == '\0') return;
		ecs::Issues issues;
		const std::vector<ecs::Entity> pasted =
		    PasteTrees(edit->world.registry, EnsureTypes(edit->world), text, issues);
		if (pasted.empty())
		{
			panels->status = "can't paste: " +
			                 (issues.errors.empty() ? std::string("nothing there") : issues.errors.front().message);
			return;
		}
		panels->Select(pasted.front());
		panels->extra.assign(pasted.begin() + 1, pasted.end());
		if (!issues.warnings.empty())
			panels->status = "pasted, but " + std::to_string(issues.warnings.size()) + " warning(s), e.g. " +
			                 issues.warnings.front().path + ": " + issues.warnings.front().message;
	}

	std::filesystem::path EditorGame::PrefabsFolder() const
	{
		return PrefabFolder(config.assetFolder);
	}

	// A copy of the prefab at at (picture pixels) in the open scene, selected.
	void EditorGame::PutPrefab(const std::string& name, graphics::Position at)
	{
		std::string error;
		ecs::Registry& registry = edit->world.registry;
		const ecs::Entity root = InstantiatePrefab(registry, EnsureTypes(edit->world), PrefabsFolder(), name, error);
		if (root.IsNull())
		{
			message = "can't use prefab " + name + ": " + error;
			return;
		}
		(void)SetMovablePosition(registry, root, at);
		panels->Select(root);
		message.clear();
	}

	bool EditorGame::SavePrefabAs(const std::string& name)
	{
		panels->Prune(edit->world.registry);
		std::string error;
		if (panels->selected.IsNull() ||
		    !SavePrefab(edit->world.registry, EnsureTypes(edit->world), panels->selected, PrefabsFolder(), name, error))
		{
			message = "can't save prefab: " + (error.empty() ? std::string("nothing is selected") : error);
			return false;
		}
		message = "saved prefab " + name;
		return true;
	}

	void EditorGame::ApplySelectedPrefab()
	{
		std::string error;
		if (ApplyToPrefab(edit->world.registry, EnsureTypes(edit->world), panels->selected, PrefabsFolder(), error))
			message = "applied to prefab " + PrefabNameOf(edit->world.registry, panels->selected);
		else
			message = "can't apply: " + error;
	}

	void EditorGame::RevertSelectedPrefab()
	{
		std::string error;
		const ecs::Entity fresh =
		    RevertToPrefab(edit->world.registry, EnsureTypes(edit->world), panels->selected, PrefabsFolder(), error);
		if (fresh.IsNull())
		{
			message = "can't revert: " + error;
			return;
		}
		panels->Select(fresh);
		message.clear();
	}

	void EditorGame::DrawMenuBar()
	{
		HandleShortcuts();
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) (void)Undo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) (void)Redo();
			ImGui::Separator();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, Editing() && !panels->selected.IsNull())) CopySelection();
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, Editing())) PasteClipboard();
			ImGui::Separator();
			if (ImGui::BeginMenu("Prefab", Editing() && !config.assetFolder.empty()))
			{
				panels->Prune(edit->world.registry);
				const bool hasSelection = !panels->selected.IsNull();
				const std::string instanceOf = PrefabNameOf(edit->world.registry, panels->selected);
				if (ImGui::MenuItem("Save selection as prefab...", nullptr, false, hasSelection))
				{
					prefabName = instanceOf;
					message.clear();
					dialog = Dialog::SavePrefab;
					openDialog = true;
				}
				if (ImGui::MenuItem(
				        ("Apply to prefab" + (instanceOf.empty() ? std::string() : " (" + instanceOf + ")")).c_str(),
				        nullptr, false, !instanceOf.empty()))
					ApplySelectedPrefab();
				if (ImGui::MenuItem("Revert to prefab", nullptr, false, !instanceOf.empty())) RevertSelectedPrefab();
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}
		if (config.menu && Editing()) config.menu(edit->world);
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N")) Request(Action::New, {});
			if (ImGui::BeginMenu("Open"))
			{
				std::vector<std::string> names;
				std::error_code error;
				for (const auto& entry : std::filesystem::directory_iterator(config.sceneFolder, error))
				{
					if (entry.is_regular_file() && entry.path().extension() == ".json")
					{
						names.push_back(entry.path().filename().string());
					}
				}
				std::sort(names.begin(), names.end());
				if (names.empty()) ImGui::TextDisabled("no scenes in the scene folder");
				for (const std::string& name : names)
				{
					if (ImGui::MenuItem(name.c_str(), nullptr, name == sceneName)) Request(Action::Open, name);
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Open recent", !recent.empty()))
			{
				for (const std::string& name : std::vector<std::string>(recent))
					if (ImGui::MenuItem(name.c_str(), nullptr, name == sceneName)) Request(Action::Open, name);
				ImGui::EndMenu();
			}
			if (ImGui::MenuItem("Save", "Ctrl+S", false, Editing()))
			{
				if (!SaveScene())
				{
					saveAsName = sceneName;
					dialog = Dialog::SaveAs;
					openDialog = true;
				}
			}
			if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S", false, Editing()))
			{
				saveAsName = sceneName;
				dialog = Dialog::SaveAs;
				openDialog = true;
			}
			ImGui::EndMenu();
		}

		if (play && ImGui::MenuItem("Restart", "F6")) (void)RestartPlay();
		if (play)
		{
			if (ImGui::MenuItem(playPaused ? "Resume" : "Pause", "F7")) playPaused = !playPaused;
			if (ImGui::MenuItem("Step", "F8")) StepPlay();
			if (ImGui::BeginMenu("Speed"))
			{
				for (const float option : {0.25F, 0.5F, 1.0F, 2.0F, 4.0F})
				{
					char label[16];
					(void)std::snprintf(label, sizeof label, "%gx", static_cast<double>(option));
					if (ImGui::MenuItem(label, nullptr, playSpeed == option)) SetPlaySpeed(option);
				}
				ImGui::EndMenu();
			}
			ImGui::MenuItem("Mute", nullptr, &playMuted);
		}
		if (ImGui::MenuItem(play ? "Stop" : "Play", "F5"))
		{
			if (play)
			{
				StopPlay();
			}
			else
			{
				(void)StartPlay();
			}
		}

		if (play)
		{
			ImGui::TextColored(ImVec4(1.0F, 0.8F, 0.3F, 1.0F), playPaused ? "PAUSED (F7 resumes, F8 steps)"
			                                                              : "PLAYING (changes are discarded on Stop)");
		}
		else
		{
			ImGui::TextDisabled("%s%s", sceneName.empty() ? "(new scene)" : sceneName.c_str(), dirty ? " *" : "");
		}
		if (!message.empty()) ImGui::TextColored(ImVec4(1.0F, 0.4F, 0.4F, 1.0F), "%s", message.c_str());
	}

	void EditorGame::DrawDialogs()
	{
		if (dialog == Dialog::ConfirmDiscard)
		{
			if (openDialog) ImGui::OpenPopup("Unsaved changes");
			openDialog = false;
			if (ImGui::BeginPopupModal("Unsaved changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("The scene has changes that are not saved.");
				if (ImGui::Button("Discard them"))
				{
					dialog = Dialog::None;
					const Action action = pendingAction;
					const std::string name = pendingName;
					pendingAction = Action::None;
					ImGui::CloseCurrentPopup();
					Perform(action, name);
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel"))
				{
					dialog = Dialog::None;
					pendingAction = Action::None;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		else if (dialog == Dialog::Recover)
		{
			if (openDialog) ImGui::OpenPopup("Unsaved changes found");
			openDialog = false;
			if (ImGui::BeginPopupModal("Unsaved changes found", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::Text("%s has an autosave newer than its file, from changes that were never saved.",
				            sceneName.empty() ? "The new scene" : sceneName.c_str());
				if (ImGui::Button("Restore them"))
				{
					(void)AcceptRecovery();
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine();
				if (ImGui::Button("Throw them away"))
				{
					DeclineRecovery();
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		else if (dialog == Dialog::SavePrefab)
		{
			if (openDialog) ImGui::OpenPopup("Save prefab");
			openDialog = false;
			if (ImGui::BeginPopupModal("Save prefab", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::TextUnformatted("Saves the selected entity and everything under it.");
				ImGui::InputText("name", &prefabName);
				if (!message.empty()) ImGui::TextColored(ImVec4(1.0F, 0.6F, 0.3F, 1.0F), "%s", message.c_str());
				const bool save = ImGui::Button("Save");
				ImGui::SameLine();
				const bool cancel = ImGui::Button("Cancel");
				if (save && SavePrefabAs(prefabName))
				{
					dialog = Dialog::None;
					ImGui::CloseCurrentPopup();
				}
				if (cancel)
				{
					dialog = Dialog::None;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		else if (dialog == Dialog::SaveAs)
		{
			if (openDialog) ImGui::OpenPopup("Save scene as");
			openDialog = false;
			if (ImGui::BeginPopupModal("Save scene as", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
			{
				ImGui::InputText("file name", &saveAsName);
				const bool save = ImGui::Button("Save");
				ImGui::SameLine();
				const bool cancel = ImGui::Button("Cancel");
				if (save && SaveSceneAs(saveAsName))
				{
					dialog = Dialog::None;
					ImGui::CloseCurrentPopup();
				}
				if (cancel)
				{
					dialog = Dialog::None;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
	}
} // namespace engine::editor
