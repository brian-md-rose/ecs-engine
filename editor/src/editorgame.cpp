#include "editor/editorgame.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <system_error>
#include <utility>

#include "core/files.hpp"
#include "core/json.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"
#include "editor/entityops.hpp"
#include "graphics/renderer2d.hpp"

namespace engine::editor
{
	namespace
	{
		constexpr int kFramesBetweenDirtyChecks = 20;
		constexpr std::size_t kMaxHistory = 100;

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
		editor.AddPanel("Issues", [this]
		{
			DrawIssues();
		});
		editor.SetPanelSize("Viewport", 700, 480);
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
		if (file.empty() || play) return false;
		CheckDirty();
		if (!dirty)
		{
			DropAutosave(); // changes undone or saved: nothing left to bring back
			return false;
		}
		std::error_code ignored;
		std::filesystem::create_directories(file.parent_path(), ignored);
		// Atomically, so a crash in the middle leaves the last whole autosave and not half of one.
		return core::WriteFileAtomic(file, SceneText() + '\n');
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
		const ecs::SceneLoadResult loaded = ecs::LoadSceneFile(world.registry, EnsureTypes(world), file);
		if (!loaded.Ok())
		{
			message = "can't restore the autosave: " + FirstProblem(loaded.issues);
			ShowIssues("restoring the autosave", loaded.issues);
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
		if (config.afterLoad) config.afterLoad(world);
		ResetHistory();
		CheckDirty(); // against the saved text, so the restored scene shows as unsaved
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
		const core::JsonValue scene = ecs::SaveScene(edit->world.registry, EnsureTypes(edit->world), issues);
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
		if (!play && !history.empty() && text != history[historyPos]) problemsStale = true;
		if (!play && !history.empty() && text != history[historyPos]) Commit(text);
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
		if (play || position >= history.size()) return false;
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
		if (play) return false;
		CheckDirty(); // a change not yet looked at becomes a step first, so undoing it is possible
		return historyPos > 0 && GoTo(historyPos - 1);
	}

	bool EditorGame::Redo()
	{
		if (play) return false;
		CheckDirty();
		return GoTo(historyPos + 1);
	}

