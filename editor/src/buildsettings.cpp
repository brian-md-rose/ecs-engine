#include "editor/buildsettings.hpp"

#include <algorithm>
#include <iterator>
#include <memory>
#include <system_error>
#include <utility>

#include <SDL3/SDL_process.h>
#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include "core/log.hpp"

namespace engine::editor
{
	namespace
	{
		namespace fs = std::filesystem;

		constexpr const char* kManifestName = "build.json";

		bool Contains(const std::vector<std::string>& list, const std::string& name)
		{
			return std::find(list.begin(), list.end(), name) != list.end();
		}

		fs::path Canonical(const fs::path& path)
		{
			std::error_code error;
			fs::path result = fs::weakly_canonical(path, error);
			return error ? path.lexically_normal() : result;
		}

		// True if inner is the same as outer or inside it.
		bool IsInside(const fs::path& inner, const fs::path& outer)
		{
			const fs::path relative = Canonical(inner).lexically_relative(Canonical(outer));
			if (relative.empty()) return false;
			return *relative.begin() != "..";
		}

		bool IsPackagedScene(const fs::path& relative)
		{
			// relative is below the assets folder: scenes/<name>.json is a scene.
			const auto begin = relative.begin();
			if (begin == relative.end() || *begin != "scenes") return false;
			return relative.extension() == ".json" && std::distance(relative.begin(), relative.end()) == 2;
		}

		std::vector<std::string> SelectedSceneFiles(const BuildSettings& settings)
		{
			std::vector<std::string> names;
			names.reserve(settings.scenes.size());
			for (const std::string& scene : settings.scenes)
				names.push_back(scene + ".json");
			return names;
		}

		struct PanelState
		{
			BuildSettingsConfig config;
			BuildSettings settings;
			std::vector<std::string> available;
			std::string status;
			bool statusIsError = false;
			bool loaded = false;
			std::vector<std::string> problems; // cached: BuildProblems touches the disk
			double problemsChecked = -1.0;     // ImGui time of the last check; also redone after any edit
		};

		void Rescan(PanelState& state)
		{
			state.available = ListSceneNames(state.config.sceneFolder);
			// A scene that was deleted since is dropped, so the package never names a file that isn't there.
			std::erase_if(state.settings.scenes, [&](const std::string& name)
			{
				return !Contains(state.available, name);
			});
			if (!Contains(state.settings.scenes, state.settings.startupScene)) state.settings.startupScene.clear();
		}

		void Load(PanelState& state)
		{
			state.settings =
			    state.config.settingsFile.empty() ? BuildSettings{} : LoadBuildSettings(state.config.settingsFile);
			const bool fresh = state.config.settingsFile.empty() || !fs::exists(state.config.settingsFile);
			if (state.settings.gameExe.empty()) state.settings.gameExe = state.config.defaultGameExe.string();
			if (state.settings.outputFolder.empty()) state.settings.outputFolder = state.config.defaultOutput.string();
			state.available = ListSceneNames(state.config.sceneFolder);
			if (fresh) state.settings.scenes = state.available; // first time: everything ships
			Rescan(state);
			state.loaded = true;
		}

		void Save(PanelState& state)
		{
			if (!state.config.settingsFile.empty()) SaveBuildSettings(state.config.settingsFile, state.settings);
		}

		void Report(PanelState& state, const PackageResult& result)
		{
			state.status = result.message;
			state.statusIsError = !result.ok;
			if (result.ok)
				core::LogInfo(result.message);
			else
				core::LogError("Build: " + result.message);
		}

		void Launch(PanelState& state, const PackageResult& result)
		{
			if (!result.ok) return;
			const std::string exe = result.exe.string();
			const char* const args[] = {exe.c_str(), nullptr};
			SDL_Process* process = SDL_CreateProcess(args, false);
			if (process == nullptr)
			{
				state.status = "Packaged, but the game did not start.";
				state.statusIsError = true;
				core::LogError("Build: could not start " + exe);
				return;
			}
			SDL_DestroyProcess(process); // the game runs on its own; the editor does not wait
		}

