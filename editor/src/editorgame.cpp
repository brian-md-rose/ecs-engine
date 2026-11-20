#include "editor/editorgame.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cmath>
#include <system_error>
#include <utility>

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
		editor.SetPanelSize("Viewport", 700, 480);
		editor.SetPanelOpen("Viewport", true);
		editor.SetVisible(true);
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
		if (!play && !history.empty() && text != history[historyPos]) Commit(text);
	}

	void EditorGame::ResetHistory()
	{
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
		std::size_t selectedIndex = before.size();
		for (std::size_t i = 0; i < before.size(); ++i)
			if (before[i] == panels->selected) selectedIndex = i;

		const ecs::SceneLoadResult loaded = ecs::LoadScene(world.registry, EnsureTypes(world), *parsed.value);
		if (!loaded.Ok())
		{
			message = "can't go back: " + FirstProblem(loaded.issues);
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
		if (config.afterLoad) config.afterLoad(world);

		panels->selected = ecs::Entity::Null;
		const std::vector<ecs::Entity> after = world.registry.AllEntities();
		if (selectedIndex < after.size()) panels->selected = after[selectedIndex];

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
		ClearWorld(edit->world);
		if (config.afterLoad) config.afterLoad(edit->world);
		sceneName.clear();
		savedText = SceneText();
		ResetHistory();
		dirty = false;
		message.clear();
		panels->selected = ecs::Entity::Null;
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
			return false;
		}
		for (const ecs::Entity entity : before)
			world.registry.Destroy(entity);
		if (config.afterLoad) config.afterLoad(world);
		sceneName = name;
		savedText = SceneText();
		ResetHistory();
		dirty = false;
		message = loaded.issues.warnings.empty()
		              ? std::string()
		              : std::to_string(loaded.issues.warnings.size()) + " warning(s) opening " + name;
		panels->selected = ecs::Entity::Null;
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
			return false;
		}
		sceneName = file;
		savedText = SceneText();
		dirty = false;
		message.clear();
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
			return false;
		}
		if (config.afterLoad) config.afterLoad(game->world);
		play = std::move(game);
		message.clear();
		panels->selected = ecs::Entity::Null;
		return true;
	}

	void EditorGame::StopPlay()
	{
		if (!play) return;
		play.reset();
		panels->selected = ecs::Entity::Null;
	}

	void EditorGame::Update(double dt, const input::Input& input, audio::AudioDevice* audio)
	{
		if (!play) return; // an edited scene is still
		play->Update(dt, input, audio);
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

	void EditorGame::DrawViewport()
	{
		if (targetHandle == nullptr || target == graphics::kNoTexture)
		{
			ImGui::TextDisabled("The game has not drawn a picture yet.");
			return;
		}
		if (ImGui::Button("Reset view")) view = ViewTransform{};
		ImGui::SameLine();
		ImGui::Text("%.0f%%   wheel zooms, middle or right drag pans, click selects",
		            static_cast<double>(view.zoom) * 100.0);

		ImVec2 size = ImGui::GetContentRegionAvail();
		size.x = std::max(size.x, 16.0F);
		size.y = std::max(size.y, 16.0F);
		ImGui::InvisibleButton("##viewport", size, ImGuiButtonFlags_MouseButtonLeft);
		const bool hovered = ImGui::IsItemHovered();
		const ImVec2 min = ImGui::GetItemRectMin();
		const ImVec2 max = ImGui::GetItemRectMax();
		const graphics::Position origin{min.x, min.y};
		const ImGuiIO& io = ImGui::GetIO();

		if (hovered && io.MouseWheel != 0.0F)
			view.ZoomAt(std::pow(1.15F, io.MouseWheel), {io.MousePos.x, io.MousePos.y}, origin);
		if (hovered && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0F) ||
		                ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0F)))
		{
			view.panX += io.MouseDelta.x;
			view.panY += io.MouseDelta.y;
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
		// Press selects what is under the pointer (or nothing); dragging from there moves it, so the selected
		// entity can be dragged without a separate handle.
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			const graphics::Position picture = view.ToPicture({io.MousePos.x, io.MousePos.y}, origin);
			panels->selected = PickEntity(world.registry, camera, picture);
			movingSelected = panels->selected != ecs::Entity::Null;
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) movingSelected = false;
		if (movingSelected && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0F))
		{
			// The pointer's movement over the picture, in the picture's pixels.
			(void)MoveEntity(world.registry, panels->selected, camera,
			                 {io.MouseDelta.x / view.zoom, io.MouseDelta.y / view.zoom});
		}
		if (panels->selected != ecs::Entity::Null && world.registry.IsAlive(panels->selected))
		{
			if (const auto rect = EntityScreenRect(world.registry, panels->selected, camera))
			{
				const graphics::Position a = view.ToWindow({rect->x, rect->y}, origin);
				const graphics::Position b = view.ToWindow({rect->x + rect->width, rect->y + rect->height}, origin);
				draw->AddRect(ImVec2(a.x - 1, a.y - 1), ImVec2(b.x + 1, b.y + 1), IM_COL32(255, 200, 40, 255), 0.0F,
				              2.0F);
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

	// Ctrl+Z undo, Ctrl+Y or Ctrl+Shift+Z redo, Ctrl+S save, Ctrl+Shift+S save as, Ctrl+N new, Ctrl+D duplicate and
	// Delete remove the selected entity (with everything under it), F5 Play or Stop. A text box being typed in keeps
	// its keys, and nothing happens in the middle of a drag.
	void EditorGame::HandleShortcuts()
	{
		const ImGuiIO& io = ImGui::GetIO();
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
		}

		ecs::Registry& registry = edit->world.registry;
		if (panels->selected.IsNull() || !registry.IsAlive(panels->selected)) return;
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
		{
			DestroyTree(registry, panels->selected);
			panels->selected = ecs::Entity::Null;
		}
		else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
		{
			ecs::Issues issues;
			const ecs::Entity copy = DuplicateTree(registry, EnsureTypes(edit->world), panels->selected, issues);
			if (!copy.IsNull())
				panels->selected = copy;
			else if (!issues.errors.empty())
				panels->status =
				    "can't duplicate: " + issues.errors.front().path + ": " + issues.errors.front().message;
		}
	}

	void EditorGame::DrawMenuBar()
	{
		HandleShortcuts();
		if (ImGui::BeginMenu("Edit"))
		{
			if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) (void)Undo();
			if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) (void)Redo();
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
