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
#include <hello_imgui/hello_imgui.h>
#include <memory>
#include <string>

SHADERTOY_NAMESPACE_BEGIN

using TextureId = uintptr_t;
template <typename T>
struct DoubleBuffered final {
    T t1, t2;

    explicit DoubleBuffered(T t) : t1{ t }, t2{ t } {}
    explicit DoubleBuffered(T t1, T t2) : t1{ t1 }, t2{ t2 } {}
    T get() {
        std::swap(t1, t2);
        return t1;
    }
};

using DoubleBufferedTex = DoubleBuffered<TextureId>;

struct ShaderToyUniform final {
    float time;
    float timeDelta;
    float frameRate;
    int32_t frame;
    ImVec4 mouse;
};

class TextureObject {
public:
    TextureObject() = default;
    virtual ~TextureObject() = default;
    [[nodiscard]] virtual TextureId getTexture() const = 0;
    [[nodiscard]] virtual ImVec2 size() const = 0;
};

class FrameBuffer {
public:
    FrameBuffer() = default;
    virtual ~FrameBuffer() = default;
    virtual void bind(uint32_t width, uint32_t height) = 0;
    virtual void unbind() = 0;
    [[nodiscard]] virtual TextureId getTexture() const = 0;
};
using DoubleBufferedFB = DoubleBuffered<FrameBuffer*>;

struct Channel final {
    uint32_t slot;
    DoubleBufferedTex tex;
    enum class Filter { Mipmap, Nearest, Linear } filter;
    enum class Wrap { Clamp, Repeat } wrapMode;
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
    virtual void addPass(const std::string& src, DoubleBufferedFB target, std::vector<Channel> channels) = 0;
    virtual void render(ImVec2 frameBufferSize, ImVec2 clipMin, ImVec2 clipMax, ImVec2 base, ImVec2 size,
                        const ShaderToyUniform& uniform) = 0;
};

std::unique_ptr<TextureObject> loadTexture(uint32_t width, uint32_t height, const uint32_t* data);
std::unique_ptr<Pipeline> createPipeline();

SHADERTOY_NAMESPACE_END
