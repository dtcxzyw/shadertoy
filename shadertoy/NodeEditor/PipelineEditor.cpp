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

#define IMGUI_DEFINE_MATH_OPERATORS
#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "shadertoy/NodeEditor/PipelineEditor.hpp"
#include <fmt/format.h>
#include <hello_imgui/hello_imgui.h>
#include <hello_imgui/image_from_asset.h>
#include <httplib.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <nfd.h>
#include <queue>
#include <stb_image.h>

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
    auto& shader = spawnShader();
    shader.editor.setText(initialShader);
    auto& sink = spawnRenderOutput();

    mLinks.emplace_back(nextId(), shader.Outputs.front().ID, sink.Inputs.front().ID);
}

PipelineEditor::PipelineEditor() {
    const ed::Config config;
    mCtx = ed::CreateEditor(&config);
    mHeaderBackground = HelloImGui::ImTextureIdFromAsset("BlueprintBackground.png");

    setupInitialPipeline();

    ed::SetCurrentEditor(mCtx);
    resetLayout();
    ed::SetCurrentEditor(nullptr);
}

PipelineEditor::~PipelineEditor() {
    ed::DestroyEditor(mCtx);
}

void PipelineEditor::resetLayout() {
    // TODO: set node position for DAG
    ed::NavigateToContent();
}

bool PipelineEditor::isUniqueName(const std::string_view& name, const EditorNode* exclude) const {
    for(auto& node : mNodes) {
        if(node.get() == exclude)
            continue;
        if(node->Name == name)
            return false;
    }
    return true;
}
std::string PipelineEditor::generateUniqueName(const std::string_view& base) const {
    if(isUniqueName(base, nullptr))
        return { base.data(), base.size() };
    for(uint32_t idx = 1;; ++idx) {
        if(const auto str = fmt::format("{}{}", base, idx); isUniqueName(str, nullptr)) {
            return str;
        }
    }
}

