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

#ifndef CPPHTTPLIB_OPENSSL_SUPPORT
#define CPPHTTPLIB_OPENSSL_SUPPORT
#endif

#define IMGUI_DEFINE_MATH_OPERATORS
#include "shadertoy/NodeEditor/PipelineEditor.hpp"
#include <queue>

#include "shadertoy/SuppressWarningPush.hpp"

#include <fmt/format.h>
#include <hello_imgui/dpi_aware.h>
#include <hello_imgui/hello_imgui.h>
#include <hello_imgui/image_from_asset.h>
#include <httplib.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <magic_enum.hpp>
#include <nfd.h>
#include <nlohmann/json.hpp>
#include <stb_image.h>

using HelloImGui::EmToVec2;

#include "shadertoy/SuppressWarningPop.hpp"

SHADERTOY_NAMESPACE_BEGIN

ShaderToyEditor::ShaderToyEditor() {
    const auto lang = TextEditor::LanguageDefinition::GLSL();
    // TODO: more keywords/built-ins
    mEditor.SetLanguageDefinition(lang);
    mEditor.SetTabSize(4);
    mEditor.SetShowWhitespaces(false);
    mEditor.SetText(R"(void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    fragColor = vec4(0.0,0.0,1.0,1.0);
})");
}

[[nodiscard]] std::string ShaderToyEditor::getText() const {
    return mEditor.GetText();
}

void ShaderToyEditor::setText(const std::string& str) {
    mEditor.SetText(str);
}

void ShaderToyEditor::render(const ImVec2 size) {
    const auto cpos = mEditor.GetCursorPosition();
    ImGui::Text("%6d/%-6d %6d lines  %s", cpos.mLine + 1, cpos.mColumn + 1, mEditor.GetTotalLines(),
                mEditor.IsOverwrite() ? "Ovr" : "Ins");
    mEditor.Render("TextEditor", size, false);
}

static constexpr auto initialShader = R"(void mainImage( out vec4 fragColor, in vec2 fragCoord )
{
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord/iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5*cos(iTime+uv.xyx+vec3(0,2,4));

    // Output to screen
    fragColor = vec4(col,1.0);
}
)";

uint32_t PipelineEditor::nextId() {
    return mNextId++;
}

void PipelineEditor::setupInitialPipeline() {
    auto& shader = spawnShader(NodeType::Image);
    shader.editor.setText(initialShader);
    auto& sink = spawnRenderOutput();

    mLinks.emplace_back(nextId(), shader.outputs.front().id, sink.inputs.front().id);
}

PipelineEditor::PipelineEditor() {
    const ed::Config config;
    mCtx = ed::CreateEditor(&config);
    mHeaderBackground = HelloImGui::ImTextureIdFromAsset("BlueprintBackground.png");

    setupInitialPipeline();
    mShouldBuildPipeline = true;
    mShouldResetLayout = true;
}
void PipelineEditor::resetPipeline() {
    mNodes.clear();
    mLinks.clear();
    mMetadata.clear();
    setupInitialPipeline();
    mShouldBuildPipeline = true;
    mShouldResetLayout = true;
}

PipelineEditor::~PipelineEditor() {
    ed::DestroyEditor(mCtx);
}

void PipelineEditor::resetLayout() {
    std::unordered_map<EditorNode*, std::vector<std::pair<EditorNode*, uint32_t>>> graph;
    std::unordered_map<EditorNode*, uint32_t> degree;
    for(auto& link : mLinks) {
        auto u = findPin(link.startPinId);
        auto v = findPin(link.endPinId);
        auto idx = static_cast<uint32_t>(v - v->node->inputs.data());
        graph[v->node].emplace_back(u->node, idx);
        ++degree[u->node];
    }

    std::queue<EditorNode*> q;
    std::unordered_map<EditorNode*, uint32_t> depth;
    for(auto& node : mNodes)
        if(!degree.count(node.get()))
            q.push(node.get());
    while(!q.empty()) {
        auto u = q.front();
        q.pop();
        for(auto [v, idx] : graph[u]) {
            depth[v] = std::max(depth[v], depth[u] + 1);
            if(--degree[v] == 0) {
                q.push(v);
            }
        }
    }

    std::map<uint32_t, std::vector<EditorNode*>> layers;
    std::unordered_map<const EditorNode*, std::pair<double, uint32_t>> barycenter;
    for(auto [u, d] : depth)
        layers[d].push_back(u);

    float selfX = 0;
    for(auto& [d, layer] : layers) {
        constexpr auto width = 500.0f;
        auto getBarycenter = [&](const EditorNode* u) {
            if(const auto iter = barycenter.find(u); iter != barycenter.cend()) {
                return iter->second.first / iter->second.second;
            }
            return 0.0;
        };
        std::sort(layer.begin(), layer.end(),
                  [&](const EditorNode* u, const EditorNode* v) { return getBarycenter(u) < getBarycenter(v); });
        double pos = 0;
        float selfY = 0;
        for(auto u : layer) {
            constexpr auto height = 300.0f;
            ++pos;
            for(auto [v, idx] : graph[u]) {
                auto& [sum, count] = barycenter[v];
                sum += pos + idx;
                ++count;
            }
            pos += static_cast<double>(u->inputs.size());

            ed::SetNodePosition(u->id, ImVec2{ selfX, selfY });
            selfY += height;
        }
        selfX -= width;
    }

    mShouldZoomToContent = true;
}

bool PipelineEditor::isUniqueName(const std::string_view& name, const EditorNode* exclude) const {
    return std::all_of(mNodes.cbegin(), mNodes.cend(), [&](auto& node) { return node.get() == exclude || node->name != name; });
}
std::string PipelineEditor::generateUniqueName(const std::string_view& base) const {
    if(isUniqueName(base, nullptr))
        return { base.data(), base.size() };
    for(uint32_t idx = 1;; ++idx) {
        if(auto str = fmt::format("{}{}", base, idx); isUniqueName(str, nullptr)) {
            return str;
        }
    }
}

