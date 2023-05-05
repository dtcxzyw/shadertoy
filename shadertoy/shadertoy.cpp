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
#include "shadertoy/NodeEditor/PipelineEditor.hpp"
#include "shadertoy/ShaderToyContext.hpp"
#include <cstdlib>
#include <fmt/format.h>
#include <hello_imgui/hello_imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.h>

#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>

SHADERTOY_NAMESPACE_BEGIN

namespace ed = ax::NodeEditor;

[[noreturn]] void reportFatalError(std::string_view error) {
    // TODO: pop up a message box
    fmt::print(stderr, "{}\n", error);
    std::exit(EXIT_FAILURE);
}

[[noreturn]] void reportNotImplemented() {
    reportFatalError("Not implemented feature");
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
        // See also https://shadertoyunofficial.wordpress.com/2016/07/20/special-shadertoy-features/
        if(ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            const auto pos = ImGui::GetMousePos();
            mouse = ImVec4(pos.x - base.x, size.y - (pos.y - base.y), 1.0f,
                           ImGui::IsMouseClicked(ImGuiMouseButton_Left) ? 1.0f : -1.0f);
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

static std::string url;
static bool startImport = false;

static void showMenu() {
    if(ImGui::BeginMenu("File")) {
        if(ImGui::MenuItem("New Shader")) {
            // TODO: reset
        }
        if(ImGui::MenuItem("Open Shader")) {
            nfdchar_t* path;
            if(NFD_OpenDialog("sttf", nullptr, &path) == NFD_OKAY) {
                PipelineEditor::get().loadSTTF(path);
            }
        }
        if(ImGui::MenuItem("Save Shader")) {
            nfdchar_t* path;
            if(NFD_SaveDialog("sttf", nullptr, &path) == NFD_OKAY) {
                PipelineEditor::get().saveSTTF(path);
            }
        }
        if(ImGui::MenuItem("Import From shadertoy.com")) {
            startImport = true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit")) {
            HelloImGui::GetRunnerParams()->appShallExit = true;
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Help")) {
        if(ImGui::MenuItem("About")) {
            // TODO: about model
        }
        ImGui::EndMenu();
    }
}
void showImportModal() {
    if(startImport) {
        ImGui::OpenPopup("Import Shader");
        const std::string_view clipboardText = ImGui::GetClipboardText();
        if(clipboardText.starts_with("https://www.shadertoy.com/view/")) {
            url = clipboardText;
        }
        startImport = false;
    }
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopupModal("Import Shader", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(ICON_FA_LINK "URL");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("https://www.shadertoy.com/view/WWWWWWXXXX").x);
        ImGui::InputText("##Url", &url, ImGuiInputTextFlags_CharsNoBlank);

        if(ImGui::Button("Import", ImVec2(120, 0))) {
            PipelineEditor::get().loadFromShaderToy(url);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
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
    runnerParams.callbacks.ShowMenus = [] { showMenu(); };

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
            reportFatalError("Fai led to initialize glew");

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
    editorWindow.GuiFunction = [&] {
        PipelineEditor::get().render(ctx);
        showImportModal();
    };
    runnerParams.dockingParams.dockableWindows = { canvasWindow, outputWindow, editorWindow };

    HelloImGui::Run(runnerParams);
    return 0;
}

SHADERTOY_NAMESPACE_END

int main(int argc, char** argv) {
    return ShaderToy::shaderToyMain(argc, argv);
}