template <typename T>
static auto& buildNode(std::vector<std::unique_ptr<EditorNode>>& nodes, std::unique_ptr<T> node) {
    for(auto& input : node->Inputs) {
        input.Node = node.get();
        input.Kind = PinKind::Input;
    }

    for(auto& output : node->Outputs) {
        output.Node = node.get();
        output.Kind = PinKind::Output;
    }
    auto& ref = *node;
    nodes.push_back(std::move(node));
    return ref;
}
EditorTexture& PipelineEditor::spawnTexture() {
    auto ret = std::make_unique<EditorTexture>(nextId(), generateUniqueName("Texture"));
    ret->Outputs.emplace_back(nextId(), "Output", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorRenderOutput& PipelineEditor::spawnRenderOutput() {
    auto ret = std::make_unique<EditorRenderOutput>(nextId(), generateUniqueName("RenderOutput"));
    ret->Inputs.emplace_back(nextId(), "Input", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorLastFrame& PipelineEditor::spawnLastFrame() {
    auto ret = std::make_unique<EditorLastFrame>(nextId(), generateUniqueName("LastFrame"));
    ret->Outputs.emplace_back(nextId(), "Output", NodeType::Image);
    return buildNode(mNodes, std::move(ret));
}
EditorShader& PipelineEditor::spawnShader() {
    auto ret = std::make_unique<EditorShader>(nextId(), generateUniqueName("Shader"));
    for(uint32_t idx = 0; idx < 4; ++idx) {
        ret->Inputs.emplace_back(nextId(), fmt::format("Channel{}", idx).c_str(), NodeType::Image);
    }
    ret->Outputs.emplace_back(nextId(), "Output", NodeType::Image);
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
    IconType iconType = iconType = IconType::Square;
    ImColor color = getIconColor(pin.Type);
    color.Value.w = static_cast<float>(alpha) / 255.0f;
    switch(pin.Type) {
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

    ax::Widgets::Icon(ImVec2(24, 24), iconType, connected, color, ImColor(32, 32, 32, alpha));
}

bool PipelineEditor::canCreateLink(const EditorPin* startPin, const EditorPin* endPin) {
    if(endPin == startPin) {
        return false;
    }
    if(endPin->Kind == startPin->Kind) {
        return false;
    }
    if(endPin->Type != startPin->Type) {
        return false;
    }
    if(endPin->Node == startPin->Node) {
        return false;
    }
    if(isPinLinked(endPin->ID)) {
        return false;
    }
    return true;
}

bool PipelineEditor::isPinLinked(ed::PinId id) const {
    if(!id)
        return false;

    for(auto& link : mLinks)
        if(link.StartPinID == id || link.EndPinID == id)
            return true;

    return false;
}

EditorNode* PipelineEditor::findNode(ed::NodeId id) const {
    if(!id)
        return nullptr;

    for(auto& node : mNodes)
        if(node->ID == id)
            return node.get();

    return nullptr;
}

EditorPin* PipelineEditor::findPin(ed::PinId id) const {
    if(!id)
        return nullptr;

    for(auto& node : mNodes) {
        for(auto& pin : node->Inputs)
            if(pin.ID == id)
                return &pin;

        for(auto& pin : node->Outputs)
            if(pin.ID == id)
                return &pin;
    }

    return nullptr;
}

void PipelineEditor::renderEditor() {
    ed::Begin("##PipelineEditor", ImVec2(0.0, 0.0));
    util::BlueprintNodeBuilder builder(mHeaderBackground, 64, 64);

    const auto cursorTopLeft = ImGui::GetCursorScreenPos();

    shaderNodeNames.clear();
    shaderNodes.clear();
    const EditorNode* directRenderNode = nullptr;
    for(const auto& link : mLinks) {
        const auto u = findPin(link.StartPinID);
        const auto v = findPin(link.EndPinID);
        if(v->Node->getClass() == NodeClass::RenderOutput && u->Node->getClass() == NodeClass::GLSLShader) {
            directRenderNode = u->Node;
        }
    }
    for(auto& node : mNodes) {
        if(node->getClass() == NodeClass::GLSLShader) {
            if(node.get() == directRenderNode)
                continue;
            shaderNodeNames.push_back(node->Name.c_str());
            shaderNodes.push_back(node.get());
        }
    }

    for(auto& node : mNodes) {
        builder.Begin(node->ID);
        builder.Header(node->Color);
        ImGui::Spring(0);
        if(node->rename) {
            if(ImGui::InputText("##Name", &node->Name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsNoBlank)) {
                if(isUniqueName(node->Name, node.get())) {
                    node->rename = false;
                } else {
                    HelloImGui::Log(HelloImGui::LogLevel::Error, "Please specify a unique name for this node");
                }
            }
        } else
            ImGui::TextUnformatted(node->Name.c_str());
        ImGui::Spring(1);
        ImGui::Dummy(ImVec2(0, 28));
        ImGui::Spring(0);
        builder.EndHeader();

        constexpr auto disabledAlphaScale = 48.0f / 255.0f;

        for(auto& input : node->Inputs) {
            auto alpha = ImGui::GetStyle().Alpha;
            if(mNewLinkPin && !canCreateLink(mNewLinkPin, &input) && &input != mNewLinkPin)
                alpha *= disabledAlphaScale;

            builder.Input(input.ID);
            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            drawPinIcon(input, isPinLinked(input.ID), static_cast<int>(alpha * 255));
            ImGui::Spring(0);
            if(!input.Name.empty()) {
                ImGui::TextUnformatted(input.Name.c_str());
                ImGui::Spring(0);
            }
            ImGui::PopStyleVar();
            builder.EndInput();
        }

        for(auto& output : node->Outputs) {
            auto alpha = ImGui::GetStyle().Alpha;
            if(mNewLinkPin && !canCreateLink(mNewLinkPin, &output) && &output != mNewLinkPin)
                alpha *= disabledAlphaScale;

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
            builder.Output(output.ID);
            if(!output.Name.empty()) {
                ImGui::Spring(0);
                ImGui::TextUnformatted(output.Name.c_str());
            }
            node->renderContent();
            ImGui::Spring(0);
            drawPinIcon(output, isPinLinked(output.ID), static_cast<int>(alpha * 255));
            ImGui::PopStyleVar();
            builder.EndOutput();
        }

        builder.End();

        if(node->getClass() == NodeClass::LastFrame) {
            dynamic_cast<EditorLastFrame*>(node.get())->renderPopup();
        }
    }

    for(const auto& link : mLinks)
        ed::Link(link.ID, link.StartPinID, link.EndPinID, ImColor(255, 255, 255), 2.0f);

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

                if(startPin->Kind == PinKind::Input) {
                    std::swap(startPin, endPin);
                    std::swap(startPinId, endPinId);
                }

                if(startPin && endPin) {
                    if(endPin == startPin) {
                        ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                    } else if(endPin->Kind == startPin->Kind) {
                        showLabel("x Incompatible Pin Kind", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 0, 0), 2.0f);
                    } else if(endPin->Type != startPin->Type) {
                        showLabel("x Incompatible Pin Type", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                    } else if(endPin->Node == startPin->Node) {
                        showLabel("x Self Loop", ImColor(45, 32, 32, 180));
                        ed::RejectNewItem(ImColor(255, 128, 128), 1.0f);
                    } else if(isPinLinked(endPin->ID)) {
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
                    auto id =
                        std::find_if(mLinks.begin(), mLinks.end(), [linkId](EditorLink& link) { return link.ID == linkId; });
                    if(id != mLinks.end())
                        mLinks.erase(id);
                }
            }

            ed::NodeId nodeId = 0;
            while(ed::QueryDeletedNode(&nodeId)) {
                if(ed::AcceptDeletedItem()) {
                    auto id = std::find_if(mNodes.begin(), mNodes.end(),
                                           [nodeId](const std::unique_ptr<EditorNode>& node) { return node->ID == nodeId; });

                    if(id != mNodes.end()) {
                        std::erase_if(mLinks, [&](auto& link) {
                            auto u = findPin(link.StartPinID);
                            auto v = findPin(link.EndPinID);
                            return (u->Node == id->get() || v->Node == id->get());
                        });
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

    if(ed::ShowNodeContextMenu(&contextNodeId))
        ImGui::OpenPopup("Node Context Menu");
    else if(ed::ShowLinkContextMenu(&contextLinkId))
        ImGui::OpenPopup("Link Context Menu");
    else if(ed::ShowBackgroundContextMenu()) {
        ImGui::OpenPopup("Create New Node");
        mNewNodeLinkPin = nullptr;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    if(ImGui::BeginPopup("Node Context Menu")) {
        const auto node = findNode(contextNodeId);
        if(!node->rename && ImGui::MenuItem("Rename")) {
            node->rename = true;
        }
        if(node->getClass() != NodeClass::RenderOutput && ImGui::MenuItem("Delete"))
            ed::DeleteNode(contextNodeId);
        ImGui::EndPopup();
    }

    if(ImGui::BeginPopup("Link Context Menu")) {
        if(ImGui::MenuItem("Delete"))
            ed::DeleteLink(contextLinkId);
        ImGui::EndPopup();
    }
    if(ImGui::BeginPopup("Create New Node")) {
        const auto newNodePosition = openPopupPosition;

        EditorNode* node = nullptr;

        ImGui::TextUnformatted("New Node");

        ImGui::Separator();
        if(ImGui::MenuItem("Texture"))
            node = &spawnTexture();
        if(ImGui::MenuItem("LastFrame"))
            node = &spawnLastFrame();

        ImGui::Separator();
        if(ImGui::MenuItem("Shader"))
            node = &spawnShader();

        ImGui::Separator();
        auto hasRenderOutput = [&] {
            for(auto& it : mNodes) {
                if(it->getClass() == NodeClass::RenderOutput)
                    return true;
            }
            return false;
        };
        if(!hasRenderOutput() && ImGui::MenuItem("Render Output"))
            node = &spawnRenderOutput();

        if(node) {
            mOnNodeCreate = false;

            ed::SetNodePosition(node->ID, newNodePosition);

            if(auto startPin = mNewNodeLinkPin) {
                auto& pins = startPin->Kind == PinKind::Input ? node->Outputs : node->Inputs;

                for(auto& pin : pins) {
                    auto endPin = &pin;
                    if(startPin->Kind == PinKind::Input)
                        std::swap(startPin, endPin);
                    if(canCreateLink(startPin, endPin)) {
                        mLinks.emplace_back(nextId(), startPin->ID, endPin->ID);
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

std::unique_ptr<Pipeline> PipelineEditor::buildPipeline() {
    std::unordered_map<EditorNode*, std::vector<std::pair<EditorNode*, uint32_t>>> graph;
    EditorNode* directRenderNode = nullptr;
    std::unordered_map<EditorNode*, uint32_t> degree;
    EditorNode* sinkNode = nullptr;
    for(auto& link : mLinks) {
        auto u = findPin(link.StartPinID);
        auto v = findPin(link.EndPinID);
        auto idx = static_cast<uint32_t>(v - v->Node->Inputs.data());
        graph[v->Node].emplace_back(u->Node, idx);
        ++degree[u->Node];
        if(v->Node->getClass() == NodeClass::RenderOutput && u->Node->getClass() == NodeClass::GLSLShader) {
            sinkNode = v->Node;
            directRenderNode = u->Node;
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
    while(!q.empty()) {
        auto u = q.front();
        q.pop();
        visited.insert(u);
        order.push_back(u);

        if(auto it = graph.find(u); it != graph.cend()) {
            for(auto [v, idx] : it->second) {
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
    std::unordered_map<EditorNode*, DoubleBufferedFB> frameBufferMap;
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
            DoubleBufferedFB frameBuffer{ nullptr };
            if(requireDoubleBuffer.count(node)) {
                auto t1 = pipeline->createFrameBuffer();
                auto t2 = pipeline->createFrameBuffer();
                frameBuffer = DoubleBufferedFB{ t1, t2 };
            } else if(node != directRenderNode) {
                auto t = pipeline->createFrameBuffer();
                frameBuffer = DoubleBufferedFB{ t };
            }
            frameBufferMap.emplace(node, frameBuffer);
        }
    }

    for(auto node : order) {
        switch(node->getClass()) {
            case NodeClass::GLSLShader: {
                auto target = frameBufferMap.at(node);
                std::vector<Channel> channels;
                if(auto it = graph.find(node); it != graph.cend()) {
                    for(auto [v, idx] : it->second) {
                        channels.emplace_back(idx, textureMap.at(v), Channel::Filter::Linear,
                                              Channel::Wrap::Repeat);  // TODO: filter & wrap mode
                    }
                }
                pipeline->addPass(dynamic_cast<EditorShader*>(node)->editor.getText(), target, std::move(channels));
                if(target.t1)
                    textureMap.emplace(node, DoubleBufferedTex{ target.t1->getTexture(), target.t2->getTexture() });
                break;
            }
            case NodeClass::LastFrame: {
                auto target = frameBufferMap.at(dynamic_cast<EditorLastFrame*>(node)->lastFrame);
                assert(target.t1 && target.t2);
                textureMap.emplace(node, DoubleBufferedTex{ target.t2->getTexture(), target.t1->getTexture() });
                break;
            }
            case NodeClass::RenderOutput: {
                break;
            }
            case NodeClass::Texture: {
                textureMap.emplace(node, DoubleBufferedTex{ dynamic_cast<EditorTexture*>(node)->textureId->getTexture() });
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
    if(!ImGui::Begin("Editor", nullptr)) {
        ImGui::End();
        return;
    }

    if(ImGui::BeginTabBar("##EditorTabBar", ImGuiTabBarFlags_Reorderable)) {
        // pipeline editor
        if(ImGui::BeginTabItem("Pipeline", nullptr, ImGuiTabItemFlags_NoReorder)) {

            ed::SetCurrentEditor(mCtx);
            // toolbar
            if(!context.isValid() || ImGui::Button(ICON_FA_PLAY " Build")) {
                build(context);
            }
            ImGui::SameLine();
            if(ImGui::Button("Zoom to Context")) {
                ed::NavigateToContent();
            }
            /*
            // TODO: Layout
            ImGui::SameLine();
            if(ImGui::Button("Reset layout")) {
                resetLayout();
            }
            */

            renderEditor();
            ed::SetCurrentEditor(nullptr);
            ImGui::EndTabItem();
        }

        // source editor
        for(auto& node : mNodes) {
            if(const auto shader = dynamic_cast<EditorShader*>(node.get())) {
                if(shader->isOpen &&
                   ImGui::BeginTabItem(shader->Name.c_str(), &shader->isOpen,
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
}

PipelineEditor& PipelineEditor::get() {
    static PipelineEditor instance;
    return instance;
}

std::unique_ptr<Node> EditorRenderOutput::toSTTF() const {
    return std::make_unique<RenderOutput>();
}
void EditorRenderOutput::fromSTTF(Node& node) {
    Type = node.getNodeType();
}

void EditorShader::renderContent() {
    if(ImGui::Button(ICON_FA_EDIT " Edit")) {
        isOpen = true;
        requestFocus = true;
    }
}
std::unique_ptr<Node> EditorShader::toSTTF() const {
    return std::make_unique<GLSLShader>(editor.getText(), Type);
}
void EditorShader::fromSTTF(Node& node) {
    const auto& shader = dynamic_cast<GLSLShader&>(node);
    editor.setText(shader.source);
}

void EditorTexture::renderContent() {
    if(ImGui::Button(ICON_FA_FILE_IMAGE " Update")) {
        [&] {
            nfdchar_t* path;
            if(NFD_OpenDialog("jpg,jpeg;bmp;png;tga;tiff", nullptr, &path) == NFD_OKAY) {
                HelloImGui::Log(HelloImGui::LogLevel::Info, "Loading image %s", path);
                stbi_set_flip_vertically_on_load(true);
                int width, height, channels;
                const auto ptr = stbi_load(path, &width, &height, &channels, 4);
                if(!ptr) {
                    HelloImGui::Log(HelloImGui::LogLevel::Info, "Failed to load image %s: %s", path, stbi_failure_reason());
                    return;
                }
                auto guard = scopeExit([ptr] { stbi_image_free(ptr); });
                const auto begin = std::bit_cast<const uint32_t*>(ptr);
                const auto end = begin + static_cast<ptrdiff_t>(width) * height;
                pixel = std::vector<uint32_t>{ begin, end };
                textureId = loadTexture(width, height, pixel.data());
            }
        }();
    }

    if(textureId) {
        ImGui::Image(std::bit_cast<ImTextureID>(textureId->getTexture()), ImVec2{ 64, 64 }, ImVec2{ 0, 1 }, ImVec2{ 1, 0 });
    }
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

// See also https://github.com/thedmd/imgui-node-editor/issues/48
void EditorLastFrame::renderContent() {
    const auto& editor = PipelineEditor::get();
    auto& selectables = editor.shaderNodes;
    if(!std::count(selectables.cbegin(), selectables.cend(), lastFrame))
        lastFrame = nullptr;
    if(ImGui::Button(lastFrame ? lastFrame->Name.c_str() : "<Select One>")) {
        openPopup = true;
    }
}
void EditorLastFrame::renderPopup() {
    const auto& editor = PipelineEditor::get();
    const auto& names = editor.shaderNodeNames;
    const auto& nodes = editor.shaderNodes;

    ed::Suspend();
    if(openPopup) {
        ImGui::OpenPopup("popup_button");
        openPopup = false;
    }

    if(ImGui::BeginPopup("popup_button")) {
        lastFrame = nullptr;
        ImGui::BeginChild("popup_scroller", ImVec2(100, 100), true, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        for(uint32_t idx = 0; idx < names.size(); ++idx) {
            if(ImGui::Button(names[idx])) {
                lastFrame = nodes[idx];
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndChild();
        ImGui::EndPopup();
    }
    ed::Resume();
}
std::unique_ptr<Node> EditorLastFrame::toSTTF() const {
    return std::make_unique<LastFrame>(lastFrame->Name, Type);
}
void EditorLastFrame::fromSTTF(Node&) {
    // should be fixed by post processing
}
void PipelineEditor::loadSTTF(const std::string& path) {}
void PipelineEditor::saveSTTF(const std::string& path) {}
void PipelineEditor::loadFromShaderToy(const std::string& path) {
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
    // TODO: parse json
}

SHADERTOY_NAMESPACE_END