template <typename T>
static auto& buildNode(std::vector<std::unique_ptr<EditorNode>>& nodes, std::unique_ptr<T> node) {
    for(auto& input : node->inputs) {
        input.node = node.get();
        input.kind = PinKind::Input;
    }

    for(auto& output : node->outputs) {
        output.node = node.get();
        output.kind = PinKind::Output;
    }
    auto& ref = *node;
    nodes.push_back(std::move(node));
    return ref;
}
EditorTexture& PipelineEditor::spawnTexture() {
    auto ret = std::make_unique<EditorTexture>(nextId(), generateUniqueName("Texture"));
    ret->outputs.emplace_back(nextId(), "Output", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorCubeMap& PipelineEditor::spawnCubeMap() {
    auto ret = std::make_unique<EditorCubeMap>(nextId(), generateUniqueName("CubeMap"));
    ret->type = NodeType::CubeMap;
    ret->outputs.emplace_back(nextId(), "Output", NodeType::CubeMap);
    return buildNode(mNodes, std::move(ret));
}
EditorKeyboard& PipelineEditor::spawnKeyboard() {
    auto ret = std::make_unique<EditorKeyboard>(nextId(), generateUniqueName("Keyboard"));
    ret->outputs.emplace_back(nextId(), "Output", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorRenderOutput& PipelineEditor::spawnRenderOutput() {
    auto ret = std::make_unique<EditorRenderOutput>(nextId(), generateUniqueName("RenderOutput"));
    ret->inputs.emplace_back(nextId(), "Input", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorLastFrame& PipelineEditor::spawnLastFrame() {
    auto ret = std::make_unique<EditorLastFrame>(nextId(), generateUniqueName("LastFrame"));
    ret->outputs.emplace_back(nextId(), "Output", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorShader& PipelineEditor::spawnShader(NodeType type) {
    auto ret = std::make_unique<EditorShader>(nextId(), generateUniqueName("Shader"));
    ret->type = type;
    for(uint32_t idx = 0; idx < 4; ++idx) {
        ret->inputs.emplace_back(nextId(), fmt::format("Channel{}", idx).c_str(), NodeType::Image);
    }
    ret->outputs.emplace_back(nextId(), "Output", type);
    return buildNode(mNodes, std::move(ret));
}

static ImColor getIconColor(const NodeType type) {
    switch(type) {
        case NodeType::Image:
            return { 255, 0, 0 };
        case NodeType::CubeMap:
            return { 0, 255, 0 };
        case NodeType::Sound:
            return { 0, 0, 255 };
    }
    return {};
}

static void drawPinIcon(const EditorPin& pin, const bool connected, const int alpha) {
    IconType iconType = IconType::Square;
    ImColor color = getIconColor(pin.type);
    color.Value.w = static_cast<float>(alpha) / 255.0f;
    switch(pin.type) {
        case NodeType::Image:
            iconType = IconType::Square;
            break;
        case NodeType::CubeMap:
            iconType = IconType::Diamond;
            break;
        case NodeType::Sound:
            iconType = IconType::Circle;
            break;
    }

    ax::Widgets::icon(EmToVec2(1, 1), iconType, connected, color, ImColor(32, 32, 32, alpha));
}

bool PipelineEditor::canCreateLink(const EditorPin* startPin, const EditorPin* endPin) const {
    if(endPin == startPin) {
        return false;
    }
    if(endPin->kind == startPin->kind) {
        return false;
    }
    /*
    if(endPin->type != startPin->type) {
        return false;
    }
    */
    if(endPin->node == startPin->node) {
        return false;
    }
    if(isPinLinked(endPin->id)) {
        return false;
    }
    return true;
}

bool PipelineEditor::isPinLinked(ed::PinId id) const {
    if(!id)
        return false;

    return std::any_of(mLinks.cbegin(), mLinks.cend(), [id](auto& link) { return link.startPinId == id || link.endPinId == id; });
}

EditorNode* PipelineEditor::findNode(const ed::NodeId id) const {
    if(!id)
        return nullptr;

    for(auto& node : mNodes)
        if(node->id == id)
            return node.get();

    return nullptr;
}

EditorPin* PipelineEditor::findPin(const ed::PinId id) const {
    if(!id)
        return nullptr;

    for(auto& node : mNodes) {
        for(auto& pin : node->inputs)
            if(pin.id == id)
                return &pin;

        for(auto& pin : node->outputs)
            if(pin.id == id)
                return &pin;
    }

    return nullptr;
}

void PipelineEditor::renderEditor() {
    ed::Begin("##PipelineEditor", ImVec2(0.0, 0.0));
    ax::NodeEditor::Utilities::BlueprintNodeBuilder builder(mHeaderBackground, 64, 64);

    const auto cursorTopLeft = ImGui::GetCursorScreenPos();

    mShaderNodeNames.clear();
    mShaderNodes.clear();
    const EditorNode* directRenderNode = nullptr;
    for(const auto& link : mLinks) {
        const auto u = findPin(link.startPinId);
        const auto v = findPin(link.endPinId);
        if(v->node->getClass() == NodeClass::RenderOutput && u->node->getClass() == NodeClass::GLSLShader) {
            directRenderNode = u->node;
        }
    }
    for(auto& node : mNodes) {
        if(node->getClass() == NodeClass::GLSLShader) {
            if(node.get() == directRenderNode)
                continue;
            mShaderNodeNames.push_back(node->name.c_str());
            mShaderNodes.push_back(node.get());
        }
    }

    for(auto& node : mNodes) {
        builder.begin(node->id);
        builder.header(node->color);
        ImGui::Spring(0);
        if(node->rename) {
            if(ImGui::InputText("##Name", &node->name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsNoBlank)) {
                if(isUniqueName(node->name, node.get())) {
                    node->rename = false;
                } else {
                    HelloImGui::Log(HelloImGui::LogLevel::Error, "Please specify a unique name for this node");
                }
            }
        } else
            ImGui::TextUnformatted(node->name.c_str());
        ImGui::Spring(1);
        ImGui::Dummy(EmToVec2(0, 1.5));
        ImGui::Spring(0);
        builder.endHeader();

        constexpr auto disabledAlphaScale = 48.0f / 255.0f;

        for(auto& input : node->inputs) {
            auto alpha = ImGui::GetStyle().Alpha;
            if(mNewLinkPin && !canCreateLink(mNewLinkPin, &input) && &input != mNewLinkPin)
                alpha *= disabledAlphaScale;

            builder.input(input.id);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            const bool linked = isPinLinked(input.id);
            drawPinIcon(input, linked, static_cast<int>(alpha * 255));
            ImGui::Spring(0);
            if(!input.name.empty()) {
                ImGui::TextUnformatted(input.name.c_str());
                ImGui::Spring(0);
            }
            if(linked && node->getClass() == NodeClass::GLSLShader) {
                for(auto& link : mLinks) {
                    if(link.endPinId == input.id) {
                        if(ImGui::Button(magic_enum::enum_name(link.filter).data())) {
                            link.filter = static_cast<Filter>((static_cast<uint32_t>(link.filter) + 1) %
                                                              static_cast<uint32_t>(magic_enum::enum_count<Filter>()));
                        }
                        if(ImGui::Button(magic_enum::enum_name(link.wrapMode).data())) {
                            link.wrapMode = static_cast<Wrap>((static_cast<uint32_t>(link.wrapMode) + 1) %
                                                              static_cast<uint32_t>(magic_enum::enum_count<Wrap>()));
                        }
                        break;
                    }
                }
            }
            ImGui::PopStyleVar();
            builder.endInput();
        }

        for(auto& output : node->outputs) {
            auto alpha = ImGui::GetStyle().Alpha;
            if(mNewLinkPin && !canCreateLink(mNewLinkPin, &output) && &output != mNewLinkPin)
                alpha *= disabledAlphaScale;

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            builder.output(output.id);
            if(!output.name.empty()) {
                ImGui::Spring(0);
                ImGui::TextUnformatted(output.name.c_str());
            }
            mShouldBuildPipeline |= node->renderContent();
            ImGui::Spring(0);
            drawPinIcon(output, isPinLinked(output.id), static_cast<int>(alpha * 255));
            ImGui::PopStyleVar();
            builder.endOutput();
        }

        builder.end();

        if(node->getClass() == NodeClass::LastFrame) {
            dynamic_cast<EditorLastFrame*>(node.get())->renderPopup();
        }
    }

    for(const auto& link : mLinks)
        ed::Link(link.id, link.startPinId, link.endPinId, ImColor(255, 255, 255), 2.0f);

    if(!mOnNodeCreate) {
        if(ed::BeginCreate(ImColor(255, 255, 255), 2.0f)) {
            auto showLabel = [](const char* label, ImColor color) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight());
                const auto size = ImGui::CalcTextSize(label);

                const auto padding = ImGui::GetStyle().FramePadding;
                const auto spacing = ImGui::GetStyle().ItemSpacing;

                ImGui::SetCursorPos(ImGui::GetCursorPos() + ImVec2(spacing.x, -spacing.y));

                const auto rectMin = ImGui::GetCursorScreenPos() - padding;
                const auto rectMax = ImGui::GetCursorScreenPos() + size + padding;

                const auto drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(rectMin, rectMax, color, size.y * 0.15f);
                ImGui::TextUnformatted(label);
            };

            ed::PinId startPinId = 0, endPinId = 0;
            if(ed::QueryNewLink(&startPinId, &endPinId)) {
                auto startPin = findPin(startPinId);
                auto endPin = findPin(endPinId);

                mNewLinkPin = startPin ? startPin : endPin;

                if(startPin->kind == PinKind::Input) {
                    std::swap(startPin, endPin);
                    std::swap(startPinId, endPinId);
                }

                if(startPin && endPin) {
                    if(endPin == startPin) {
                        ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                    } else if(endPin->kind == startPin->kind) {
                        showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                    }
                    /* else if(endPin->type != startPin->type) {
                        showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                    }*/
                    else if(endPin->node == startPin->node) {
                        showLabel("x Self Loop", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                    } else if(isPinLinked(endPin->id)) {
                        showLabel("x Multiple Inputs", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                    } else {
                        showLabel("+ Create Link", ImColor(32, 45, 32, 180));
                        if(ed::AcceptNewItem(ImColor(128, 255, 128), 4.0f)) {
                            mLinks.emplace_back(nextId(), startPinId, endPinId);
                        }
                    }
                }
            }

            ed::PinId pinId = 0;
            if(ed::QueryNewNode(&pinId)) {
                mNewLinkPin = findPin(pinId);
                if(mNewLinkPin)
                    showLabel("+ Create Node", ImColor(32, 45, 32, 180));

                if(ed::AcceptNewItem()) {
                    mOnNodeCreate = true;
                    mNewNodeLinkPin = findPin(pinId);
                    mNewLinkPin = nullptr;
                    ed::Suspend();
                    ImGui::OpenPopup("Create New Node");
                    ed::Resume();
                }
            }
        } else
            mNewLinkPin = nullptr;

        ed::EndCreate();

        if(ed::BeginDelete()) {
            ed::LinkId linkId = 0;
            while(ed::QueryDeletedLink(&linkId)) {
                if(ed::AcceptDeletedItem()) {
                    const auto id = std::find_if(mLinks.cbegin(), mLinks.cend(),
                                                 [linkId](const EditorLink& link) { return link.id == linkId; });
                    if(id != mLinks.end())
                        mLinks.erase(id);
                }
            }

            ed::NodeId nodeId = 0;
            while(ed::QueryDeletedNode(&nodeId)) {
                if(ed::AcceptDeletedItem()) {
                    auto id = std::find_if(mNodes.cbegin(), mNodes.cend(),
                                           [nodeId](const std::unique_ptr<EditorNode>& node) { return node->id == nodeId; });

                    if(id != mNodes.end()) {
                        mLinks.erase(std::remove_if(mLinks.begin(), mLinks.end(),
                                                    [&](auto& link) {
                                                        auto u = findPin(link.startPinId);
                                                        auto v = findPin(link.endPinId);
                                                        return (u->node == id->get() || v->node == id->get());
                                                    }),
                                     mLinks.end());
                        mNodes.erase(id);
                    }
                }
            }
        }
        ed::EndDelete();
    }
    ImGui::SetCursorScreenPos(cursorTopLeft);

    const auto openPopupPosition = ImGui::GetMousePos();
    ed::Suspend();

    if(ed::ShowNodeContextMenu(&mContextNodeId))
        ImGui::OpenPopup("Node Context Menu");
    else if(ed::ShowLinkContextMenu(&mContextLinkId))
        ImGui::OpenPopup("Link Context Menu");
    else if(ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("Create New Node");
        mNewNodeLinkPin = nullptr;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, EmToVec2(0.25, 0.25));
    if(ImGui::BeginPopup("Node Context Menu")) {
        const auto node = findNode(mContextNodeId);
        if(!node->rename && ImGui::MenuItem("Rename")) {
            node->rename = true;
        }
        if(node->getClass() != NodeClass::RenderOutput && ImGui::MenuItem("Delete"))
            ed::DeleteNode(mContextNodeId);
        ImGui::EndPopup();
    }

    if(ImGui::BeginPopup("Link Context Menu")) {
        if(ImGui::MenuItem("Delete"))
            ed::DeleteLink(mContextLinkId);
        ImGui::EndPopup();
    }
    if(ImGui::BeginPopup("Create New Node")) {
        const auto newNodePosition = openPopupPosition;

        EditorNode* node = nullptr;

        ImGui::TextUnformatted("New Node");

        ImGui::Separator();
        if(ImGui::MenuItem("Texture"))
            node = &spawnTexture();
        if(ImGui::MenuItem("CubeMap"))
            node = &spawnCubeMap();
        if(ImGui::MenuItem("LastFrame"))
            node = &spawnLastFrame();
        auto hasClass = [&](NodeClass nodeClass) {
            return std::any_of(mNodes.begin(), mNodes.end(), [&](const auto& it) { return it->getClass() == nodeClass; });
        };
        if(!hasClass(NodeClass::Keyboard) && ImGui::MenuItem("Keyboard"))
            node = &spawnKeyboard();

        ImGui::Separator();
        if(ImGui::MenuItem("Shader"))
            node = &spawnShader(NodeType::Image);

        ImGui::Separator();
        if(!hasClass(NodeClass::RenderOutput) && ImGui::MenuItem("Render Output"))
            node = &spawnRenderOutput();

        if(node) {
            mOnNodeCreate = false;

            ed::SetNodePosition(node->id, newNodePosition);

            if(auto startPin = mNewNodeLinkPin) {
                auto& pins = startPin->kind == PinKind::Input ? node->outputs : node->inputs;

                for(auto& pin : pins) {
                    auto endPin = &pin;
                    if(startPin->kind == PinKind::Input)
                        std::swap(startPin, endPin);
                    if(canCreateLink(startPin, endPin)) {
                        mLinks.emplace_back(nextId(), startPin->id, endPin->id);
                        break;
                    }
                }
            }
        }

        ImGui::EndPopup();
    } else
        mOnNodeCreate = false;
    ImGui::PopStyleVar();
    ed::Resume();

    ed::End();
}

static void setupKeyboardData(uint32_t* data) {
    // See also
    // https://shadertoyunofficial.wordpress.com/2016/07/20/special-shadertoy-features/
    // FIXME: remapping keys
    constexpr std::pair<int32_t, ImGuiKey> mapping[] = {
        { 8, ImGuiKey_Backspace },
        { 9, ImGuiKey_Tab },
        { 13, ImGuiKey_Enter },
        { 16, ImGuiKey_LeftShift },
        { 16, ImGuiKey_RightShift },
        { 17, ImGuiKey_LeftCtrl },
        { 17, ImGuiKey_RightCtrl },
        { 19, ImGuiKey_Pause },
        { 20, ImGuiKey_CapsLock },
        { 27, ImGuiKey_Escape },
        { 32, ImGuiKey_Space },
        { 33, ImGuiKey_PageUp },
        { 36, ImGuiKey_Home },
        { 37, ImGuiKey_LeftArrow },
        { 38, ImGuiKey_UpArrow },
        { 39, ImGuiKey_RightArrow },
        { 40, ImGuiKey_DownArrow },
        { 44, ImGuiKey_PrintScreen },
        { 45, ImGuiKey_Insert },
        { 46, ImGuiKey_Delete },
        { 48, ImGuiKey_0 },
        { 49, ImGuiKey_1 },
        { 50, ImGuiKey_2 },
        { 51, ImGuiKey_3 },
        { 52, ImGuiKey_4 },
        { 53, ImGuiKey_5 },
        { 54, ImGuiKey_6 },
        { 55, ImGuiKey_7 },
        { 56, ImGuiKey_8 },
        { 57, ImGuiKey_9 },
        { 65, ImGuiKey_A },
        { 66, ImGuiKey_B },
        { 67, ImGuiKey_C },
        { 68, ImGuiKey_D },
        { 69, ImGuiKey_E },
        { 70, ImGuiKey_F },
        { 71, ImGuiKey_G },
        { 72, ImGuiKey_H },
        { 73, ImGuiKey_I },
        { 74, ImGuiKey_J },
        { 75, ImGuiKey_K },
        { 76, ImGuiKey_L },
        { 77, ImGuiKey_M },
        { 78, ImGuiKey_N },
        { 79, ImGuiKey_O },
        { 80, ImGuiKey_P },
        { 81, ImGuiKey_Q },
        { 82, ImGuiKey_R },
        { 83, ImGuiKey_S },
        { 84, ImGuiKey_T },
        { 85, ImGuiKey_U },
        { 86, ImGuiKey_V },
        { 87, ImGuiKey_W },
        { 88, ImGuiKey_X },
        { 89, ImGuiKey_Y },
        { 90, ImGuiKey_Z },
        { 96, ImGuiKey_Keypad0 },
        { 97, ImGuiKey_Keypad1 },
        { 98, ImGuiKey_Keypad2 },
        { 99, ImGuiKey_Keypad3 },
        { 100, ImGuiKey_Keypad4 },
        { 101, ImGuiKey_Keypad5 },
        { 102, ImGuiKey_Keypad6 },
        { 103, ImGuiKey_Keypad7 },
        { 104, ImGuiKey_Keypad8 },
        { 105, ImGuiKey_Keypad9 },
        { 106, ImGuiKey_KeypadMultiply },
        { 109, ImGuiKey_KeypadSubtract },
        { 110, ImGuiKey_KeypadDecimal },
        { 111, ImGuiKey_KeypadDivide },
        { 112, ImGuiKey_F1 },
        { 113, ImGuiKey_F2 },
        { 114, ImGuiKey_F3 },
        { 115, ImGuiKey_F4 },
        { 116, ImGuiKey_F5 },
        { 117, ImGuiKey_F6 },
        { 118, ImGuiKey_F7 },
        { 119, ImGuiKey_F8 },
        { 120, ImGuiKey_F9 },
        { 121, ImGuiKey_F10 },
        { 122, ImGuiKey_F11 },
        { 123, ImGuiKey_F12 },
        { 145, ImGuiKey_ScrollLock },
        { 144, ImGuiKey_NumLock },
        { 187, ImGuiKey_Equal },
        { 189, ImGuiKey_Minus },
        { 192, ImGuiKey_GraveAccent },
        { 219, ImGuiKey_LeftBracket },
        { 220, ImGuiKey_Backslash },
        { 221, ImGuiKey_RightBracket },
    };
    auto getKey = [&](int32_t x, int32_t y) -> uint32_t& { return data[x + y * 256]; };
    for(auto [idx, key] : mapping) {
        const auto down = ImGui::IsKeyDown(key);
        const auto pressed = ImGui::IsKeyPressed(key, false);
        constexpr uint32_t mask = 0xffffffff;
        getKey(idx, 0) = down ? mask : 0;
        getKey(idx, 1) = pressed ? mask : 0;
        if(pressed)
            getKey(idx, 2) ^= mask;
    }
}

std::unique_ptr<Pipeline> PipelineEditor::buildPipeline() {
    std::unordered_map<EditorNode*, std::vector<std::tuple<EditorNode*, uint32_t, EditorLink*>>> graph;
    EditorNode* directRenderNode = nullptr;
    std::unordered_map<EditorNode*, uint32_t> degree;
    EditorNode* sinkNode = nullptr;
    for(auto& link : mLinks) {
        auto u = findPin(link.startPinId);
        auto v = findPin(link.endPinId);
        auto idx = static_cast<uint32_t>(v - v->node->inputs.data());
        graph[v->node].emplace_back(u->node, idx, &link);
        ++degree[u->node];
        if(v->node->getClass() == NodeClass::RenderOutput && u->node->getClass() == NodeClass::GLSLShader) {
            sinkNode = v->node;
            directRenderNode = u->node;
        }
    }

    if(!sinkNode) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Exactly one shader should be connected to the final render output");
        throw Error{};
    }

    std::unordered_set<EditorNode*> visited;
    std::queue<EditorNode*> q;
    std::vector<EditorNode*> order;
    q.push(sinkNode);
    std::unordered_set<EditorNode*> weakRef;
    for(auto& node : mNodes) {
        if(node->getClass() == NodeClass::LastFrame) {
            weakRef.insert(dynamic_cast<EditorLastFrame*>(node.get())->lastFrame);
        }
    }
    for(auto node : weakRef) {
        if(!degree.count(node))
            q.push(node);
    }
    while(!q.empty()) {
        auto u = q.front();
        q.pop();
        visited.insert(u);
        order.push_back(u);

        if(auto it = graph.find(u); it != graph.cend()) {
            for(auto [v, idx, link] : it->second) {
                visited.insert(v);
                if(--degree[v] == 0) {
                    q.push(v);
                }
            }
        }
    }

    if(visited.size() != order.size()) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Loop detected");
        throw Error{};
    }

    std::reverse(order.begin(), order.end());

    auto pipeline = createPipeline();
    std::unordered_map<EditorNode*, DoubleBufferedTex> textureMap;
    std::unordered_map<EditorNode*, ImVec2> textureSizeMap;
    std::unordered_map<EditorNode*, std::vector<DoubleBufferedFB>> frameBufferMap;
    std::unordered_set<EditorNode*> requireDoubleBuffer;
    for(auto node : order) {
        if(node->getClass() == NodeClass::LastFrame) {
            auto ref = dynamic_cast<EditorLastFrame&>(*node).lastFrame;
            if(!ref || ref == directRenderNode) {
                HelloImGui::Log(HelloImGui::LogLevel::Error, "Invalid reference");
                throw Error{};
            }
            requireDoubleBuffer.insert(ref);
        }
    }
    // TODO: FB allocation
    for(auto node : order) {
        if(node->getClass() == NodeClass::GLSLShader) {
            if(node->type == NodeType::Image) {
                DoubleBufferedFB frameBuffer{ nullptr };
                if(requireDoubleBuffer.count(node)) {
                    auto t1 = pipeline->createFrameBuffer();
                    auto t2 = pipeline->createFrameBuffer();
                    frameBuffer = DoubleBufferedFB{ t1, t2 };
                } else if(node != directRenderNode) {
                    auto t = pipeline->createFrameBuffer();
                    frameBuffer = DoubleBufferedFB{ t };
                }
                frameBufferMap.emplace(node, std::vector<DoubleBufferedFB>{ frameBuffer });
            } else if(node->type == NodeType::CubeMap) {
                std::vector<DoubleBufferedFB> buffers;
                buffers.reserve(6);
                if(requireDoubleBuffer.count(node)) {
                    auto t1 = pipeline->createCubeMapFrameBuffer();
                    auto t2 = pipeline->createCubeMapFrameBuffer();
                    for(uint32_t idx = 0; idx < 6; ++idx)
                        buffers.emplace_back(t1[idx], t2[idx]);
                } else {
                    assert(node != directRenderNode);
                    auto t = pipeline->createCubeMapFrameBuffer();
                    for(uint32_t idx = 0; idx < 6; ++idx)
                        buffers.emplace_back(t[idx]);
                }
                frameBufferMap.emplace(node, std::move(buffers));
            } else {
                HelloImGui::Log(HelloImGui::LogLevel::Error, "Unsupported shader type");
                throw Error{};
            }
        }
    }

    for(auto node : order) {
        switch(node->getClass()) {  // NOLINT(clang-diagnostic-switch-enum)
            case NodeClass::GLSLShader: {
                auto& target = frameBufferMap.at(node);
                std::vector<Channel> channels;
                if(auto it = graph.find(node); it != graph.cend()) {
                    for(auto [v, idx, link] : it->second) {
                        std::optional<ImVec2> size = std::nullopt;
                        if(auto iter = textureSizeMap.find(v); iter != textureSizeMap.cend())
                            size = iter->second;
                        channels.push_back(Channel{ idx, textureMap.at(v), link->filter, link->wrapMode, size });
                    }
                }
                // TODO: error markers
                auto guard = scopeFail(
                    [&] { HelloImGui::Log(HelloImGui::LogLevel::Error, "Failed to compile shader %s", node->name.c_str()); });
                pipeline->addPass(dynamic_cast<EditorShader*>(node)->editor.getText(), node->type, target, std::move(channels));
                if(target.front().t1)
                    textureMap.emplace(node,
                                       DoubleBufferedTex{ target.front().t1->getTexture(), target.front().t2->getTexture(),
                                                          node->type == NodeType::CubeMap });
                break;
            }
            case NodeClass::LastFrame: {
                const auto ref = dynamic_cast<EditorLastFrame*>(node)->lastFrame;
                auto target = frameBufferMap.at(ref).front();
                assert(target.t1 && target.t2);
                textureMap.emplace(
                    node, DoubleBufferedTex{ target.t2->getTexture(), target.t1->getTexture(), ref->type == NodeType::CubeMap });
                break;
            }
            case NodeClass::RenderOutput: {
                break;
            }
            case NodeClass::Texture: {
                auto& textureId = dynamic_cast<EditorTexture*>(node)->textureId;
                textureSizeMap.emplace(node, textureId->size());
                textureMap.emplace(node, DoubleBufferedTex{ textureId->getTexture(), false });
                break;
            }
            case NodeClass::CubeMap: {
                auto& textureId = dynamic_cast<EditorCubeMap*>(node)->textureId;
                textureSizeMap.emplace(node, textureId->size());
                textureMap.emplace(node, DoubleBufferedTex{ textureId->getTexture(), true });
                break;
            }
            case NodeClass::Keyboard: {
                textureSizeMap.emplace(node, ImVec2{ 256, 3 });
                textureMap.emplace(node, DoubleBufferedTex{ pipeline->createDynamicTexture(256, 3, setupKeyboardData), false });
                break;
            }
            default:
                reportNotImplemented();
        }
    }

    return pipeline;
}

void PipelineEditor::build(ShaderToyContext& context) {
    try {
        const auto start = Clock::now();
        context.reset(buildPipeline());
        const auto duration =
            static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count()) * 1e-9;
        Log(HelloImGui::LogLevel::Info, "Compiled in %.1f secs", duration);
    } catch(const Error&) {
        Log(HelloImGui::LogLevel::Error, "Build failed");
    }
}

void PipelineEditor::render(ShaderToyContext& context) {
    updateNodeType();
    if(!ImGui::Begin("Editor", nullptr)) {
        ImGui::End();
        return;
    }

    if(ImGui::BeginTabBar("##EditorTabBar", ImGuiTabBarFlags_Reorderable)) {
        // pipeline editor
        if(ImGui::BeginTabItem("Pipeline", nullptr, ImGuiTabItemFlags_NoReorder)) {

            ed::SetCurrentEditor(mCtx);
            // toolbar
            if(ImGui::Button(ICON_FA_PLAY " Build")) {
                mShouldBuildPipeline = true;
            }
            ImGui::SameLine();
            if(ImGui::Button("Zoom to context")) {
                mShouldZoomToContent = true;
            }
            if(mShouldZoomToContent) {
                ed::NavigateToContent();
                mShouldZoomToContent = false;
            }
            ImGui::SameLine();
            if(ImGui::Button("Reset layout")) {
                mShouldResetLayout = true;
            }
            if(mShouldResetLayout) {
                resetLayout();
                mShouldResetLayout = false;
            }
            ImGui::SameLine();
            if(ImGui::Button(ICON_FA_EDIT " Edit metadata")) {
                mOpenMetadataEditor = true;
                mMetadataEditorRequestFocus = true;
            }

            renderEditor();
            ed::SetCurrentEditor(nullptr);
            ImGui::EndTabItem();
        }
        if(mOpenMetadataEditor &&
           ImGui::BeginTabItem("Metadata", &mOpenMetadataEditor,
                               mMetadataEditorRequestFocus ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
            if(ImGui::Button(ICON_FA_PLUS " Add item")) {
                mMetadata.emplace_back("Key", "Value");
            }
            if(ImGui::BeginChild("##StringMap")) {
                uint32_t removeIdx = std::numeric_limits<uint32_t>::max();
                uint32_t idx = 0;
                const auto width = ImGui::GetContentRegionAvail().x / 7.0f * 3.0f;
                for(auto& [k, v] : mMetadata) {
                    ImGui::SetNextItemWidth(width);
                    ImGui::InputText(fmt::format("##Key{}", idx).c_str(), &k);
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(width);
                    ImGui::InputText(fmt::format("##Value{}", idx).c_str(), &v);
                    ImGui::SameLine();
                    if(ImGui::Button(ICON_FA_TIMES)) {
                        removeIdx = idx;
                    }
                    ++idx;
                }

                if(removeIdx != std::numeric_limits<uint32_t>::max()) {
                    mMetadata.erase(mMetadata.cbegin() + removeIdx);
                }
            }
            ImGui::EndChild();

            mMetadataEditorRequestFocus = false;
            ImGui::EndTabItem();
        }
        // source editor
        for(auto& node : mNodes) {
            if(const auto shader = dynamic_cast<EditorShader*>(node.get())) {
                if(shader->isOpen &&
                   ImGui::BeginTabItem(shader->name.c_str(), &shader->isOpen,
                                       shader->requestFocus ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None)) {
                    shader->editor.render(ImVec2(0, 0));
                    shader->requestFocus = false;
                    ImGui::EndTabItem();
                }
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();

    if(mShouldBuildPipeline) {
        build(context);
        mShouldBuildPipeline = false;
    }
}

PipelineEditor& PipelineEditor::get() {
    static PipelineEditor instance;
    return instance;
}

std::unique_ptr<Node> EditorRenderOutput::toSTTF() const {
    return std::make_unique<RenderOutput>();
}
void EditorRenderOutput::fromSTTF(Node& node) {
    type = node.getNodeType();
}

bool EditorShader::renderContent() {
    if(ImGui::Button(ICON_FA_EDIT " Edit")) {
        isOpen = true;
        requestFocus = true;
    }
    if(ImGui::Button(magic_enum::enum_name(type).data())) {
        type = static_cast<NodeType>((static_cast<uint32_t>(type) + 1) % 3);
    }
    return false;
}
std::unique_ptr<Node> EditorShader::toSTTF() const {
    return std::make_unique<GLSLShader>(editor.getText(), type);
}
void EditorShader::fromSTTF(Node& node) {
    const auto& shader = dynamic_cast<GLSLShader&>(node);
    type = shader.nodeType;
    editor.setText(shader.source);
}

struct ImageStorage final {
    uint32_t width;
    uint32_t height;
    std::vector<uint32_t> data;
};

static ImageStorage loadImageFromFile(const char* path) {
    HelloImGui::Log(HelloImGui::LogLevel::Info, "Loading image %s", path);
    stbi_set_flip_vertically_on_load(true);
    int width, height, channels;
    const auto ptr = stbi_load(path, &width, &height, &channels, 4);
    if(!ptr) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Failed to load image %s: %s", path, stbi_failure_reason());
        return { 0, 0, {} };
    }
    auto guard = scopeExit([ptr] { stbi_image_free(ptr); });
    const auto begin = reinterpret_cast<const uint32_t*>(ptr);
    const auto end = begin + static_cast<ptrdiff_t>(width) * height;
    return { static_cast<uint32_t>(width), static_cast<uint32_t>(height), std::vector<uint32_t>{ begin, end } };
}

bool EditorTexture::renderContent() {
    bool updateTex = false;
    if(ImGui::Button(ICON_FA_FILE_IMAGE " Update")) {
        [&] {
            nfdchar_t* path;
            if(NFD_OpenDialog("jpg,jpeg;bmp;png;tga;tiff", nullptr, &path) == NFD_OKAY) {
                auto [width, height, img] = loadImageFromFile(path);
                if(img.empty())
                    return;
                pixel = std::move(img);
                textureId = loadTexture(width, height, pixel.data());
                updateTex = true;
            }
        }();
    }
    if(textureId && ImGui::Button("Vertical Flip")) {
        const auto width = static_cast<uint32_t>(textureId->size().x);
        const auto height = static_cast<uint32_t>(textureId->size().y);
        for(uint32_t i = 0, j = height - 1; i < j; ++i, --j) {
            for(uint32_t k = 0; k < width; ++k)
                std::swap(pixel[i * width + k], pixel[j * width + k]);
        }
        textureId = loadTexture(width, height, pixel.data());
        updateTex = true;
    }

    if(textureId) {
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        ImGui::Image(reinterpret_cast<ImTextureID>(textureId->getTexture()), EmToVec2(3, 3), ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
    }
    return updateTex;
}
std::unique_ptr<Node> EditorTexture::toSTTF() const {
    return std::make_unique<Texture>(static_cast<uint32_t>(textureId->size().x), static_cast<uint32_t>(textureId->size().y),
                                     pixel);
}
void EditorTexture::fromSTTF(Node& node) {
    const auto& texture = dynamic_cast<Texture&>(node);
    pixel = texture.pixel;
    textureId = loadTexture(texture.width, texture.height, pixel.data());
}
bool EditorCubeMap::renderContent() {
    bool updateTex = false;
    if(ImGui::Button(ICON_FA_FILE_IMAGE " Update")) {
        [&] {
            nfdpathset_t pathSet;
            if(NFD_OpenDialogMultiple("jpg,jpeg;bmp;png;tga;tiff", nullptr, &pathSet) == NFD_OKAY) {
                auto guard = scopeExit([&] { NFD_PathSet_Free(&pathSet); });

                if(NFD_PathSet_GetCount(&pathSet) != 6) {
                    HelloImGui::Log(HelloImGui::LogLevel::Error, "Please choose exactly 6 images for cube map");
                    return;
                }

                std::vector<uint32_t> tmp;
                uint32_t size = 0;
                for(int32_t idx = 0; idx < 6; ++idx) {
                    auto [w, h, img] = loadImageFromFile(NFD_PathSet_GetPath(&pathSet, idx));
                    if(img.empty())
                        return;
                    if(w != h)
                        return;
                    if(size == 0)
                        size = w;
                    else if(size != w)
                        return;
                    tmp.insert(tmp.end(), img.begin(), img.end());
                }
                tmp.swap(pixel);
                textureId = loadCubeMap(size, pixel.data());
                updateTex = true;
            }
        }();
    }
    // TODO: preview for cube map
    ImGui::Text(textureId ? "Loaded" : "Unavailable");

    return updateTex;
}
std::unique_ptr<Node> EditorCubeMap::toSTTF() const {
    return std::make_unique<CubeMap>(static_cast<uint32_t>(textureId->size().x), pixel);
}
void EditorCubeMap::fromSTTF(Node& node) {
    const auto& texture = dynamic_cast<CubeMap&>(node);
    pixel = texture.pixel;
    textureId = loadCubeMap(texture.size, pixel.data());
}

// See also https://github.com/thedmd/imgui-node-editor/issues/48
bool EditorLastFrame::renderContent() {
    const auto& editor = PipelineEditor::get();
    auto& selectables = editor.mShaderNodes;
    if(std::find(selectables.cbegin(), selectables.cend(), lastFrame) == selectables.cend())
        lastFrame = nullptr;
    if(ImGui::Button(lastFrame ? lastFrame->name.c_str() : "<Select One>")) {
        openPopup = true;
    }
    return false;
}
void EditorLastFrame::renderPopup() {
    const auto& editor = PipelineEditor::get();
    const auto& names = editor.mShaderNodeNames;
    const auto& nodes = editor.mShaderNodes;

    ed::Suspend();
    if(openPopup) {
        ImGui::OpenPopup("##popup_button");
        openPopup = false;
        editing = true;
    }

    if(editing && ImGui::BeginPopup("##popup_button")) {
        lastFrame = nullptr;
        ImGui::BeginChild("##popup_scroller", EmToVec2(4, 4), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for(uint32_t idx = 0; idx < names.size(); ++idx) {
            if(ImGui::Button(names[idx])) {
                lastFrame = nodes[idx];
                editing = false;
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    } else
        editing = false;
    ed::Resume();
}
std::unique_ptr<Node> EditorLastFrame::toSTTF() const {
    return std::make_unique<LastFrame>(lastFrame->name, type);
}
void EditorLastFrame::fromSTTF(Node&) {
    // should be fixed by post processing
}
std::unique_ptr<Node> EditorKeyboard::toSTTF() const {
    return std::make_unique<Keyboard>();
}
void EditorKeyboard::fromSTTF(Node&) {}
void PipelineEditor::loadSTTF(const std::string& path) {
    try {
        HelloImGui::Log(HelloImGui::LogLevel::Info, "Loading sttf from %s", path.c_str());
        ShaderToyTransmissionFormat sttf;
        sttf.load(path);

        std::vector<std::unique_ptr<EditorNode>> oldNodes;
        oldNodes.swap(mNodes);
        std::vector<EditorLink> oldLinks;
        oldLinks.swap(mLinks);
        std::vector<std::pair<std::string, std::string>> oldMetadata;
        oldMetadata.swap(mMetadata);
        auto guard = scopeFail([&] {
            oldNodes.swap(mNodes);
            oldLinks.swap(mLinks);
            oldMetadata.swap(mMetadata);
        });

        for(auto [k, v] : sttf.metadata) {
            mMetadata.emplace_back(k, v);
        }

        std::unordered_map<Node*, EditorNode*> nodeMap;
        for(auto& node : sttf.nodes) {
            EditorNode* newNode = nullptr;
            switch(node->getNodeClass()) {  // NOLINT(clang-diagnostic-switch-enum)
                case NodeClass::RenderOutput: {
                    newNode = &spawnRenderOutput();
                } break;
                case NodeClass::GLSLShader: {
                    newNode = &spawnShader(node->getNodeType());
                } break;
                case NodeClass::Texture: {
                    newNode = &spawnTexture();
                } break;
                case NodeClass::CubeMap: {
                    newNode = &spawnCubeMap();
                } break;
                case NodeClass::LastFrame: {
                    newNode = &spawnLastFrame();
                } break;
                case NodeClass::Keyboard: {
                    newNode = &spawnKeyboard();
                } break;
                default: {
                    reportNotImplemented();
                }
            }

            newNode->fromSTTF(*node);
            nodeMap.emplace(node.get(), newNode);
        }

        // fix references of LastFrame
        for(auto& node : sttf.nodes) {
            if(node->getNodeClass() == NodeClass::LastFrame) {
                auto editorNode = nodeMap.at(node.get());
                dynamic_cast<EditorLastFrame*>(editorNode)->lastFrame = nodeMap.at(dynamic_cast<LastFrame*>(node.get())->refNode);
            }
        }

        for(auto& [start, end, filter, wrapMode, slot] : sttf.links) {
            auto startNode = nodeMap.at(start);
            auto endNode = nodeMap.at(end);
            mLinks.emplace_back(nextId(), startNode->outputs.front().id, endNode->inputs[slot].id, filter, wrapMode);
        }

        HelloImGui::Log(HelloImGui::LogLevel::Info, "Success!");

        mShouldResetLayout = true;
        mShouldBuildPipeline = true;
    } catch(const Error&) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Failed to load sttf %s", path.c_str());
    }
}
void PipelineEditor::saveSTTF(const std::string& path) {
    try {
        HelloImGui::Log(HelloImGui::LogLevel::Info, "Writing shader to sttf file %s", path.c_str());
        ShaderToyTransmissionFormat sttf;
        for(auto& [key, val] : mMetadata)
            sttf.metadata.emplace(key, val);
        std::unordered_map<EditorNode*, Node*> nodeMap;
        for(auto& node : mNodes) {
            sttf.nodes.push_back(node->toSTTF());
            auto& sttfNode = sttf.nodes.back();
            sttfNode->name = node->name;
            nodeMap.emplace(node.get(), sttfNode.get());
        }
        for(const auto& link : mLinks) {
            const auto startPin = findPin(link.startPinId);
            const auto endPin = findPin(link.endPinId);
            const auto slot = static_cast<uint32_t>(endPin - endPin->node->inputs.data());
            sttf.links.push_back(Link{ nodeMap.at(startPin->node), nodeMap.at(endPin->node), link.filter, link.wrapMode, slot });
        }
        sttf.save(path);
        HelloImGui::Log(HelloImGui::LogLevel::Info, "Success!");
    } catch(const Error&) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Failed to save sttf %s", path.c_str());
    }
}
void PipelineEditor::loadFromShaderToy(const std::string& path) {
    std::vector<std::unique_ptr<EditorNode>> oldNodes;
    oldNodes.swap(mNodes);
    std::vector<EditorLink> oldLinks;
    oldLinks.swap(mLinks);
    std::vector<std::pair<std::string, std::string>> oldMetadata;
    oldMetadata.swap(mMetadata);
    auto guard = scopeFail([&] {
        oldNodes.swap(mNodes);
        oldLinks.swap(mLinks);
        oldMetadata.swap(mMetadata);
    });

    std::string_view shaderId = path;
    if(const auto pos = shaderId.find_last_of('/'); pos != std::string_view::npos)
        shaderId = shaderId.substr(pos + 1);
    const auto url = fmt::format("https://www.shadertoy.com/view/{}", shaderId);
    HelloImGui::Log(HelloImGui::LogLevel::Info, "Loading from %s", url.c_str());
    httplib::SSLClient client{ "www.shadertoy.com" };
    httplib::Headers headers;
    headers.emplace("referer", url);
    auto res = client.Post("/shadertoy", headers, std::string(R"(s={"shaders":[")") + shaderId.data() + "\"]}&nt=1&nl=1&np=1",
                           "application/x-www-form-urlencoded");
    int status = res.value().status;
    if(status != 200) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Invalid response from shadertoy.com (Status code = %d).", status);
        throw Error{};
    }
    auto json = nlohmann::json::parse(res->body);
    if(!json.is_array()) {
        HelloImGui::Log(HelloImGui::LogLevel::Error, "Invalid response from shadertoy.com");
        throw Error{};
    }
    auto metadata = json[0].at("info");

    mMetadata.emplace_back("Name", metadata.at("name").get<std::string>());
    mMetadata.emplace_back("Author", metadata.at("username").get<std::string>());
    mMetadata.emplace_back("Description", metadata.at("description").get<std::string>());
    mMetadata.emplace_back("ShaderToyURL", url);

    auto renderPasses = json[0].at("renderpass");
    // BA BB BC BD CA IE
    auto getOrder = [](const std::string& name) { return std::toupper(name.front()) * 1000 + std::toupper(name.back()); };

    std::unordered_map<std::string, EditorShader*> newShaderNodes;

    auto& sinkNode = spawnRenderOutput();
    auto addLink = [&](EditorNode* src, EditorNode* dst, uint32_t channel, nlohmann::json* ref) {
        auto filter = Filter::Linear;
        auto wrapMode = Wrap::Repeat;
        if(ref) {
            auto sampler = ref->at("sampler");
            const auto filterName = sampler.at("filter").get<std::string>();
            const auto wrapName = sampler.at("wrap").get<std::string>();
            if(filterName == "linear") {
                filter = Filter::Linear;
            } else if(filterName == "nearest") {
                filter = Filter::Nearest;
            } else if(filterName == "mipmap") {
                filter = Filter::Mipmap;
            } else {
                reportNotImplemented();
            }

            if(wrapName == "clamp") {
                wrapMode = Wrap::Clamp;
            } else if(wrapName == "repeat") {
                wrapMode = Wrap::Repeat;
            } else {
                reportNotImplemented();
            }
        }
        mLinks.emplace_back(nextId(), src->outputs.front().id, dst->inputs[channel].id, filter, wrapMode);
    };
    EditorNode* keyboard = nullptr;
    auto getKeyboard = [&] {
        if(!keyboard)
            keyboard = &spawnKeyboard();
        return keyboard;
    };
    std::unordered_map<std::string, EditorTexture*> textureCache;
    auto getTexture = [&](nlohmann::json& tex) -> EditorTexture* {
        const auto id = tex.at("id").get<std::string>();
        if(const auto iter = textureCache.find(id); iter != textureCache.cend())
            return iter->second;
        auto& texture = spawnTexture();
        const auto texPath = tex.at("filepath").get<std::string>();
        HelloImGui::Log(HelloImGui::LogLevel::Info, "Downloading texture %s", texPath.c_str());
        auto img = client.Get(texPath, headers);

        stbi_set_flip_vertically_on_load(tex.at("sampler").at("vflip").get<std::string>() == "true");
        int width, height, channels;
        const auto ptr = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(img->body.data()),
                                               static_cast<int>(img->body.size()), &width, &height, &channels, 4);
        if(!ptr) {
            HelloImGui::Log(HelloImGui::LogLevel::Info, "Failed to load texture %s: %s", texPath.c_str(), stbi_failure_reason());
            throw Error{};
        }
        const auto imgGuard = scopeExit([ptr] { stbi_image_free(ptr); });
        const auto begin = reinterpret_cast<const uint32_t*>(ptr);
        const auto end = begin + static_cast<ptrdiff_t>(width) * height;
        texture.pixel = std::vector<uint32_t>{ begin, end };
        texture.textureId = loadTexture(static_cast<uint32_t>(width), static_cast<uint32_t>(height), texture.pixel.data());

        textureCache.emplace(id, &texture);
        return &texture;
    };
    std::unordered_map<std::string, EditorCubeMap*> cubeMapCache;
    auto getCubeMap = [&](nlohmann::json& tex) -> EditorCubeMap* {
        const auto id = tex.at("id").get<std::string>();
        if(const auto iter = cubeMapCache.find(id); iter != cubeMapCache.cend())
            return iter->second;
        auto& texture = spawnCubeMap();
        const auto texPath = tex.at("filepath").get<std::string>();
        std::string base, ext;
        if(const auto pos = texPath.find_last_of('.'); pos != std::string::npos) {
            base = texPath.substr(0, pos);
            ext = texPath.substr(pos);
        } else {
            HelloImGui::Log(HelloImGui::LogLevel::Info, "Failed to parse cube map %s", texPath.c_str());
            throw Error{};
        }

        constexpr const char* suffixes[] = { "", "_1", "_2", "_3", "_4", "_5" };
        int32_t size = 0;
        for(const auto suffix : suffixes) {
            auto facePath = base;
            facePath += suffix;
            facePath += ext;
            HelloImGui::Log(HelloImGui::LogLevel::Info, "Downloading texture %s", facePath.c_str());
            auto img = client.Get(facePath, headers);

            stbi_set_flip_vertically_on_load(tex.at("sampler").at("vflip").get<std::string>() == "true");
            int width, height, channels;
            const auto ptr = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(img->body.data()),
                                                   static_cast<int>(img->body.size()), &width, &height, &channels, 4);
            if(!ptr) {
                HelloImGui::Log(HelloImGui::LogLevel::Info, "Failed to load texture %s: %s", facePath.c_str(),
                                stbi_failure_reason());
                throw Error{};
            }
            const auto imgGuard = scopeExit([ptr] { stbi_image_free(ptr); });
            const auto begin = reinterpret_cast<const uint32_t*>(ptr);
            const auto end = begin + static_cast<ptrdiff_t>(width) * height;
            texture.pixel.insert(texture.pixel.end(), begin, end);
            if(width != height) {
                throw Error{};
            }
            if(size == 0)
                size = width;
            else if(size != width) {
                throw Error{};
            }
        }

        texture.textureId = loadCubeMap(static_cast<uint32_t>(size), texture.pixel.data());
        cubeMapCache.emplace(id, &texture);
        return &texture;
    };
    std::unordered_set<std::string> passIds;
    const auto isDynamicCubeMap = [&](nlohmann::json& tex) {
        const auto id = tex.at("id").get<std::string>();
        return passIds.count(id) != 0;
    };
    std::string common;
    for(auto& pass : renderPasses) {
        if(pass.at("name").get<std::string>().empty()) {
            pass.at("name") = generateUniqueName(pass.at("type").get<std::string>());
        }
        if(pass.at("outputs").empty()) {
            pass.at("outputs").push_back(nlohmann::json::object({ { "id", "tmp" + std::to_string(nextId()) } }));
        }
        passIds.insert(pass.at("outputs")[0].at("id").get<std::string>());
    }
    for(auto& pass : renderPasses) {
        const auto type = pass.at("type").get<std::string>();
        const auto code = pass.at("code").get<std::string>();
        const auto name = pass.at("name").get<std::string>();
        if(type == "common") {
            common = code + '\n';
        } else if(type == "image" || type == "buffer" || type == "cubemap") {
            const auto output = pass.at("outputs")[0].at("id").get<std::string>();
            auto& node = spawnShader(type != "cubemap" ? NodeType::Image : NodeType::CubeMap);
            node.editor.setText(code);
            node.name = name;
            newShaderNodes.emplace(output, &node);

            for(auto& input : pass.at("inputs")) {
                auto inputType = input.at("type").get<std::string>();
                if(inputType == "buffer") {
                    continue;
                }
                auto channel = input.at("channel").get<uint32_t>();
                if(inputType == "keyboard") {
                    addLink(getKeyboard(), &node, channel, &input);
                } else if(inputType == "texture") {
                    addLink(getTexture(input), &node, channel, &input);
                } else if(inputType == "cubemap") {
                    if(!isDynamicCubeMap(input))
                        addLink(getCubeMap(input), &node, channel, &input);
                } else {
                    Log(HelloImGui::LogLevel::Error, "Unsupported input type %s", inputType.c_str());
                }
            }

            if(type == "image") {
                addLink(&node, &sinkNode, 0, nullptr);
            }
        } else {
            Log(HelloImGui::LogLevel::Error, "Unsupported pass type %s", type.c_str());
        }
    }

    if(!common.empty()) {
        for(auto& [name, shader] : newShaderNodes) {
            shader->editor.setText(common + shader->editor.getText());
        }
    }

    std::unordered_map<EditorShader*, EditorLastFrame*> lastFrames;
    auto getLastFrame = [&](EditorShader* src) {
        if(const auto iter = lastFrames.find(src); iter != lastFrames.cend()) {
            return iter->second;
        }
        auto& lastFrame = spawnLastFrame();
        lastFrame.lastFrame = src;
        lastFrames.emplace(src, &lastFrame);
        return &lastFrame;
    };
    for(auto& pass : renderPasses) {
        const auto type = pass.at("type").get<std::string>();
        if(type == "common") {
            continue;
        }
        if(type == "image" || type == "buffer" || type == "cubemap") {
            const auto name = pass.at("name").get<std::string>();
            const auto idxDst = getOrder(name);
            const auto node = newShaderNodes.at(pass.at("outputs")[0].at("id").get<std::string>());

            for(auto& input : pass.at("inputs")) {
                auto inputType = input.at("type").get<std::string>();
                if(!(inputType == "buffer" || (inputType == "cubemap" && isDynamicCubeMap(input)))) {
                    continue;
                }

                auto channel = input.at("channel").get<uint32_t>();
                auto src = newShaderNodes.at(input.at("id").get<std::string>());
                const auto idxSrc = getOrder(src->name);
                if(idxSrc < idxDst) {
                    addLink(src, node, channel, &input);
                } else {
                    addLink(getLastFrame(src), node, channel, &input);
                }
            }
        } else {
            Log(HelloImGui::LogLevel::Error, "Unsupported pass type %s", type.c_str());
        }
    }

    mShouldResetLayout = true;
    mShouldBuildPipeline = true;
}

std::string PipelineEditor::getShaderName() const {
    using namespace std::string_view_literals;
    for(auto [k, v] : mMetadata)
        if(k == "Name"sv || k == "name"sv)
            return v;
    return "untitled";
}

void PipelineEditor::updateNodeType() {
    while(true) {
        bool modified = false;
        auto sync = [&](NodeType& x, NodeType y) {
            if(x == y)
                return;
            x = y;
            modified = true;
        };

        // last frame
        for(auto& node : mNodes) {
            if(node->getClass() == NodeClass::LastFrame) {
                auto& lastFrame = dynamic_cast<EditorLastFrame&>(*node);
                if(std::find(mShaderNodes.cbegin(), mShaderNodes.cend(), lastFrame.lastFrame) != mShaderNodes.cend() &&
                   lastFrame.lastFrame->type != lastFrame.type) {
                    sync(lastFrame.type, lastFrame.lastFrame->type);
                }
            }
        }

        std::unordered_map<uintptr_t, ed::PinId> graph;
        for(auto link : mLinks) {
            assert(!graph.count(link.endPinId.Get()));
            graph.emplace(link.endPinId.Get(), link.startPinId);
        }
        for(const auto& node : mNodes) {
            // input pin
            for(auto& input : node->inputs) {
                if(auto iter = graph.find(input.id.Get()); iter != graph.cend()) {
                    sync(input.type, findPin(iter->second)->node->type);
                } else {
                    sync(input.type, NodeType::Image);
                }
            }

            // output pin
            for(auto& output : node->outputs)
                sync(output.type, node->type);
        }

        if(!modified)
            return;
    }
}

SHADERTOY_NAMESPACE_END
