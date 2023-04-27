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
#include <cstdlib>
#include <fmt/format.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <imgui_stdlib.h>
#include <thread>

#define GL_SILENCE_DEPRECATION
#include <GL/glew.h>
#include <GLFW/glfw3.h>

SHADERTOY_NAMESPACE_BEGIN

[[noreturn]] void reportError(std::string_view error) {
    fmt::print(stderr, "{}\n", error);
    std::exit(EXIT_FAILURE);
}

static void drawCanvas(ShaderToyContext& ctx) {
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
    if(ImGui::Button("reset")) {
        ctx.reset();
    }
    ImGui::SameLine();

    if(ctx.isRunning()) {
        if(ImGui::Button("pause")) {
            ctx.pause();
        }
    } else {
        if(ImGui::Button("resume")) {
            ctx.resume();
        }
    }

    // TODO: record
    ImGui::SameLine();
    ImGui::Text("% 6.2f % 9.2f fps % 4d x% 4d", static_cast<double>(ctx.getTime()), static_cast<double>(ImGui::GetIO().Framerate),
                static_cast<int>(size.x), static_cast<int>(size.y));
    ImGui::End();
}

static void drawShaderEditor(ShaderToyContext& ctx) {
    if(!ImGui::Begin("Editor", nullptr)) {
        ImGui::End();
        return;
    }
    const auto reservedHeight = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
    static std::string src;
    ImGui::SetNextItemWidth(-ImGui::GetStyle().ItemSpacing.x);
    ImGui::InputTextMultiline("##ShaderSource", &src, ImVec2(0, -reservedHeight), ImGuiInputTextFlags_AllowTabInput);
    ImGui::Separator();
    if(ImGui::Button("compile")) {
        ctx.compile(src);
    }
    ImGui::End();
}

static void drawOutputs() {
    if(!ImGui::Begin("Outputs", nullptr)) {
        ImGui::End();
        return;
    }
    ImGui::End();
}

static void drawMain(ShaderToyContext& ctx) {
    if(ImGui::BeginMainMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("New Shader")) {
            }
            if(ImGui::MenuItem("Open Shader")) {
            }
            if(ImGui::MenuItem("Save Shader")) {
            }
            if(ImGui::MenuItem("Save Shader As")) {
            }
            if(ImGui::MenuItem("Import From shadertoy.com")) {
            }
            ImGui::Separator();
            if(ImGui::MenuItem("Exit")) {
                std::exit(EXIT_SUCCESS);
            }
            ImGui::EndMenu();
        }
        if(ImGui::BeginMenu("Help")) {
            if(ImGui::MenuItem("About")) {
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    drawCanvas(ctx);
    drawShaderEditor(ctx);
    drawOutputs();
}

int shaderToyMain(int argc, char** argv) {
    glfwSetErrorCallback(
        [](const int id, const char* error) { reportError(fmt::format("GLFW: (error code {}) {}", id, error)); });
    if(!glfwInit())
        reportError("Failed to initialize glfw");

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_DEPTH_BITS, GL_FALSE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#ifdef SHADERTOY_MACOS
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#endif
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_SAMPLES, 8);

    constexpr int32_t windowWidth = 1024, windowHeight = 768;
    GLFWwindow* const window = glfwCreateWindow(windowWidth, windowHeight, "ShaderToy live viewer", nullptr, nullptr);

    glfwMakeContextCurrent(window);

    if(glewInit() != GLEW_OK)
        reportError("Failed to initialize glew");

    /*
    int flags;
    glGetIntegerv(GL_CONTEXT_FLAGS, &flags);
    if(flags & GL_CONTEXT_FLAG_DEBUG_BIT) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugOutputCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
    }
    */

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    glfwSwapInterval(0);

    ShaderToyContext ctx;

    while(!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        ctx.tick(width, height);
        drawMain(ctx);

        ImGui::Render();
        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

SHADERTOY_NAMESPACE_END

int main(int argc, char** argv) {
    return ShaderToy::shaderToyMain(argc, argv);
}
