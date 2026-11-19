#include "editor/editorgame.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <system_error>
#include <utility>

#include "core/json.hpp"
#include "ecs/enginecomponents.hpp"
#include "ecs/scene.hpp"
#include "ecs/typeregistry.hpp"

namespace engine::editor
{
	namespace
	{
		constexpr int kFramesBetweenDirtyChecks = 20;

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

	EditorGame::EditorGame(Editor& editorToUse, EditorGameConfig configToUse)
	    : editor(editorToUse), config(std::move(configToUse))
	{
		if (!config.makeGame) return;
		edit = config.makeGame();
		if (edit == nullptr) return;
		EnsureTypes(edit->world);
		ClearWorld(edit->world);
		savedText = SceneText();

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
		dirty = SceneText() != savedText;
	}

	void EditorGame::NewScene()
	{
		if (play) StopPlay();
		ClearWorld(edit->world);
		if (config.afterLoad) config.afterLoad(edit->world);
		sceneName.clear();
		savedText = SceneText();
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

	void EditorGame::Render(graphics::Renderer& renderer, double alpha)
	{
		if (play)
		{
			play->Render(renderer, alpha);
			return;
		}
		if (++framesSinceCheck >= kFramesBetweenDirtyChecks) CheckDirty();
		edit->Render(renderer, alpha);
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

	void EditorGame::DrawMenuBar()
	{
		if (ImGui::BeginMenu("Scene"))
		{
			if (ImGui::MenuItem("New")) Request(Action::New, {});
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
			if (ImGui::MenuItem("Save", nullptr, false, !play))
			{
				if (!SaveScene())
				{
					dialog = Dialog::SaveAs;
					openDialog = true;
				}
			}
			if (ImGui::MenuItem("Save as...", nullptr, false, !play))
			{
				saveAsName = sceneName;
				dialog = Dialog::SaveAs;
				openDialog = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::MenuItem(play ? "Stop" : "Play"))
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
