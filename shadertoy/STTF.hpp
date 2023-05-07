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

#include "shadertoy/Config.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

SHADERTOY_NAMESPACE_BEGIN

enum class NodeClass { RenderOutput, SoundOutput, GLSLShader, Texture, LastFrame, Keyboard, Unknown };
enum class NodeType { Image, CubeMap, Sound };
enum class Filter { Mipmap, Linear, Nearest };
enum class Wrap { Clamp, Repeat };

struct Node {
    std::string name;

    Node() = default;
    Node(const Node&) = delete;
    Node(Node&&) = delete;
    Node& operator=(const Node&) = delete;
    Node& operator=(Node&&) = delete;
    virtual ~Node() = default;
    [[nodiscard]] virtual NodeClass getNodeClass() const noexcept = 0;
    [[nodiscard]] virtual NodeType getNodeType() const noexcept = 0;
};

struct RenderOutput final : Node {
    [[nodiscard]] NodeClass getNodeClass() const noexcept override {
        return NodeClass::RenderOutput;
    }
    [[nodiscard]] NodeType getNodeType() const noexcept override {
        return NodeType::Image;
    }
};

/*
class SoundOutput final : public Node {
public:
    [[nodiscard]] NodeClass getNodeClass() const noexcept override {
        return NodeClass::SoundOutput;
    }
    [[nodiscard]] NodeType getNodeType() const noexcept override {
        return NodeType::Sound;
    }
};
*/

struct GLSLShader final : Node {
    std::string source;
    NodeType nodeType;

    GLSLShader(std::string src, const NodeType type) : source{ std::move(src) }, nodeType{ type } {}
    [[nodiscard]] NodeClass getNodeClass() const noexcept override {
        return NodeClass::GLSLShader;
    }
    [[nodiscard]] NodeType getNodeType() const noexcept override {
        return nodeType;
    }
};

struct LastFrame final : Node {
    std::string refNodeName;
    Node* refNode = nullptr;
    NodeType nodeType;

    LastFrame(std::string refNodeName, const NodeType nodeType) : refNodeName{ std::move(refNodeName) }, nodeType{ nodeType } {}
    [[nodiscard]] NodeClass getNodeClass() const noexcept override {
        return NodeClass::LastFrame;
    }
    [[nodiscard]] NodeType getNodeType() const noexcept override {
        return nodeType;
    }
};

struct Texture final : Node {
    uint32_t width;
    uint32_t height;
    std::vector<uint32_t> pixel;  // R8G8B8A8

    Texture(const uint32_t w, const uint32_t h, std::vector<uint32_t> data) : width{ w }, height{ h }, pixel{ std::move(data) } {}
    [[nodiscard]] NodeClass getNodeClass() const noexcept override {
        return NodeClass::Texture;
    }
    [[nodiscard]] NodeType getNodeType() const noexcept override {
        return NodeType::Image;
    }
};

struct Keyboard final : Node {
    [[nodiscard]] NodeClass getNodeClass() const noexcept override {
        return NodeClass::Keyboard;
    }
    [[nodiscard]] NodeType getNodeType() const noexcept override {
        return NodeType::Image;
    }
};

struct Link final {
    Node* start;
    Node* end;
    Filter filter;
    Wrap wrapMode;
    uint32_t slot;
};

struct ShaderToyTransmissionFormat final {
    using Metadata = std::unordered_map<std::string, std::string>;
    Metadata metadata;
    std::vector<std::unique_ptr<Node>> nodes;
    std::vector<Link> links;

    void load(const std::string& filePath);
    void save(const std::string& filePath) const;
};

SHADERTOY_NAMESPACE_END
