/*
    SPDX-License-Identifier: Apache-2.0
    Copyright 2023 Yingwei Zheng
    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at
        http://www.apache.org/licenses/LICENSE-2.0
    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "shadertoy/Config.hpp"
#include "shadertoy/ShaderToyContext.hpp"
#include <ImGuiColorTextEdit/TextEditor.h>
#include <cstdlib>
#include <fmt/format.h>
#include <hello_imgui/hello_imgui.h>
#include <nfd.h>

#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>

SHADERTOY_NAMESPACE_BEGIN

[[noreturn]] void reportFatalError(std::string_view error) {
    // TODO: pop up a message box
    fmt::print(stderr, "{}\n", error);
    std::exit(EXIT_FAILURE);
}

static void showCanvas(ShaderToyContext& ctx) {
    if(!ImGui::Begin("Canvas", nullptr)) {
        ImGui::End();
        return;
    }

    const auto reservedHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    ImVec2 size(0, 0);
    if(ImGui::BeginChild("CanvasRegion", ImVec2(0, -reservedHeight), false)) {
        size = ImGui::GetContentRegionAvail();
        const auto base = ImGui::GetCursorScreenPos();
        std::optional<ImVec4> mouse = std::nullopt;
        ImGui::InvisibleButton("CanvasArea", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        if(ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const auto pos = ImGui::GetMousePos();
            mouse = ImVec4(pos.x - base.x, size.y - (pos.y - base.y), 1.0f, 1.0f);
        }
        ctx.render(base, size, mouse);
        ImGui::EndChild();
    }

    ImGui::Separator();
    if(ImGui::Button(ICON_FA_STEP_BACKWARD)) {
        ctx.reset();
    }
    ImGui::SameLine();

    if(ctx.isRunning()) {
        if(ImGui::Button(ICON_FA_PAUSE)) {
            HelloImGui::GetRunnerParams()->fpsIdling.enableIdling = true;
            ctx.pause();
        }
    } else {
        if(ImGui::Button(ICON_FA_PLAY)) {
            HelloImGui::GetRunnerParams()->fpsIdling.enableIdling = false;
            ctx.resume();
        }
    }

    // TODO: record
    ImGui::SameLine();
    ImGui::Text("% 6.2f % 9.2f fps % 4d x% 4d", static_cast<double>(ctx.getTime()), static_cast<double>(ImGui::GetIO().Framerate),
                static_cast<int>(size.x), static_cast<int>(size.y));
    ImGui::End();
}

class ShaderToyEditor final {
    TextEditor mEditor;

public:
    ShaderToyEditor() {
        const auto lang = TextEditor::LanguageDefinition::GLSL();
        mEditor.SetLanguageDefinition(lang);
        mEditor.SetTabSize(4);
        mEditor.SetShowWhitespaces(false);
    }

    [[nodiscard]] std::string getText() const {
        return mEditor.GetText();
    }

    void render(const ImVec2 size) {
        const auto cpos = mEditor.GetCursorPosition();
        ImGui::Text("%6d/%-6d %6d lines  %s", cpos.mLine + 1, cpos.mColumn + 1, mEditor.GetTotalLines(),
                    mEditor.IsOverwrite() ? "Ovr" : "Ins");
        mEditor.Render("TextEditor", size, false);
    }
};

static ShaderToyEditor editor;

static void showShaderEditor(ShaderToyContext& ctx) {
    if(!ImGui::Begin("Editor", nullptr)) {
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-ImGui::GetStyle().ItemSpacing.x);
    const auto reservedHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    editor.render(ImVec2(0, -reservedHeight));
    // ImGui::InputTextMultiline("##ShaderSource", &src, , ImGuiInputTextFlags_AllowTabInput);
    ImGui::Separator();
    if(ImGui::Button("compile")) {
        ctx.compile(editor.getText());  // TODO: error marker
    }
    ImGui::End();
}

static void showMenu(ShaderToyContext& ctx) {
    if(ImGui::BeginMenu("File")) {
        if(ImGui::MenuItem("New Shader")) {
            // src = "";
        }
        if(ImGui::MenuItem("Open Shader")) {
            nfdchar_t* path;
            if(NFD_OpenDialog("glsl,frag,fsh", nullptr, &path) == NFD_OKAY) {
                fmt::print(stderr, "{}", path);
            }
        }
        if(ImGui::MenuItem("Save Shader")) {
            nfdchar_t* path;
            if(NFD_SaveDialog("sttf", nullptr, &path) == NFD_OKAY) {
                fmt::print(stderr, "{}", path);
            }
        }
        if(ImGui::MenuItem("Import From shadertoy.com")) {
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit")) {
            HelloImGui::GetRunnerParams()->appShallExit = true;
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Help")) {
        if(ImGui::MenuItem("About")) {
        }
        ImGui::EndMenu();
    }
}

int shaderToyMain(int argc, char** argv) {
    ShaderToyContext ctx;
    HelloImGui::RunnerParams runnerParams;
    runnerParams.appWindowParams.windowTitle = "ShaderToy live viewer";
    runnerParams.appWindowParams.restorePreviousGeometry = true;
    runnerParams.fpsIdling.enableIdling = false;

    runnerParams.imGuiWindowParams.showStatusBar = true;
    runnerParams.imGuiWindowParams.showStatus_Fps = true;
    runnerParams.callbacks.ShowStatus = [&] {};

    runnerParams.imGuiWindowParams.showMenuBar = true;
    runnerParams.imGuiWindowParams.showMenu_App_Quit = false;
    runnerParams.callbacks.ShowMenus = [&] { showMenu(ctx); };

    runnerParams.callbacks.LoadAdditionalFonts = [] { HelloImGui::ImGuiDefaultSettings::LoadDefaultFont_WithFontAwesomeIcons(); };

    runnerParams.imGuiWindowParams.defaultImGuiWindowType = HelloImGui::DefaultImGuiWindowType::ProvideFullScreenDockSpace;
    runnerParams.imGuiWindowParams.enableViewports = true;

    HelloImGui::DockingSplit splitMainBottom;
    splitMainBottom.initialDock = "MainDockSpace";
    splitMainBottom.newDock = "BottomSpace";
    splitMainBottom.direction = ImGuiDir_Down;
    splitMainBottom.ratio = 0.25f;

    HelloImGui::DockingSplit splitMainLeft;
    splitMainLeft.initialDock = "MainDockSpace";
    splitMainLeft.newDock = "LeftSpace";
    splitMainLeft.direction = ImGuiDir_Left;
    splitMainLeft.ratio = 0.75f;

    runnerParams.dockingParams.dockingSplits = { splitMainBottom, splitMainLeft };

    HelloImGui::DockableWindow canvasWindow;
    canvasWindow.label = "Canvas";
    canvasWindow.dockSpaceName = "LeftSpace";
    canvasWindow.GuiFunction = [&ctx] {
        if(!__glewCreateProgram && glewInit() != GLEW_OK)
            reportFatalError("Failed to initialize glew");

        ctx.tick();
        showCanvas(ctx);
    };
    HelloImGui::DockableWindow outputWindow;
    outputWindow.label = "Output";
    outputWindow.dockSpaceName = "BottomSpace";
    outputWindow.GuiFunction = [] { HelloImGui::LogGui(); };
    HelloImGui::DockableWindow editorWindow;
    editorWindow.label = "Editor";
    editorWindow.dockSpaceName = "MainDockSpace";
    editorWindow.GuiFunction = [&] { showShaderEditor(ctx); };
    runnerParams.dockingParams.dockableWindows = { canvasWindow, outputWindow, editorWindow };

    HelloImGui::Run(runnerParams);
    return 0;
}

SHADERTOY_NAMESPACE_END

int main(int argc, char** argv) {
    return ShaderToy::shaderToyMain(argc, argv);
}