	void EditorGame::NewScene()
	{
		if (play) StopPlay();
		DropAutosave(); // the changes were given up (or saved) by now
		ClearWorld(edit->world);
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
		const ecs::SceneLoadResult loaded =
		    ecs::LoadSceneFile(world.registry, EnsureTypes(world), config.sceneFolder / name);
		if (!loaded.Ok())
		{
			message = "can't open " + name + ": " + FirstProblem(loaded.issues);
			ShowIssues("opening " + name, loaded.issues);
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
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
		if (!ecs::SaveSceneFile(edit->world.registry, EnsureTypes(edit->world), config.sceneFolder / file, issues))
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
		CheckDirty(); // so the prompt on New and Open while playing knows about edits made before Play
		ecs::Issues issues;
		const core::JsonValue snapshot = ecs::SaveScene(edit->world.registry, EnsureTypes(edit->world), issues);
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
		play->Update(dt, gameInput, audio);
		playMouse.mouse.EndUpdate();
		if (play->WantsToQuit()) StopPlay(); // the game's Quit ends the run, not the editor
	}

	graphics::Camera EditorGame::CurrentCamera()
	{
		if (const auto* renderer2D = CurrentWorld().resources.TryGet<graphics::Renderer2D>()) return renderer2D->camera;
		return {};
	}

	void EditorGame::Render(graphics::Renderer& renderer, double alpha)
	{
		application::ScheduledGame& current = play ? *play : *edit;
		if (!play)
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
		const graphics::Camera camera = CurrentCamera();
		if (camera.logicalWidth > 0 && camera.logicalHeight > 0 &&
		    (camera.logicalWidth != targetWidth || camera.logicalHeight != targetHeight))
		{
			renderer.DestroyTexture(target);
			target = renderer.CreateRenderTarget(camera.logicalWidth, camera.logicalHeight);
			targetWidth = camera.logicalWidth;
			targetHeight = camera.logicalHeight;
			targetHandle = renderer.GetTextureHandle(target);
		}
		if (target != graphics::kNoTexture && renderer.SetRenderTarget(target))
		{
			current.Render(renderer, alpha);
			(void)renderer.SetRenderTarget(graphics::kNoTexture);
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

		if (ImGui::IsItemActive())
		{
			stroking = true;
			if (under)
			{
				const std::vector<GridCell> cells =
				    lastCell ? CellLine(*lastCell, *under) : std::vector<GridCell>{*under};
				const GridTool& tool = config.gridTool;
				for (const GridCell& cell : cells)
				{
					if (brush >= 0 && static_cast<std::size_t>(brush) < tool.palette.size() && tool.paint)
						tool.paint(edit->world, static_cast<std::size_t>(brush), cell);
					else if (brush == kBrushErase && tool.erase)
						tool.erase(edit->world, cell);
				}
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
	}

	void EditorGame::DrawViewport()
	{
		if (targetHandle == nullptr || target == graphics::kNoTexture)
		{
			ImGui::TextDisabled("The game has not drawn a picture yet.");
			return;
		}
		if (ImGui::Button("Reset view")) view = ViewTransform{};
		ImGui::SameLine();
		ImGui::Text(play ? "%.0f%%   the game has the mouse; hold Ctrl to zoom and pan"
		                 : "%.0f%%   wheel zooms, middle or right drag pans, click selects",
		            static_cast<double>(view.zoom) * 100.0);

		if (!play)
		{
			ImGui::SameLine();
			ImGui::Checkbox("Snap", &snapToGrid);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Dragging and nudging land on multiples of the grid size.\nHold Ctrl while dragging "
				                  "to flip it; Shift keeps one axis.");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(56.0F);
			ImGui::InputInt("grid", &snapSize, 0);
			snapSize = std::clamp(snapSize, 1, 256);
		}
		// The grid tool's palette, when the scene being edited has a grid (a maze).
		std::optional<GridInfo> grid;
		if (!play && config.gridTool.grid) grid = config.gridTool.grid(edit->world);
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

		// While playing, the game gets the wheel and the buttons; hold Ctrl to zoom and pan the view instead.
		const bool viewKeys = !play || io.KeyCtrl;
		if (hovered && viewKeys && io.MouseWheel != 0.0F)
			view.ZoomAt(std::pow(1.15F, io.MouseWheel), {io.MousePos.x, io.MousePos.y}, origin);
		if (hovered && viewKeys &&
		    (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F) ||
		     ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0F)))
		{
			view.panX += io.MouseDelta.x;
			view.panY += io.MouseDelta.y;
		}

		if (play)
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
		// Press selects what is under the pointer; dragging from there moves it (and the rest of the selection it is
		// part of), so no separate handle is needed. Ctrl or Shift adds to the selection or takes the entity out of it.
		// A press on empty space starts a box: what it covers when released is selected (added to the selection with
		// Ctrl or Shift).
		if (!play && !painting && ImGui::IsItemClicked(ImGuiMouseButton_Left))
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
			if (movingSelected)
			{
				for (const ecs::Entity entity : SelectionRoots(world.registry, *panels))
					if (const auto at = MovablePosition(world.registry, entity)) dragged.push_back({entity, *at});
			}
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
		}
		// A selection made elsewhere (the Hierarchy) that is off-screen is scrolled into view, so it can be seen. While
		// playing the selection is only noted, so Stop does not scroll to a change made during Play.
		if (panels->selected != shownSelection)
		{
			shownSelection = panels->selected;
			if (!play && shownSelection != ecs::Entity::Null && world.registry.IsAlive(shownSelection))
			{
				if (const auto rect = EntityScreenRect(world.registry, shownSelection, camera))
					(void)view.ScrollIntoView(*rect, max.x - min.x, max.y - min.y);
			}
		}
		if (!play && !painting && movingSelected && ImGui::IsItemActive() &&
		    ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0F))
		{
			// The pointer's movement over the picture, in the picture's pixels. Each entity moves by that in its own
			// units. Shift keeps one axis; the grid applies when snapping is on, and Ctrl flips that for the drag.
			dragPixels.x += io.MouseDelta.x / view.zoom;
			dragPixels.y += io.MouseDelta.y / view.zoom;
			const bool snapping = snapToGrid != io.KeyCtrl;
			for (const DraggedEntity& item : dragged)
			{
				const float units = UnitsPerPixel(world.registry, item.entity, camera);
				(void)SetMovablePosition(world.registry, item.entity,
				                         DragTarget(item.start, {dragPixels.x * units, dragPixels.y * units},
				                                    io.KeyShift, snapping ? static_cast<float>(snapSize) : 0.0F));
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
		panels->Prune(world.registry);
		for (const ecs::Entity entity : panels->All())
		{
			if (const auto rect = EntityScreenRect(world.registry, entity, camera))
			{
				const graphics::Position a = view.ToWindow({rect->x, rect->y}, origin);
				const graphics::Position b = view.ToWindow({rect->x + rect->width, rect->y + rect->height}, origin);
				// The primary selection, the one the Inspector shows, is brighter than the rest.
				const ImU32 color =
				    entity == panels->selected ? IM_COL32(255, 200, 40, 255) : IM_COL32(255, 160, 40, 150);
				draw->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), color, 0.0F, 2.0F);
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
	// new, Ctrl+D duplicate and Delete remove the selected entity (with everything under it), F5 Play or Stop. A text
	// box being typed in keeps its keys, and nothing happens in the middle of a drag.
	void EditorGame::HandleShortcuts()
	{
		const ImGuiIO& io = ImGui::GetIO();
		const bool inViewport = viewportActive; // set again by the Viewport if it is drawn this frame
		viewportActive = false;
		if (io.WantTextInput || wasBusy || dialog != Dialog::None) return;

		if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
		{
			if (play)
				StopPlay();
			else
				(void)StartPlay();
			return;
		}
		if (play) return; // the rest edit the scene

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
						(void)SetMovablePosition(
						    registry, entity,
						    DragTarget(*at, by, false, snapToGrid ? static_cast<float>(snapSize) : 0.0F));
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

	void EditorGame::DrawMenuBar()
	{
		HandleShortcuts();
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) (void)Undo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) (void)Redo();
			ImGui::Separator();
			if (ImGui::MenuItem("Copy", "Ctrl+C", false, !play && !panels->selected.IsNull())) CopySelection();
			if (ImGui::MenuItem("Paste", "Ctrl+V", false, !play)) PasteClipboard();
			ImGui::EndMenu();
		}
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
			if (ImGui::MenuItem("Save", "Ctrl+S", false, !play))
			{
				if (!SaveScene())
				{
					saveAsName = sceneName;
					dialog = Dialog::SaveAs;
					openDialog = true;
				}
			}
			if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S", false, !play))
			{
				saveAsName = sceneName;
				dialog = Dialog::SaveAs;
				openDialog = true;
			}
			ImGui::EndMenu();
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
			ImGui::TextColored(ImVec4(1.0F, 0.8F, 0.3F, 1.0F), "PLAYING (changes are discarded on Stop)");
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
