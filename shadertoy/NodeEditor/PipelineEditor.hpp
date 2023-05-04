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

#pragma once
#include "PipelineEditor.hpp"
#include "shadertoy/Config.hpp"
#include "shadertoy/NodeEditor/Builders.hpp"
#include "shadertoy/NodeEditor/Widgets.hpp"
#include "shadertoy/STTF.hpp"
#include <ImGuiColorTextEdit/TextEditor.h>
#include <functional>
#include <optional>

SHADERTOY_NAMESPACE_BEGIN

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

    void setText(const std::string& str) {
        mEditor.SetText(str);
    }

    void render(const ImVec2 size) {
        const auto cpos = mEditor.GetCursorPosition();
        ImGui::Text("%6d/%-6d %6d lines  %s", cpos.mLine + 1, cpos.mColumn + 1, mEditor.GetTotalLines(),
                    mEditor.IsOverwrite() ? "Ovr" : "Ins");
        mEditor.Render("TextEditor", size, false);
    }
};

namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;
using namespace ax;
using ax::Widgets::IconType;

enum class PinKind { Output, Input };

struct EditorNode;
struct EditorPin final {
    ed::PinId ID;
    EditorNode* Node;
    std::string Name;
    NodeType Type;
    PinKind Kind;

    EditorPin(ed::PinId id, const char* name, NodeType type)
        : ID(id), Node(nullptr), Name(name), Type(type), Kind(PinKind::Input) {}
};

struct EditorNode {
    ed::NodeId ID;
    std::string Name;
    std::vector<EditorPin> Inputs;
    std::optional<EditorPin> Output;
    ImColor Color;
    NodeType Type;
    bool rename = false;

    EditorNode(uint32_t id, const char* name, ImColor color = ImColor(255, 255, 255))
        : ID(id), Name(name), Color(color), Type(NodeType::Image) {}
    virtual ~EditorNode() = default;

    virtual void renderContent() {}
    virtual std::unique_ptr<Node> toSTTF() const = 0;
    virtual void fromSTTF(Node& node) = 0;
};

struct EditorRenderOutput final : EditorNode {
    EditorRenderOutput(uint32_t id, const char* name) : EditorNode(id, name) {}
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
};

struct EditorShader final : EditorNode {
    ShaderToyEditor editor;
    bool isOpen = false;

    EditorShader(uint32_t id, const char* name) : EditorNode(id, name) {}
    void renderContent() override;
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
};

struct EditorTexture final : EditorNode {
    EditorTexture(uint32_t id, const char* name) : EditorNode(id, name) {}
    void renderContent() override;
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
};

struct EditorLink final {
    ed::LinkId ID;

    ed::PinId StartPinID;
    ed::PinId EndPinID;

    EditorLink(ed::LinkId id, ed::PinId startPinId, ed::PinId endPinId) : ID(id), StartPinID(startPinId), EndPinID(endPinId) {}
};

class PipelineEditor final {
    ed::EditorContext* mCtx;
    bool mOnNodeCreate = false;
    std::uint32_t mNextId = 1;
    EditorPin* mNewNodeLinkPin = nullptr;
    EditorPin* mNewLinkPin = nullptr;
    bool mIsDirty = false;
    ImTextureID mHeaderBackground = nullptr;
    std::vector<std::unique_ptr<EditorNode>> mNodes;
    std::vector<EditorLink> mLinks;
    ed::NodeId contextNodeId;
    ed::LinkId contextLinkId;

    uint32_t nextId();
    bool isPinLinked(ed::PinId id) const;
    EditorNode* findNode(ed::NodeId id) const;

    void setupInitialPipeline();
    void renderEditor();
    std::unique_ptr<EditorTexture> spawnTexture();
    std::unique_ptr<EditorRenderOutput> spawnRenderOutput();
    std::unique_ptr<EditorShader> spawnShader();

public:
    PipelineEditor();
    ~PipelineEditor();
    void render(const std::function<void()>& build);
    void load(const std::string& path);
    void save(const std::string& path);

    [[nodiscard]] bool isDirty() const noexcept {
        return mIsDirty;
    }

    static PipelineEditor& get();
};

SHADERTOY_NAMESPACE_END