		void DrawBuildSettings(PanelState& state)
		{
			if (!state.loaded) Load(state);

			ImGui::TextUnformatted("Scenes");
			ImGui::SameLine();
			bool changed = false;
			if (ImGui::SmallButton("Rescan"))
			{
				Rescan(state);
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("All")) state.settings.scenes = state.available;
			ImGui::SameLine();
			if (ImGui::SmallButton("None"))
			{
				state.settings.scenes.clear();
				state.settings.startupScene.clear();
				changed = true;
			}
			if (ImGui::BeginChild("buildscenes", ImVec2(0.0F, ImGui::GetTextLineHeightWithSpacing() * 8.0F),
			                      ImGuiChildFlags_Borders))
			{
				for (const std::string& name : state.available)
				{
					ImGui::PushID(name.c_str());
					bool picked = Contains(state.settings.scenes, name);
					if (ImGui::Checkbox("##pick", &picked))
					{
						changed = true;
						if (picked)
						{
							state.settings.scenes.push_back(name);
							std::sort(state.settings.scenes.begin(), state.settings.scenes.end());
						}
						else
						{
							std::erase(state.settings.scenes, name);
							if (state.settings.startupScene == name) state.settings.startupScene.clear();
						}
					}
					ImGui::SameLine();
					const bool startup = state.settings.startupScene == name;
					if (ImGui::Selectable(startup ? (name + "  (startup)").c_str() : name.c_str(), startup) && picked)
					{
						state.settings.startupScene = name;
						changed = true;
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Click to make this the startup scene (it must be ticked)");
					ImGui::PopID();
				}
			}
			ImGui::EndChild();
			ImGui::TextDisabled(
			    "The game also loads scenes by name (title, pause...); leave out only ones it never opens.");

			changed |= ImGui::InputText("Game exe", &state.settings.gameExe);
			changed |= ImGui::InputText("Output folder", &state.settings.outputFolder);
			if (changed) Save(state);

			const double now = ImGui::GetTime();
			if (changed || state.problemsChecked < 0.0 || now - state.problemsChecked > 1.0)
			{
				state.problems = BuildProblems(state.settings, state.available);
				state.problemsChecked = now;
			}
			const std::vector<std::string>& problems = state.problems;
			for (const std::string& problem : problems)
				ImGui::TextColored(ImVec4(1.0F, 0.8F, 0.3F, 1.0F), "%s", problem.c_str());

			ImGui::BeginDisabled(!problems.empty());
			const bool package = ImGui::Button("Package");
			ImGui::SameLine();
			const bool packageAndRun = ImGui::Button("Package and run");
			ImGui::EndDisabled();
			if (package || packageAndRun)
			{
				Save(state);
				const PackageResult result = PackageGame(state.config, state.settings);
				Report(state, result);
				state.problemsChecked = -1.0;
				if (packageAndRun) Launch(state, result);
			}
			if (!state.status.empty())
			{
				const ImVec4 colour =
				    state.statusIsError ? ImVec4(1.0F, 0.4F, 0.4F, 1.0F) : ImVec4(0.5F, 0.9F, 0.5F, 1.0F);
				ImGui::TextColored(colour, "%s", state.status.c_str());
			}
		}
	} // namespace

	std::vector<std::string> ListSceneNames(const fs::path& folder)
	{
		std::vector<std::string> names;
		std::error_code error;
		for (fs::directory_iterator it(folder, error), end; !error && it != end; it.increment(error))
		{
			std::error_code entryError;
			if (it->is_regular_file(entryError) && it->path().extension() == ".json")
				names.push_back(it->path().stem().string());
		}
		std::sort(names.begin(), names.end());
		return names;
	}

	BuildSettings LoadBuildSettings(const fs::path& file)
	{
		BuildSettings settings;
		std::error_code error;
		if (!fs::exists(file, error)) return settings;
		const core::JsonResult parsed = core::LoadJson(file);
		if (!parsed.value || !parsed.value->IsObject())
		{
			core::LogWarning("Build settings " + file.string() + " could not be read; using the defaults.");
			return settings;
		}
		const core::JsonValue& root = *parsed.value;
		if (const core::JsonValue* scenes = root.Find("scenes"); scenes != nullptr && scenes->AsArray() != nullptr)
		{
			for (const core::JsonValue& item : *scenes->AsArray())
				if (const auto name = item.AsString()) settings.scenes.emplace_back(*name);
			std::sort(settings.scenes.begin(), settings.scenes.end());
		}
		const auto text = [&](const char* key) -> std::string
		{
			const core::JsonValue* value = root.Find(key);
			const auto string = value != nullptr ? value->AsString() : std::nullopt;
			return string ? std::string(*string) : std::string();
		};
		settings.startupScene = text("startupScene");
		settings.gameExe = text("gameExe");
		settings.outputFolder = text("outputFolder");
		return settings;
	}

	bool SaveBuildSettings(const fs::path& file, const BuildSettings& settings)
	{
		core::JsonValue root = core::JsonValue::Object();
		core::JsonValue scenes = core::JsonValue::Array();
		for (const std::string& name : settings.scenes)
			scenes.Append(name);
		root.Set("scenes", std::move(scenes));
		root.Set("startupScene", settings.startupScene);
		root.Set("gameExe", settings.gameExe);
		root.Set("outputFolder", settings.outputFolder);
		return core::WriteJson(file, root);
	}

