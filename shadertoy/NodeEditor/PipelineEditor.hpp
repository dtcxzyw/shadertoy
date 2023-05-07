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
#include "shadertoy/ShaderToyContext.hpp"
#include <ImGuiColorTextEdit/TextEditor.h>

SHADERTOY_NAMESPACE_BEGIN

class ShaderToyEditor final {
    TextEditor mEditor;

public:
    ShaderToyEditor();
    ShaderToyEditor(const ShaderToyEditor&) = delete;
    ShaderToyEditor& operator=(const ShaderToyEditor&) = delete;

    [[nodiscard]] std::string getText() const;
    void setText(const std::string& str);
    void render(const ImVec2 size);
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
    std::vector<EditorPin> Outputs;
    ImColor Color;
    NodeType Type;
    bool rename = false;

    EditorNode(uint32_t id, std::string name, ImColor color = ImColor(255, 255, 255))
        : ID(id), Name(std::move(name)), Color(color), Type(NodeType::Image) {}
    EditorNode(const EditorNode&) = delete;
    EditorNode& operator=(const EditorNode&) = delete;
    virtual ~EditorNode() = default;

    [[nodiscard]] virtual NodeClass getClass() const noexcept = 0;
    virtual void renderContent() {}
    virtual std::unique_ptr<Node> toSTTF() const = 0;
    virtual void fromSTTF(Node& node) = 0;
};

struct EditorRenderOutput final : EditorNode {
    EditorRenderOutput(uint32_t id, std::string name) : EditorNode(id, std::move(name)) {}
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::RenderOutput;
    }
};

struct EditorShader final : EditorNode {
    ShaderToyEditor editor;
    bool isOpen = false;
    bool requestFocus = false;

    EditorShader(uint32_t id, std::string name) : EditorNode(id, std::move(name)) {}
    void renderContent() override;
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::GLSLShader;
    }
};

struct EditorLastFrame final : EditorNode {
    EditorNode* lastFrame = nullptr;
    bool openPopup = false;

    EditorLastFrame(uint32_t id, std::string name) : EditorNode(id, std::move(name)) {}
    void renderContent() override;
    void renderPopup();
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::LastFrame;
    }
};

struct EditorTexture final : EditorNode {
    std::vector<uint32_t> pixel;
    std::unique_ptr<TextureObject> textureId;

    EditorTexture(uint32_t id, std::string name) : EditorNode(id, std::move(name)) {}
    void renderContent() override;
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;

    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::Texture;
    }
};

struct EditorKeyboard final : EditorNode {
    EditorKeyboard(uint32_t id, std::string name) : EditorNode(id, std::move(name)) {}
    std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;

    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::Keyboard;
    }
};

struct EditorLink final {
    ed::LinkId ID;

    ed::PinId StartPinID;
    ed::PinId EndPinID;
    Filter filter;
    Wrap wrapMode;

    EditorLink(ed::LinkId id, ed::PinId startPinId, ed::PinId endPinId, Filter filter = Filter::Linear,
               Wrap wrapMode = Wrap::Repeat)
        : ID(id), StartPinID(startPinId), EndPinID(endPinId), filter{ filter }, wrapMode{ wrapMode } {}
};

class PipelineEditor final {
    ed::EditorContext* mCtx;
    bool mOnNodeCreate = false;
    std::uint32_t mNextId = 1;
    EditorPin* mNewNodeLinkPin = nullptr;
    EditorPin* mNewLinkPin = nullptr;
    ImTextureID mHeaderBackground = nullptr;
    std::vector<std::unique_ptr<EditorNode>> mNodes;
    std::vector<EditorLink> mLinks;
    std::vector<std::pair<std::string, std::string>> mMetadata;
    ed::NodeId contextNodeId;
    ed::LinkId contextLinkId;
    std::vector<const char*> shaderNodeNames;
    std::vector<EditorNode*> shaderNodes;
    bool shouldZoomToContent = false;
    bool shouldResetLayout = false;
    bool shouldBuildPipeline = false;
    bool openMetadataEditor = false;
    bool metadataEditorRequestFocus = false;

    uint32_t nextId();
    bool isPinLinked(ed::PinId id) const;
    EditorNode* findNode(ed::NodeId id) const;
    EditorPin* findPin(ed::PinId id) const;
    bool isUniqueName(const std::string_view& name, const EditorNode* exclude) const;
    std::string generateUniqueName(const std::string_view& base) const;
    bool canCreateLink(const EditorPin* startPin, const EditorPin* endPin);

    void setupInitialPipeline();
    void renderEditor();
    void resetLayout();
    EditorTexture& spawnTexture();
    EditorRenderOutput& spawnRenderOutput();
    EditorLastFrame& spawnLastFrame();
    EditorShader& spawnShader();
    EditorKeyboard& spawnKeyboard();
    std::unique_ptr<Pipeline> buildPipeline();

    friend struct EditorLastFrame;

public:
    PipelineEditor();
    ~PipelineEditor();
    void build(ShaderToyContext& context);
    void render(ShaderToyContext& context);
    void resetPipeline();
    void loadSTTF(const std::string& path);
    void saveSTTF(const std::string& path);
    void loadFromShaderToy(const std::string& path);

    static PipelineEditor& get();
};

SHADERTOY_NAMESPACE_END
