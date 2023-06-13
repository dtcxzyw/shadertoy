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
#include "STTF.hpp"
#include "shadertoy/Config.hpp"
#include <memory>
#include <string>

#include "shadertoy/SuppressWarningPush.hpp"

#include <hello_imgui/hello_imgui.h>

#include "shadertoy/SuppressWarningPop.hpp"

SHADERTOY_NAMESPACE_BEGIN

using TextureId = uintptr_t;
struct DoubleBufferedTex final {
    TextureId t1, t2;
    bool isCube;

    explicit DoubleBufferedTex(TextureId t, bool cube) : t1{ t }, t2{ t }, isCube{ cube } {}
    explicit DoubleBufferedTex(TextureId t1Val, TextureId t2Val, bool cube) : t1{ t1Val }, t2{ t2Val }, isCube{ cube } {}
    TextureId get() {
        std::swap(t1, t2);
        return t1;
    }
};

struct ShaderToyUniform final {
    float time{};
    float timeDelta{};
    float frameRate{};
    int32_t frame{};
    ImVec4 mouse;
    ImVec4 date;
};

class TextureObject {
public:
    TextureObject() = default;
    TextureObject(const TextureObject&) = delete;
    TextureObject(TextureObject&&) = delete;
    TextureObject& operator=(const TextureObject&) = delete;
    TextureObject& operator=(TextureObject&&) = delete;
    virtual ~TextureObject() = default;
    [[nodiscard]] virtual TextureId getTexture() const = 0;
    [[nodiscard]] virtual ImVec2 size() const = 0;
};

class FrameBuffer {
public:
    FrameBuffer() = default;
    FrameBuffer(const FrameBuffer&) = delete;
    FrameBuffer(FrameBuffer&&) = delete;
    FrameBuffer& operator=(const FrameBuffer&) = delete;
    FrameBuffer& operator=(FrameBuffer&&) = delete;
    virtual ~FrameBuffer() = default;
    virtual void bind(uint32_t width, uint32_t height) = 0;
    virtual void unbind() = 0;
    [[nodiscard]] virtual TextureId getTexture() const = 0;
};
struct DoubleBufferedFB final {
    FrameBuffer *t1, *t2;

    explicit DoubleBufferedFB(FrameBuffer* t) : t1{ t }, t2{ t } {}
    explicit DoubleBufferedFB(FrameBuffer* t1Val, FrameBuffer* t2Val) : t1{ t1Val }, t2{ t2Val } {}
    FrameBuffer* get() {
        std::swap(t1, t2);
        return t1;
    }
};

struct Channel final {  // NOLINT(cppcoreguidelines-pro-type-member-init)
    uint32_t slot;
    DoubleBufferedTex tex;
    Filter filter;
    Wrap wrapMode;
    std::optional<ImVec2> size;
};

class Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;
    virtual ~Pipeline() = default;

    virtual FrameBuffer* createFrameBuffer() = 0;
    virtual std::vector<FrameBuffer*> createCubeMapFrameBuffer() = 0;
    virtual void addPass(const std::string& src, NodeType type, std::vector<DoubleBufferedFB> target,
                         std::vector<Channel> channels) = 0;
    virtual void render(ImVec2 frameBufferSize, ImVec2 clipMin, ImVec2 clipMax, ImVec2 base, ImVec2 size,
                        const ShaderToyUniform& uniform) = 0;
    virtual TextureId createDynamicTexture(uint32_t width, uint32_t height, std::function<void(uint32_t*)> update) = 0;
};

std::unique_ptr<TextureObject> loadTexture(uint32_t width, uint32_t height, const uint32_t* data);
std::unique_ptr<TextureObject> loadCubeMap(uint32_t size, const uint32_t* data);
std::unique_ptr<Pipeline> createPipeline();

SHADERTOY_NAMESPACE_END