	std::vector<std::string> BuildProblems(const BuildSettings& settings,
	                                       const std::vector<std::string>& availableScenes)
	{
		std::vector<std::string> problems;
		if (settings.scenes.empty()) problems.emplace_back("Tick at least one scene.");
		for (const std::string& name : settings.scenes)
			if (!Contains(availableScenes, name)) problems.push_back("Scene " + name + " is not in the scenes folder.");
		if (!settings.startupScene.empty() && !Contains(settings.scenes, settings.startupScene))
			problems.push_back("The startup scene " + settings.startupScene + " is not ticked.");
		if (settings.gameExe.empty())
			problems.emplace_back("Give the game executable.");
		else
		{
			std::error_code error;
			if (!fs::is_regular_file(settings.gameExe, error)) problems.push_back("No such file: " + settings.gameExe);
		}
		if (settings.outputFolder.empty()) problems.emplace_back("Give an output folder.");
		return problems;
	}

	core::JsonValue BuildManifest(const BuildSettingsConfig& config, const BuildSettings& settings,
	                              const std::vector<std::string>& files)
	{
		core::JsonValue root = core::JsonValue::Object();
		root.Set("game", config.gameName);
		root.Set("engineVersion", config.engineVersion);
		root.Set("startupScene", settings.startupScene);
		core::JsonValue scenes = core::JsonValue::Array();
		for (const std::string& name : settings.scenes)
			scenes.Append(name);
		root.Set("scenes", std::move(scenes));
		core::JsonValue list = core::JsonValue::Array();
		for (const std::string& file : files)
			list.Append(file);
		root.Set("files", std::move(list));
		return root;
	}

	PackageResult PackageGame(const BuildSettingsConfig& config, const BuildSettings& settings)
	{
		PackageResult result;
		const auto fail = [&](std::string message)
		{
			result.ok = false;
			result.message = std::move(message);
			return result;
		};

		const std::vector<std::string> problems = BuildProblems(settings, ListSceneNames(config.sceneFolder));
		if (!problems.empty()) return fail(problems.front());

		const fs::path output = settings.outputFolder;
		const fs::path exeSource = settings.gameExe;
		const fs::path packageAssets = output / "assets";
		const fs::path exeTarget = output / exeSource.filename();
		if (IsInside(packageAssets, config.assetFolder) || IsInside(config.assetFolder, packageAssets))
			return fail("The output folder would overwrite the game's own assets. Pick another folder.");
		if (Canonical(exeTarget) == Canonical(exeSource))
			return fail("The output folder is the executable's own folder. Pick another folder.");

		std::error_code error;
		fs::create_directories(output, error);
		if (error) return fail("Could not make " + output.string() + ": " + error.message());
		fs::remove_all(packageAssets, error);
		if (error) return fail("Could not clear " + packageAssets.string() + ": " + error.message());

		std::vector<std::string> files;
		const std::vector<std::string> sceneFiles = SelectedSceneFiles(settings);
		for (fs::recursive_directory_iterator it(config.assetFolder, error), end; !error && it != end;
		     it.increment(error))
		{
			std::error_code entryError;
			if (!it->is_regular_file(entryError)) continue;
			const fs::path relative = it->path().lexically_relative(config.assetFolder);
			if (IsPackagedScene(relative) && !Contains(sceneFiles, relative.filename().generic_string())) continue;
			const fs::path target = packageAssets / relative;
			fs::create_directories(target.parent_path(), entryError);
			if (entryError)
				return fail("Could not make " + target.parent_path().string() + ": " + entryError.message());
			fs::copy_file(it->path(), target, fs::copy_options::overwrite_existing, entryError);
			if (entryError) return fail("Could not copy " + relative.generic_string() + ": " + entryError.message());
			files.push_back("assets/" + relative.generic_string());
		}
		if (error) return fail("Could not read " + config.assetFolder.string() + ": " + error.message());

		fs::copy_file(exeSource, exeTarget, fs::copy_options::overwrite_existing, error);
		if (error) return fail("Could not copy " + exeSource.string() + ": " + error.message());
		files.push_back(exeSource.filename().generic_string());
		std::sort(files.begin(), files.end());

		std::vector<std::string> manifestFiles = files;
		manifestFiles.emplace_back(kManifestName);
		std::sort(manifestFiles.begin(), manifestFiles.end());
		if (!core::WriteJson(output / kManifestName, BuildManifest(config, settings, manifestFiles)))
			return fail(std::string("Could not write ") + kManifestName);

		result.ok = true;
		result.files = manifestFiles.size();
		result.exe = exeTarget;
		result.message = "Packaged " + std::to_string(result.files) + " files to " + output.string();
		return result;
	}

	void AddBuildSettingsPanel(Editor& editor, BuildSettingsConfig config)
	{
		auto state = std::make_shared<PanelState>();
		state->config = std::move(config);
		editor.AddPanel("Build settings", [state]
		{
			DrawBuildSettings(*state);
		});
		editor.SetPanelSize("Build settings", 460.0F, 420.0F);
	}
} // namespace engine::editor
