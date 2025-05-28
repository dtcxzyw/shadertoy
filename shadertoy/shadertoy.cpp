/*
    SPDX-License-Identifier: Apache-2.0
    Copyright 2023-2025 Yingwei Zheng
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

#include "shadertoy/SuppressWarningPush.hpp"

#include <fmt/format.h>
#include <hello_imgui/dpi_aware.h>
#include <hello_imgui/hello_imgui.h>
#include <hello_imgui/hello_imgui_screenshot.h>
#include <httplib.h>
#include <magic_enum.hpp>
#include <misc/cpp/imgui_stdlib.h>
#include <nfd.h>
#include <nlohmann/json.hpp>
#include <openssl/crypto.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#define GL_SILENCE_DEPRECATION  // NOLINT(clang-diagnostic-unused-macros)
#include <GL/glew.h>

#include <GLFW/glfw3.h>
#ifdef SHADERTOY_WINDOWS
#define NOMINMAX  // NOLINT(clang-diagnostic-unused-macros)
#include <Windows.h>
#endif

using HelloImGui::EmToVec2;

#include "shadertoy/SuppressWarningPop.hpp"

SHADERTOY_NAMESPACE_BEGIN

[[noreturn]] void reportFatalError(std::string_view error) {
    // TODO: pop up a message box
    fmt::print(stderr, "{}\n", error);
    std::abort();
}

[[noreturn]] void reportNotImplemented() {
    reportFatalError("Not implemented feature");
}

static void openURL(const std::string& url) {
#if defined(SHADERTOY_WINDOWS)
    ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
    const auto ret = std::system(("open " + url).c_str());
    SHADERTOY_UNUSED(ret);
#endif
}

static std::optional<std::function<void()>> takeScreenshot;
static bool endsWith(const std::string_view& str, const std::string_view& pattern) {
    return str.size() >= pattern.size() && str.substr(str.size() - pattern.size()) == pattern;
}
static bool startsWith(const std::string_view& str, const std::string_view& pattern) {
    return str.size() >= pattern.size() && str.substr(0, pattern.size()) == pattern;
}
static void saveScreenshot(const ImVec4& bound) {
    const auto [width, height, bufferRgb] = HelloImGui::AppWindowScreenshotRgbBuffer();
    if(bufferRgb.empty()) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Failed to get screenshot since it is not supported by the backend");
        return;
    }

    std::vector<uint8_t> img;
    const auto bx = std::max(static_cast<int32_t>(bound.x), 0), by = std::max(static_cast<int32_t>(bound.y), 0),
               ex = std::min(static_cast<int32_t>(bound.z), static_cast<int32_t>(width)),
               ey = std::min(static_cast<int32_t>(bound.w), static_cast<int32_t>(height));
    if(bx >= ex || by >= ey)
        return;

    const auto w = ex - bx;
    const auto h = ey - by;
    img.resize(static_cast<size_t>(w * h) * 3);
    for(auto y = by; y < ey; ++y) {
        std::copy(bufferRgb.begin() + static_cast<ptrdiff_t>((static_cast<size_t>(y) * width + bx) * 3),
                  bufferRgb.begin() + static_cast<ptrdiff_t>((static_cast<size_t>(y) * width + bx + w) * 3),
                  img.begin() + static_cast<ptrdiff_t>(static_cast<size_t>(y - by) * w * 3));
    }

    nfdchar_t* path;
    if(NFD_SaveDialog("png,jpg,bmp,tga", nullptr, &path) != NFD_OKAY)
        return;

    auto handleStbError = [](const int ret) {
        if(ret == 0) {
            HelloImGui::Log(HelloImGui::LogLevel::Error, "Failed to save the screenshot");
        }
    };

    const std::string_view imgPath = path;
    const auto data = img.data();
    const auto stride = w * 3;
    if(endsWith(imgPath, ".png")) {
        handleStbError(stbi_write_png(path, w, h, 3, data, stride));
    } else if(endsWith(imgPath, ".jpg")) {
        handleStbError(stbi_write_jpg(path, w, h, 3, data, 90));
    } else if(endsWith(imgPath, ".bmp")) {
        handleStbError(stbi_write_bmp(path, w, h, 3, data));
    } else if(endsWith(imgPath, ".tga")) {
        handleStbError(stbi_write_tga(path, w, h, 3, data));
    } else {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Unrecognized image format");
    }
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

    const auto mouse = ctx.getMouseStatus();
    ImGui::SameLine();
    ImGui::Text("% 6.2f % 9.2f fps % 4d x% 4d [%d %d %d %d]", static_cast<double>(ctx.getTime()),
                static_cast<double>(ImGui::GetIO().Framerate), static_cast<int>(size.x), static_cast<int>(size.y),
                static_cast<int>(mouse.x), static_cast<int>(mouse.y), static_cast<int>(mouse.z), static_cast<int>(mouse.w));
    ImGui::SameLine();
    if(ImGui::Button(ICON_FA_CAMERA)) {
        takeScreenshot = [&ctx] { saveScreenshot(ctx.getBound()); };
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("timescale (log2)", &ctx.getTimeScale(), 0.01f, -16.0f, 16.0f, "%.1f");
    ImGui::End();
}

static std::string url;
static bool openImportModal = false, openAboutModal = false;

static void showMenu() {
    if(ImGui::BeginMenu("File")) {
        auto& editor = PipelineEditor::get();
        if(ImGui::MenuItem("New shader")) {
            editor.resetPipeline();
        }
        if(ImGui::MenuItem("Open shader")) {
            nfdchar_t* path;
            if(NFD_OpenDialog("sttf", nullptr, &path) == NFD_OKAY) {
                editor.loadSTTF(path);
            }
        }
        if(ImGui::MenuItem("Save shader")) {
            // const auto defaultPath = editor.getShaderName() + ".sttf";
            nfdchar_t* path;
            if(NFD_SaveDialog("sttf", nullptr, &path) == NFD_OKAY) {
                editor.saveSTTF(path);
            }
        }
        if(ImGui::MenuItem("Import from shadertoy.com")) {
            openImportModal = true;
        }
        ImGui::Separator();
        if(ImGui::MenuItem("Exit")) {
            HelloImGui::GetRunnerParams()->appShallExit = true;
        }
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Help")) {
        if(ImGui::MenuItem("About")) {
            openAboutModal = true;
        }
        ImGui::EndMenu();
    }
}
static void showImportModal() {
    if(openImportModal) {
        ImGui::OpenPopup("Import Shader");
        if(const auto text = ImGui::GetClipboardText()) {
            const std::string_view clipboardText = text;
            if(startsWith(clipboardText, "https://www.shadertoy.com/view/")) {
                url = clipboardText;
            }
        }
        openImportModal = false;
    }
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopupModal("Import Shader", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(ICON_FA_LINK "URL");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::CalcTextSize("https://www.shadertoy.com/view/WWWWWWXXXX").x);
        ImGui::InputText("##Url", &url, ImGuiInputTextFlags_CharsNoBlank);

        if(ImGui::Button("Import", EmToVec2(5, 0))) {
            try {
                PipelineEditor::get().loadFromShaderToy(url);
            } catch(const std::exception&) {
                Log(HelloImGui::LogLevel::Error, "Failed to import %s", url.c_str());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", EmToVec2(5, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
static void showAboutModal() {
    if(openAboutModal) {
        ImGui::OpenPopup("About Shadertoy live viewer");
        openAboutModal = false;
    }
    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if(ImGui::BeginPopupModal("About Shadertoy live viewer", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Unofficial Shadertoy live viewer " SHADERTOY_VERSION);
        ImGui::Separator();
        ImGui::TextUnformatted("Copyright 2023-2025 Yingwei Zheng");
        ImGui::TextUnformatted("Licensed under the Apache License, Version 2.0");
        ImGui::TextUnformatted("Build Time: " __DATE__ " " __TIME__);

        if(ImGui::Button(ICON_FA_LINK " " SHADERTOY_URL)) {
            openURL(SHADERTOY_URL);
        }

        if(ImGui::CollapsingHeader("Config", ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& io = ImGui::GetIO();
            ImGui::Text("Dear ImGui %s (%d)", IMGUI_VERSION, IMGUI_VERSION_NUM);
            ImGui::Text("Platform: %s", io.BackendPlatformName ? io.BackendPlatformName : "Unknown");
            ImGui::Text("Renderer: %s", io.BackendRendererName ? io.BackendRendererName : "Unknown");
#if HELLOIMGUI_HAS_OPENGL
            ImGui::Text("OpenGL version: %s", glGetString(GL_VERSION));
            ImGui::Text("OpenGL vendor: %s", glGetString(GL_VENDOR));
            ImGui::Text("Graphics device: %s", glGetString(GL_RENDERER));
#endif
            ImGui::TextUnformatted("ImGui Node Editor " IMGUI_NODE_EDITOR_VERSION);
            ImGui::Text("GLFW3 %s", glfwGetVersionString());
            // ReSharper disable once CppEqualOperandsInBinaryExpression
            ImGui::Text("fmt %d.%d.%d", FMT_VERSION / 10000, (FMT_VERSION % 10000) / 100, FMT_VERSION % 100);
            ImGui::TextUnformatted("cpp-httplib " CPPHTTPLIB_VERSION);
            ImGui::Text("magic_enum %d.%d.%d", MAGIC_ENUM_VERSION_MAJOR, MAGIC_ENUM_VERSION_MINOR, MAGIC_ENUM_VERSION_PATCH);
            ImGui::Text("nlohmann-json %d.%d.%d", NLOHMANN_JSON_VERSION_MAJOR, NLOHMANN_JSON_VERSION_MINOR,
                        NLOHMANN_JSON_VERSION_PATCH);
            ImGui::TextUnformatted(OpenSSL_version(OPENSSL_VERSION));
        }

        if(ImGui::Button("Close", EmToVec2(5, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

int shaderToyMain(int argc, char** argv) {
    std::string initialPipeline;
    if(argc == 2) {
        initialPipeline = argv[1];
    }

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
    runnerParams.callbacks.ShowGui = [] {
        showImportModal();
        showAboutModal();
    };
    runnerParams.callbacks.PreNewFrame = [] {
        if(takeScreenshot.has_value()) {
            (*takeScreenshot)();
            takeScreenshot.reset();
        }
    };

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
    canvasWindow.GuiFunction = [&] {
        if(!initialPipeline.empty()) {
            if(startsWith(initialPipeline, "https://")) {
                PipelineEditor::get().loadFromShaderToy(initialPipeline);
            } else if(endsWith(initialPipeline, ".sttf")) {
                PipelineEditor::get().loadSTTF(initialPipeline);
            } else {
                HelloImGui::Log(HelloImGui::LogLevel::Error, "Unrecognized filepath %s", initialPipeline.c_str());
            }

            initialPipeline.clear();
        }

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
    editorWindow.GuiFunction = [&] { PipelineEditor::get().render(ctx); };
    runnerParams.dockingParams.dockableWindows = { canvasWindow, outputWindow, editorWindow };

    // 8x MSAA
    runnerParams.callbacks.PostInit = [] {
        if(glewInit() != GLEW_OK)
            reportFatalError("Failed to initialize glew");
    };
    HelloImGui::Run(runnerParams);
    return 0;
}

SHADERTOY_NAMESPACE_END

int main(const int argc, char** argv) {
    return ShaderToy::shaderToyMain(argc, argv);
}
