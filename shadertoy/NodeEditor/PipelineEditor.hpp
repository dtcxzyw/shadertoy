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

#pragma once
#include "PipelineEditor.hpp"
#include "shadertoy/Config.hpp"
#include "shadertoy/NodeEditor/Builders.hpp"
#include "shadertoy/NodeEditor/Widgets.hpp"
#include "shadertoy/STTF.hpp"
#include "shadertoy/ShaderToyContext.hpp"

#include "shadertoy/SuppressWarningPush.hpp"

#include <ImGuiColorTextEdit/TextEditor.h>

#include "shadertoy/SuppressWarningPop.hpp"

SHADERTOY_NAMESPACE_BEGIN

class ShaderToyEditor final {
    TextEditor mEditor;

public:
    ShaderToyEditor();
    ShaderToyEditor(const ShaderToyEditor&) = delete;
    ShaderToyEditor& operator=(const ShaderToyEditor&) = delete;

    [[nodiscard]] std::string getText() const;
    void setText(const std::string& str);
    void render(ImVec2 size);
};

namespace ed = ax::NodeEditor;
using namespace ax;
using ax::Widgets::IconType;

enum class PinKind { Output, Input };

struct EditorNode;
struct EditorPin final {
    ed::PinId id;
    EditorNode* node;
    std::string name;
    NodeType type;
    PinKind kind;

    EditorPin(const ed::PinId idVal, const char* nameVal, const NodeType typeVal)
        : id(idVal), node(nullptr), name(nameVal), type(typeVal), kind(PinKind::Input) {}
};

struct EditorNode {
    ed::NodeId id;
    std::string name;
    std::vector<EditorPin> inputs;
    std::vector<EditorPin> outputs;
    ImColor color;
    NodeType type;
    bool rename = false;

    EditorNode(const uint32_t idVal, std::string nameVal, const ImColor colorVal = ImColor(255, 255, 255))
        : id(idVal), name(std::move(nameVal)), color(colorVal), type(NodeType::Image) {}
    EditorNode(const EditorNode&) = delete;
    EditorNode& operator=(const EditorNode&) = delete;
    virtual ~EditorNode() = default;

    [[nodiscard]] virtual NodeClass getClass() const noexcept = 0;
    virtual bool renderContent() {
        return false;
    }

    [[nodiscard]] virtual std::unique_ptr<Node> toSTTF() const = 0;
    virtual void fromSTTF(Node& node) = 0;
};

struct EditorRenderOutput final : EditorNode {
    EditorRenderOutput(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::RenderOutput;
    }
};

struct EditorShader final : EditorNode {
    ShaderToyEditor editor;
    bool isOpen = false;
    bool requestFocus = false;

    EditorShader(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    bool renderContent() override;
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::GLSLShader;
    }
};

struct EditorLastFrame final : EditorNode {
    EditorNode* lastFrame = nullptr;
    bool openPopup = false;
    bool editing = false;

    EditorLastFrame(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    bool renderContent() override;
    void renderPopup();
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;
    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::LastFrame;
    }
};

struct EditorTexture final : EditorNode {
    std::vector<uint32_t> pixel;
    std::unique_ptr<TextureObject> textureId;

    EditorTexture(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    bool renderContent() override;
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;

    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::Texture;
    }
};

struct EditorCubeMap final : EditorNode {
    std::vector<uint32_t> pixel;
    std::unique_ptr<TextureObject> textureId;

    EditorCubeMap(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    bool renderContent() override;
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;

    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::CubeMap;
    }
};

struct EditorVolume final : EditorNode {
    std::vector<uint8_t> pixel;
    std::unique_ptr<TextureObject> textureId;

    EditorVolume(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    bool renderContent() override;
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;

    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::Volume;
    }
};

struct EditorKeyboard final : EditorNode {
    EditorKeyboard(const uint32_t idVal, std::string nameVal) : EditorNode(idVal, std::move(nameVal)) {}
    [[nodiscard]] std::unique_ptr<Node> toSTTF() const override;
    void fromSTTF(Node& node) override;

    [[nodiscard]] NodeClass getClass() const noexcept override {
        return NodeClass::Keyboard;
    }
};

struct EditorLink final {
    ed::LinkId id;

    ed::PinId startPinId;
    ed::PinId endPinId;
    Filter filter;
    Wrap wrapMode;

    EditorLink(const ed::LinkId idVal, const ed::PinId startPinIdVal, const ed::PinId endPinIdVal,
               const Filter filterVal = Filter::Linear, const Wrap wrapModeVal = Wrap::Repeat)
        : id(idVal), startPinId(startPinIdVal), endPinId(endPinIdVal), filter{ filterVal }, wrapMode{ wrapModeVal } {}
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
    ed::NodeId mContextNodeId;
    ed::LinkId mContextLinkId;
    std::vector<const char*> mShaderNodeNames;
    std::vector<EditorNode*> mShaderNodes;
    bool mShouldZoomToContent = false;
    bool mShouldResetLayout = false;
    bool mShouldBuildPipeline = false;
    bool mOpenMetadataEditor = false;
    bool mMetadataEditorRequestFocus = false;

    uint32_t nextId();
    [[nodiscard]] bool isPinLinked(ed::PinId id) const;
    [[nodiscard]] EditorNode* findNode(ed::NodeId id) const;
    [[nodiscard]] EditorPin* findPin(ed::PinId id) const;
    [[nodiscard]] bool isUniqueName(const std::string_view& name, const EditorNode* exclude) const;
    [[nodiscard]] std::string generateUniqueName(const std::string_view& base) const;
    [[nodiscard]] bool canCreateLink(const EditorPin* startPin, const EditorPin* endPin) const;

    void setupInitialPipeline();
    void renderEditor();
    void resetLayout();
    EditorCubeMap& spawnCubeMap();
    EditorVolume& spawnVolume();
    EditorTexture& spawnTexture();
    EditorRenderOutput& spawnRenderOutput();
    EditorLastFrame& spawnLastFrame();
    EditorShader& spawnShader(NodeType type);
    EditorKeyboard& spawnKeyboard();
    void updateNodeType();
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
    [[nodiscard]] std::string getShaderName() const;

    static PipelineEditor& get();
};

SHADERTOY_NAMESPACE_END
