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

#include "shadertoy/NodeEditor/PipelineEditor.hpp"
#include <fmt/format.h>
#include <hello_imgui/hello_imgui.h>
#include <hello_imgui/image_from_asset.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

SHADERTOY_NAMESPACE_BEGIN

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
    auto shader = spawnShader();
    shader->editor.setText(initialShader);
    auto sink = spawnRenderOutput();

    mLinks.emplace_back(nextId(), shader->Output.value().ID,  // NOLINT(bugprone-unchecked-optional-access)
                        sink->Inputs.front().ID);
    mNodes.push_back(std::move(shader));
    mNodes.push_back(std::move(sink));
}

PipelineEditor::PipelineEditor() {
    const ed::Config config;
    mCtx = ed::CreateEditor(&config);
    mHeaderBackground = HelloImGui::ImTextureIdFromAsset("BlueprintBackground.png");

    setupInitialPipeline();
}

PipelineEditor::~PipelineEditor() {
    ed::DestroyEditor(mCtx);
}

void buildNode(EditorNode& node) {
    for(auto& input : node.Inputs) {
        input.Node = &node;
        input.Kind = PinKind::Input;
    }

    if(node.Output) {
        auto& output = node.Output.value();
        output.Node = &node;
        output.Kind = PinKind::Output;
    }
}
std::unique_ptr<EditorTexture> PipelineEditor::spawnTexture() {
    return std::make_unique<EditorTexture>(nextId(), "Texture");
}
std::unique_ptr<EditorRenderOutput> PipelineEditor::spawnRenderOutput() {
    auto ret = std::make_unique<EditorRenderOutput>(nextId(), "RenderOutput");
    ret->Inputs.emplace_back(nextId(), "Input", NodeType::Image);
    buildNode(*ret);
    return ret;
}
std::unique_ptr<EditorShader> PipelineEditor::spawnShader() {
    auto ret = std::make_unique<EditorShader>(nextId(), "Shader");
    for(uint32_t idx = 0; idx < 4; ++idx) {
        ret->Inputs.emplace_back(nextId(), fmt::format("Channel{}", idx).c_str(), NodeType::Image);
    }
    ret->Output = EditorPin{ nextId(), "Output", NodeType::Image };
    buildNode(*ret);
    return ret;
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

static bool canCreateLink(const EditorPin& pin1, const EditorPin& pin2) {
    return pin1.Type == pin2.Type;
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

void PipelineEditor::renderEditor() {
    ed::Begin("##PipelineEditor", ImVec2(0.0, 0.0));
    util::BlueprintNodeBuilder builder(mHeaderBackground, 64, 64);

    auto cursorTopLeft = ImGui::GetCursorScreenPos();
    for(auto& node : mNodes) {
        builder.Begin(node->ID);
        builder.Header(node->Color);
        ImGui::Spring(0);
        if(node->rename) {
            if(ImGui::InputText("##Name", &node->Name, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsNoBlank)) {
                node->rename = false;
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
            if(mNewLinkPin && !canCreateLink(*mNewLinkPin, input) && &input != mNewLinkPin)
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

        if(node->Output.has_value()) {
            auto& output = node->Output.value();  // NOLINT(bugprone-unchecked-optional-access)
            auto alpha = ImGui::GetStyle().Alpha;
            if(mNewLinkPin && !canCreateLink(*mNewLinkPin, output) && &output != mNewLinkPin)
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
    }

    for(const auto& link : mLinks)
        ed::Link(link.ID, link.StartPinID, link.EndPinID, ImColor(255, 255, 255), 2.0f);

    if(!mOnNodeCreate) {
        if(ed::BeginCreate()) {
            ed::PinId inputPinId, outputPinId;
            if(ed::QueryNewLink(&inputPinId, &outputPinId)) {
                if(inputPinId && outputPinId) {
                    ed::RejectNewItem();
                    /*
                    if(ed::AcceptNewItem()) {
                    }
                    */
                }
            }
        }
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
                    if(id != mNodes.end())
                        mNodes.erase(id);
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
        auto node = findNode(contextNodeId);
        if(!node->rename && ImGui::MenuItem("Rename")) {
            node->rename = true;
        }
        if(ImGui::MenuItem("Delete"))
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

        std::unique_ptr<EditorNode> node = nullptr;

        ImGui::TextUnformatted("New Node");

        ImGui::Separator();
        if(ImGui::MenuItem("Texture"))
            node = spawnTexture();

        ImGui::Separator();
        if(ImGui::MenuItem("Shader"))
            node = spawnShader();

        ImGui::Separator();
        if(ImGui::MenuItem("Render Output"))
            node = spawnRenderOutput();

        if(node) {
            mOnNodeCreate = false;

            ed::SetNodePosition(node->ID, newNodePosition);

            /*
            if(auto startPin = mNewNodeLinkPin) {
                auto& pins = startPin->Kind == PinKind::Input ? node->Outputs : node->Inputs;

                for(auto& pin : pins) {
                    if(CanCreateLink(startPin, &pin)) {
                        auto endPin = &pin;
                        if(startPin->Kind == PinKind::Input)
                            std::swap(startPin, endPin);

                        m_Links.emplace_back(Link(GetNextId(), startPin->ID, endPin->ID));
                        m_Links.back().Color = GetIconColor(startPin->Type);

                        break;
                    }
                }
            }
            */
        }

        ImGui::EndPopup();
    } else
        mOnNodeCreate = false;
    ImGui::PopStyleVar();
    ed::Resume();

    ed::End();
}

void PipelineEditor::render(const std::function<void()>& build) {
    ed::SetCurrentEditor(mCtx);
    // toolbar
    if(ImGui::Button(ICON_FA_PLAY " Build")) {
        build();
    }
    ImGui::SameLine();
    if(ImGui::Button("Zoom to Context")) {
        ed::NavigateToContent();
    }

    renderEditor();
    ed::SetCurrentEditor(nullptr);
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
        // set focus
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
    }
}
std::unique_ptr<Node> EditorTexture::toSTTF() const {
    return nullptr;
}
void EditorTexture::fromSTTF(Node& node) {}

SHADERTOY_NAMESPACE_END
