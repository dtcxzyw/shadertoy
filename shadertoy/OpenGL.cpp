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

#include "shadertoy/Backend.hpp"
#include "shadertoy/Support.hpp"
#include <array>
#include <cmath>

#include "shadertoy/SuppressWarningPush.hpp"

#include <GL/glew.h>
#include <hello_imgui/hello_imgui.h>

#include "shadertoy/SuppressWarningPop.hpp"

SHADERTOY_NAMESPACE_BEGIN

static const char* const shaderVersionDirective = "#version 410 core\n";
static const char* const shaderCubeMapDef = "#define INTERFACE_SHADERTOY_CUBE_MAP\n";
static const char* const shaderVertexSrc = R"(
layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 texCoord;
#ifdef INTERFACE_SHADERTOY_CUBE_MAP
layout (location = 2) in vec3 point;
#endif

layout (location = 0) out vec2 f_fragCoord;
#ifdef INTERFACE_SHADERTOY_CUBE_MAP
layout (location = 1) out vec3 f_point;
#endif

void main() {
    gl_Position = vec4(pos, 0.0f, 1.0f);
    f_fragCoord = texCoord;
#ifdef INTERFACE_SHADERTOY_CUBE_MAP
    f_point = point;
#endif
}

)";

static const char* const shaderPixelHeader = R"(
layout (location = 0) in vec2 f_fragCoord;
#ifdef INTERFACE_SHADERTOY_CUBE_MAP
layout (location = 1) in vec3 f_point;
#endif

layout (location = 0) out vec4 out_frag_color;

uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform float     iTime;                 // shader playback time (in seconds)
uniform float     iTimeDelta;            // render time (in seconds)
uniform float     iFrameRate;            // shader frame rate
uniform int       iFrame;                // shader playback frame
uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
uniform vec4      iDate;                 // Year, month, day, time in seconds in .xyzw
uniform vec3 iChannelResolution[4];

#define char char_
)";

static const char* const shaderPixelFooter = R"(
void main() {
#ifdef SHADERTOY_CLAMP_OUTPUT
    out_frag_color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
#endif
    vec4 output_color = vec4(1e20f);
#ifndef INTERFACE_SHADERTOY_CUBE_MAP
    mainImage(output_color, f_fragCoord);
#else
    mainCubemap(output_color, f_fragCoord, vec3(0.0), normalize(f_point));
#endif
#ifdef SHADERTOY_CLAMP_OUTPUT
    out_frag_color = vec4(clamp(output_color.xyz, vec3(0.0f), vec3(1.0f)), 1.0f);
#else
    out_frag_color = output_color;
#endif
}
)";

struct Vertex final {
    ImVec2 pos;
    ImVec2 coord;
};

using Vec3 = std::array<float, 3>;

constexpr Vec3 cubeMapVertexPos[8] = { { -1.0f, -1.0f, -1.0f }, { -1.0f, -1.0f, 1.0f },  //
                                       { -1.0f, 1.0f, -1.0f },  { -1.0f, 1.0f, 1.0f },   //
                                       { 1.0f, -1.0f, -1.0f },  { 1.0f, -1.0f, 1.0f },   //
                                       { 1.0f, 1.0f, -1.0f },   { 1.0f, 1.0f, 1.0f } };
// left-bottom left-top right-top right-bottom
constexpr uint32_t cubeMapVertexIndex[6][4] = {
    { 4, 6, 7, 5 },  // right
    { 1, 3, 2, 0 },  // left
    { 2, 3, 7, 6 },  // top
    { 1, 0, 4, 5 },  // bottom
    { 5, 7, 3, 1 },  // back
    { 0, 2, 6, 4 }   // front
};

struct VertexCubeMap final {  // NOLINT(cppcoreguidelines-pro-type-member-init)
    ImVec2 pos;
    ImVec2 coord;
    Vec3 point;
};

static void checkShaderCompileError(const GLuint shader, const std::string_view type) {
    GLint success;
    std::vector<GLchar> buffer;
    GLint size;
    if(type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success) {
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &size);
            buffer.resize(static_cast<size_t>(size));
            glGetShaderInfoLog(shader, static_cast<GLsizei>(buffer.size()), nullptr, buffer.data());
            Log(HelloImGui::LogLevel::Error, "%s", buffer.data());
            throw Error{};
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success) {
            glGetProgramiv(shader, GL_INFO_LOG_LENGTH, &size);
            buffer.resize(static_cast<size_t>(size));
            glGetProgramInfoLog(shader, static_cast<GLsizei>(buffer.size()), nullptr, buffer.data());
            Log(HelloImGui::LogLevel::Error, "%s", buffer.data());
            throw Error{};
        }
    }
}

class GLFrameBuffer final : public FrameBuffer {
    GLuint mFBO{};
    GLuint mTexture{};
    uint32_t mWidth = 0, mHeight = 0;

public:
    GLFrameBuffer() {
        glGenFramebuffers(1, &mFBO);
        glGenTextures(1, &mTexture);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glBindTexture(GL_TEXTURE_2D, mTexture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, mTexture, 0);
        glBindTexture(GL_TEXTURE_2D, GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    }
    GLFrameBuffer(const GLFrameBuffer&) = delete;
    GLFrameBuffer(GLFrameBuffer&&) = delete;
    GLFrameBuffer& operator=(const GLFrameBuffer&) = delete;
    GLFrameBuffer& operator=(GLFrameBuffer&&) = delete;
    ~GLFrameBuffer() override {
        glDeleteFramebuffers(1, &mFBO);
        glDeleteTextures(1, &mTexture);
    }
    void bind(const uint32_t width, const uint32_t height) override {
        if(width != mWidth || height != mHeight) {
            glBindTexture(GL_TEXTURE_2D, mTexture);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA,
                         GL_FLOAT, nullptr);
            glBindTexture(GL_TEXTURE_2D, GL_NONE);
            mWidth = width;
            mHeight = height;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
    void unbind() override {
        glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    }

    [[nodiscard]] uintptr_t getTexture() const override {
        return mTexture;
    }
};

static constexpr uint32_t cubeMapRenderTargetSize = 1024;
class GLCubeMapRenderTarget final {
    GLuint mTex{};

public:
    GLCubeMapRenderTarget() {
        glGenTextures(1, &mTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTex);
        for(int32_t idx = 0; idx < 6; ++idx) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + idx, 0, GL_RGBA16F, static_cast<GLsizei>(cubeMapRenderTargetSize),
                         static_cast<GLsizei>(cubeMapRenderTargetSize), 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
        }
        glBindTexture(GL_TEXTURE_CUBE_MAP, GL_NONE);
    }
    GLCubeMapRenderTarget(const GLCubeMapRenderTarget&) = delete;
    GLCubeMapRenderTarget(GLCubeMapRenderTarget&&) = delete;
    GLCubeMapRenderTarget& operator=(const GLCubeMapRenderTarget&) = delete;
    GLCubeMapRenderTarget& operator=(GLCubeMapRenderTarget&&) = delete;
    ~GLCubeMapRenderTarget() {
        glDeleteTextures(1, &mTex);
    }
    [[nodiscard]] GLuint getTexture() const {
        return mTex;
    }
};

class GLCubeMapFrameBuffer final : public FrameBuffer {
    GLuint mFBO{};
    GLuint mTexture{};

public:
    GLCubeMapFrameBuffer(GLuint texture, uint32_t idx) : mTexture{ texture } {
        glGenFramebuffers(1, &mFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTexture);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + idx, mTexture, 0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    }
    GLCubeMapFrameBuffer(const GLCubeMapFrameBuffer&) = delete;
    GLCubeMapFrameBuffer(GLCubeMapFrameBuffer&&) = delete;
    GLCubeMapFrameBuffer& operator=(const GLCubeMapFrameBuffer&) = delete;
    GLCubeMapFrameBuffer& operator=(GLCubeMapFrameBuffer&&) = delete;
    ~GLCubeMapFrameBuffer() override {
        glDeleteFramebuffers(1, &mFBO);
    }
    void bind(const uint32_t, const uint32_t) override {
        glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    }
    void unbind() override {
        glBindFramebuffer(GL_FRAMEBUFFER, GL_NONE);
    }

    [[nodiscard]] uintptr_t getTexture() const override {
        return mTexture;
    }
};

class RenderPass final {
    GLuint mProgram;
    std::vector<DoubleBufferedFB> mBuffers;
    NodeType mType;
    GLint mLocationResolution;
    GLint mLocationTime;
    GLint mLocationTimeDelta;
    GLint mLocationFrameRate;
    GLint mLocationFrame;
    GLint mLocationMouse;
    GLint mLocationDate;
    GLint mLocationChannel[4]{};
    GLint mLocationChannelResolution[4]{};
    std::vector<Channel> mChannels;

public:
    RenderPass(const std::string& src, NodeType type, std::vector<DoubleBufferedFB> buffer, std::vector<Channel> channels,
               bool clampOutput)
        : mBuffers{ std::move(buffer) }, mType{ type }, mChannels{ std::move(channels) } {
        std::string vertexSrc = shaderVersionDirective;
        std::string pixelSrc = shaderVersionDirective;
        if(type == NodeType::CubeMap) {
            vertexSrc += shaderCubeMapDef;
            pixelSrc += shaderCubeMapDef;
        }
        vertexSrc += shaderVertexSrc;
        pixelSrc += shaderPixelHeader;
        for(auto& channel : mChannels) {
            pixelSrc += "uniform sampler";
            pixelSrc += channel.tex.type == TexType::CubeMap ? "Cube" : channel.tex.type == TexType::Tex2D ? "2D" : "3D";
            pixelSrc += " iChannel";
            pixelSrc += static_cast<char>(static_cast<uint32_t>('0') + channel.slot);
            pixelSrc += ";\n";
        }
        if(clampOutput)
            pixelSrc += "#define SHADERTOY_CLAMP_OUTPUT\n";
        pixelSrc += "#line 1\n";
        pixelSrc += src;
        pixelSrc += shaderPixelFooter;

        const auto vertexSrcData = vertexSrc.c_str();
        const auto pixelSrcData = pixelSrc.c_str();

        const auto shaderVertex = glCreateShader(GL_VERTEX_SHADER);
        auto vertGuard = scopeExit([&] { glDeleteShader(shaderVertex); });
        glShaderSource(shaderVertex, 1, &vertexSrcData, nullptr);
        glCompileShader(shaderVertex);
        checkShaderCompileError(shaderVertex, "VERTEX");

        const auto shaderPixel = glCreateShader(GL_FRAGMENT_SHADER);
        auto pixelGuard = scopeExit([&] { glDeleteShader(shaderPixel); });
        glShaderSource(shaderPixel, 1, &pixelSrcData, nullptr);
        glCompileShader(shaderPixel);
        checkShaderCompileError(shaderPixel, "PIXEL");

        mProgram = glCreateProgram();
        auto programGuard = scopeFail([&] { glDeleteProgram(mProgram); });
        glAttachShader(mProgram, shaderVertex);
        auto vertBindGuard = scopeExit([&] { glDetachShader(mProgram, shaderVertex); });
        glAttachShader(mProgram, shaderPixel);
        auto pixelBindGuard = scopeExit([&] { glDetachShader(mProgram, shaderPixel); });
        glLinkProgram(mProgram);
        checkShaderCompileError(mProgram, "PROGRAM");

        auto& mLocationChannel0 = mLocationChannel[0];
        auto& mLocationChannel1 = mLocationChannel[1];
        auto& mLocationChannel2 = mLocationChannel[2];
        auto& mLocationChannel3 = mLocationChannel[3];
#define SHADERTOY_GET_UNIFORM_LOCATION(NAME) mLocation##NAME = glGetUniformLocation(mProgram, "i" #NAME)
        SHADERTOY_GET_UNIFORM_LOCATION(Resolution);
        SHADERTOY_GET_UNIFORM_LOCATION(Time);
        SHADERTOY_GET_UNIFORM_LOCATION(TimeDelta);
        SHADERTOY_GET_UNIFORM_LOCATION(FrameRate);
        SHADERTOY_GET_UNIFORM_LOCATION(Frame);
        SHADERTOY_GET_UNIFORM_LOCATION(Mouse);
        SHADERTOY_GET_UNIFORM_LOCATION(Date);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel0);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel1);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel2);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel3);
        SHADERTOY_GET_UNIFORM_LOCATION(ChannelResolution[0]);
        SHADERTOY_GET_UNIFORM_LOCATION(ChannelResolution[1]);
        SHADERTOY_GET_UNIFORM_LOCATION(ChannelResolution[2]);
        SHADERTOY_GET_UNIFORM_LOCATION(ChannelResolution[3]);
#undef SHADERTOY_GET_UNIFORM_LOCATION
    }
    RenderPass(const RenderPass&) = delete;
    RenderPass(RenderPass&&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    RenderPass& operator=(RenderPass&&) = delete;
    ~RenderPass() {
        glDeleteProgram(mProgram);
    }
    [[nodiscard]] NodeType getType() const noexcept {
        return mType;
    }
    void render(const ImVec2 frameBufferSize, const ImVec2 clipMin, const ImVec2 clipMax, const ImVec2 canvasSize,
                const ShaderToyUniform& uniform, const GLuint vao, const GLuint vbo) {
        glDisable(GL_BLEND);
        constexpr ImVec2 cubeMapSize{ static_cast<float>(cubeMapRenderTargetSize), static_cast<float>(cubeMapRenderTargetSize) };
        const auto screenBase = clipMin;
        const auto screenSize = ImVec2{ clipMax.x - clipMin.x, clipMax.y - clipMin.y };

        for(uint32_t idx = 0; idx < mBuffers.size(); ++idx) {
            const auto buffer = mBuffers[idx].get();
            ImVec2 size, base, fbSize, uniformSize;
            if(buffer) {
                base = { 0, 0 };
                size = mType == NodeType::CubeMap ? cubeMapSize : screenSize;
                fbSize = size;
                uniformSize = mType == NodeType::CubeMap ? cubeMapSize : canvasSize;
                glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
                glDisable(GL_SCISSOR_TEST);
                buffer->bind(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
            } else {
                glViewport(0, 0, static_cast<GLsizei>(frameBufferSize.x), static_cast<GLsizei>(frameBufferSize.y));
                glEnable(GL_SCISSOR_TEST);
                glScissor(static_cast<GLint>(clipMin.x), static_cast<GLint>(frameBufferSize.y - clipMax.y),
                          static_cast<GLint>(clipMax.x - clipMin.x), static_cast<GLint>(clipMax.y - clipMin.y));
                base = screenBase;
                size = screenSize;
                fbSize = frameBufferSize;
                uniformSize = canvasSize;
            }
            glUseProgram(mProgram);
            // update vertex array
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBindVertexArray(vao);
            if(mType == NodeType::Image) {
                std::array vertices{
                    Vertex{ ImVec2{ base.x, base.y + size.y }, ImVec2{ 0.0, 0.0 } },                      // left-bottom
                    Vertex{ ImVec2{ base.x, base.y }, ImVec2{ 0.0, uniformSize.y } },                     // left-top
                    Vertex{ ImVec2{ base.x + size.x, base.y }, ImVec2{ uniformSize.x, uniformSize.y } },  // right-top
                    Vertex{ ImVec2{ base.x + size.x, base.y + size.y }, ImVec2{ uniformSize.x, 0.0 } },   // right-bottom
                };
                for(auto& [pos, coord] : vertices) {
                    pos.x = pos.x / fbSize.x * 2.0f - 1.0f;
                    pos.y = 1.0f - pos.y / fbSize.y * 2.0f;
                }
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);
            } else {
                std::array vertices{
                    VertexCubeMap{ ImVec2{ base.x, base.y + size.y }, ImVec2{ 0.0, 0.0 },
                                   cubeMapVertexPos[cubeMapVertexIndex[idx][0]] },  // left-bottom
                    VertexCubeMap{ ImVec2{ base.x, base.y }, ImVec2{ 0.0, uniformSize.y },
                                   cubeMapVertexPos[cubeMapVertexIndex[idx][1]] },  // left-top
                    VertexCubeMap{ ImVec2{ base.x + size.x, base.y }, ImVec2{ uniformSize.x, uniformSize.y },
                                   cubeMapVertexPos[cubeMapVertexIndex[idx][2]] },  // right-top
                    VertexCubeMap{ ImVec2{ base.x + size.x, base.y + size.y }, ImVec2{ uniformSize.x, 0.0 },
                                   cubeMapVertexPos[cubeMapVertexIndex[idx][3]] },  // right-bottom
                };
                for(auto& [pos, coord, point] : vertices) {
                    pos.x = pos.x / fbSize.x * 2.0f - 1.0f;
                    pos.y = 1.0f - pos.y / fbSize.y * 2.0f;
                }
                glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(VertexCubeMap), vertices.data(), GL_STREAM_DRAW);
            }

            // update texture
            for(auto& channel : mChannels) {
                if(mLocationChannelResolution[channel.slot] == -1)
                    continue;
                if(channel.tex.type != TexType::Tex3D) {
                    const auto texSize = channel.size.value_or(channel.tex.type == TexType::CubeMap ? cubeMapSize : size);
                    glUniform3f(mLocationChannelResolution[channel.slot], texSize.x, texSize.y, 1.0f);
                } else {
                    const auto x = channel.size->x;
                    glUniform3f(mLocationChannelResolution[channel.slot], x, x, x);
                }
            }
            for(auto& channel : mChannels) {
                if(mLocationChannel[channel.slot] == -1)
                    continue;
                glUniform1i(mLocationChannel[channel.slot], static_cast<GLint>(channel.slot));
                glActiveTexture(GL_TEXTURE0 + channel.slot);
                const auto type = channel.tex.type == TexType::CubeMap ? GL_TEXTURE_CUBE_MAP :
                    channel.tex.type == TexType::Tex2D                 ? GL_TEXTURE_2D :
                                                                         GL_TEXTURE_3D;
                glBindTexture(type, static_cast<GLuint>(channel.tex.get()));
                // updating
                if(glGetError() != GL_NO_ERROR)
                    continue;
                const GLint wrapMode = [&] {
                    switch(channel.wrapMode) {
                        case Wrap::Clamp:
                            return GL_CLAMP_TO_EDGE;
                        case Wrap::Repeat:
                            return GL_REPEAT;
                    }
                    SHADERTOY_UNREACHABLE();
                }();
                const GLint minFilter = [&] {
                    switch(channel.filter) {
                        case Filter::Mipmap:
                            return GL_LINEAR_MIPMAP_LINEAR;
                        case Filter::Nearest:
                            return GL_NEAREST;
                        case Filter::Linear:
                            return GL_LINEAR;
                    }
                    SHADERTOY_UNREACHABLE();
                }();
                const GLint magFilter = [&] {
                    switch(channel.filter) {
                        case Filter::Nearest:
                            return GL_NEAREST;
                        case Filter::Mipmap:
                            [[fallthrough]];
                        case Filter::Linear:
                            return GL_LINEAR;
                    }
                    SHADERTOY_UNREACHABLE();
                }();
                if(channel.filter == Filter::Mipmap)
                    glGenerateMipmap(type);
                if(channel.tex.type == TexType::Tex3D)
                    glTexParameteri(type, GL_TEXTURE_WRAP_R, wrapMode);

                glTexParameteri(type, GL_TEXTURE_WRAP_S, wrapMode);
                glTexParameteri(type, GL_TEXTURE_WRAP_T, wrapMode);
                glTexParameteri(type, GL_TEXTURE_MIN_FILTER, minFilter);
                glTexParameteri(type, GL_TEXTURE_MAG_FILTER, magFilter);
            }

            // update uniform
            if(mLocationResolution != -1)
                glUniform3f(mLocationResolution, uniformSize.x, uniformSize.y, 0.0f);
            if(mLocationTime != -1)
                glUniform1f(mLocationTime, uniform.time);
            if(mLocationTimeDelta != -1)
                glUniform1f(mLocationTimeDelta, uniform.timeDelta);
            if(mLocationFrameRate != -1)
                glUniform1f(mLocationFrameRate, uniform.frameRate);
            if(mLocationFrame != -1)
                glUniform1i(mLocationFrame, uniform.frame);
            if(mLocationMouse != -1)
                glUniform4f(mLocationMouse, uniform.mouse.x, uniform.mouse.y, uniform.mouse.z, uniform.mouse.w);
            if(mLocationDate != -1)
                glUniform4f(mLocationDate, uniform.date.x, uniform.date.y, uniform.date.z, uniform.date.w);

            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
            if(buffer)
                buffer->unbind();
        }

        glActiveTexture(GL_TEXTURE0);  // restore
    }
};

class GLTextureObject final : public TextureObject {
    GLuint mTex{};
    ImVec2 mSize;

public:
    GLTextureObject(const uint32_t width, const uint32_t height, const uint32_t* data)
        : mSize{ static_cast<float>(width), static_cast<float>(height) } {
        glGenTextures(1, &mTex);
        glBindTexture(GL_TEXTURE_2D, mTex);
        if(data) {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA,
                         GL_UNSIGNED_BYTE, data);  // R8G8B8A8
            glGenerateMipmap(GL_TEXTURE_2D);
        }
        glBindTexture(GL_TEXTURE_2D, GL_NONE);
    }
    GLTextureObject(const GLTextureObject&) = delete;
    GLTextureObject(GLTextureObject&&) = delete;
    GLTextureObject& operator=(const GLTextureObject&) = delete;
    GLTextureObject& operator=(GLTextureObject&&) = delete;
    ~GLTextureObject() override {
        glDeleteTextures(1, &mTex);
    }
    [[nodiscard]] TextureId getTexture() const override {
        return mTex;
    }
    [[nodiscard]] ImVec2 size() const override {
        return mSize;
    }
};

std::unique_ptr<TextureObject> loadTexture(uint32_t width, uint32_t height, const uint32_t* data) {
    return std::make_unique<GLTextureObject>(width, height, data);
}

class GLCubeMapObject final : public TextureObject {
    GLuint mTex{};
    ImVec2 mSize;

public:
    GLCubeMapObject(const uint32_t size, const uint32_t* data) : mSize{ static_cast<float>(size), static_cast<float>(size) } {
        glGenTextures(1, &mTex);
        glBindTexture(GL_TEXTURE_CUBE_MAP, mTex);
        assert(data);
        const auto offset = static_cast<ptrdiff_t>(size) * static_cast<ptrdiff_t>(size);
        for(int32_t idx = 0; idx < 6; ++idx) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + idx, 0, GL_RGBA, static_cast<GLsizei>(size), static_cast<GLsizei>(size),
                         0, GL_RGBA, GL_UNSIGNED_BYTE, data + idx * offset);  // R8G8B8A8
        }
        glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
        glBindTexture(GL_TEXTURE_CUBE_MAP, GL_NONE);
    }
    GLCubeMapObject(const GLCubeMapObject&) = delete;
    GLCubeMapObject(GLCubeMapObject&&) = delete;
    GLTextureObject& operator=(const GLCubeMapObject&) = delete;
    GLCubeMapObject& operator=(GLCubeMapObject&&) = delete;
    ~GLCubeMapObject() override {
        glDeleteTextures(1, &mTex);
    }
    [[nodiscard]] TextureId getTexture() const override {
        return mTex;
    }
    [[nodiscard]] ImVec2 size() const override {
        return mSize;
    }
};

std::unique_ptr<TextureObject> loadCubeMap(uint32_t size, const uint32_t* data) {
    return std::make_unique<GLCubeMapObject>(size, data);
}

class GLVolumeObject final : public TextureObject {
    GLuint mTex{};
    ImVec2 mSize;

public:
    GLVolumeObject(uint32_t size, uint32_t channels, const uint8_t* data)
        : mSize{ static_cast<float>(size), static_cast<float>(size) } {
        glGenTextures(1, &mTex);
        glBindTexture(GL_TEXTURE_3D, mTex);
        assert(data);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAX_LEVEL, static_cast<GLint>(std::log2(size)));
        GLenum format = channels == 1 ? GL_R8 : GL_RGBA;
        glTexImage3D(GL_TEXTURE_3D, 0, format, static_cast<GLsizei>(size), static_cast<GLsizei>(size), static_cast<GLsizei>(size),
                     0, format, GL_UNSIGNED_BYTE, data);  // R8G8B8A8
        glGenerateMipmap(GL_TEXTURE_3D);
        glBindTexture(GL_TEXTURE_3D, GL_NONE);
    }
    GLVolumeObject(const GLVolumeObject&) = delete;
    GLVolumeObject(GLVolumeObject&&) = delete;
    GLVolumeObject& operator=(const GLVolumeObject&) = delete;
    GLVolumeObject& operator=(GLVolumeObject&&) = delete;
    ~GLVolumeObject() override {
        glDeleteTextures(1, &mTex);
    }
    [[nodiscard]] TextureId getTexture() const override {
        return mTex;
    }
    [[nodiscard]] ImVec2 size() const override {
        return mSize;
    }
};

std::unique_ptr<TextureObject> loadVolume(uint32_t size, uint32_t channels, const uint8_t* data) {
    return std::make_unique<GLVolumeObject>(size, channels, data);
}

struct DynamicTexture final {
    std::unique_ptr<GLTextureObject> tex;
    std::vector<uint32_t> data;
    std::function<void(uint32_t*)> update;
};

class OpenGLPipeline final : public Pipeline {
    GLuint mVAOImage{};
    GLuint mVAOCubeMap{};
    GLuint mVBO{};
    std::vector<std::unique_ptr<FrameBuffer>> mFrameBuffers;
    std::vector<std::unique_ptr<GLCubeMapRenderTarget>> mCubeMapRenderTargets;
    std::vector<std::unique_ptr<RenderPass>> mRenderPasses;
    std::vector<DynamicTexture> mDynamicTextures;

public:
    explicit OpenGLPipeline() {
        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);

        glGenVertexArrays(1, &mVAOImage);
        glBindVertexArray(mVAOImage);
        glEnableVertexAttribArray(0);
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, pos)));
        glEnableVertexAttribArray(1);
        // NOLINTNEXTLINE(performance-no-int-to-ptr)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, coord)));
        glBindVertexArray(0);

        glGenVertexArrays(1, &mVAOCubeMap);
        glBindVertexArray(mVAOCubeMap);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(VertexCubeMap),
                              // NOLINTNEXTLINE(performance-no-int-to-ptr)
                              reinterpret_cast<void*>(offsetof(VertexCubeMap, pos)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(VertexCubeMap),
                              // NOLINTNEXTLINE(performance-no-int-to-ptr)
                              reinterpret_cast<void*>(offsetof(VertexCubeMap, coord)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(VertexCubeMap),
                              // NOLINTNEXTLINE(performance-no-int-to-ptr)
                              reinterpret_cast<void*>(offsetof(VertexCubeMap, point)));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    }
    OpenGLPipeline(const OpenGLPipeline&) = delete;
    OpenGLPipeline(OpenGLPipeline&&) = delete;
    OpenGLPipeline& operator=(const OpenGLPipeline&) = delete;
    OpenGLPipeline& operator=(OpenGLPipeline&&) = delete;
    ~OpenGLPipeline() override {
        glDeleteVertexArrays(1, &mVAOImage);
        glDeleteVertexArrays(1, &mVAOCubeMap);
        glDeleteBuffers(1, &mVBO);
    }

    FrameBuffer* createFrameBuffer() override {
        mFrameBuffers.push_back(std::make_unique<GLFrameBuffer>());
        return mFrameBuffers.back().get();
    }
    GLCubeMapRenderTarget* createCubeMapRenderTarget() {
        mCubeMapRenderTargets.push_back(std::make_unique<GLCubeMapRenderTarget>());
        return mCubeMapRenderTargets.back().get();
    }
    std::vector<FrameBuffer*> createCubeMapFrameBuffer() override {
        std::vector<FrameBuffer*> buffers;
        const auto target = createCubeMapRenderTarget();
        for(uint32_t idx = 0; idx < 6; ++idx) {
            mFrameBuffers.push_back(std::make_unique<GLCubeMapFrameBuffer>(target->getTexture(), idx));
            buffers.emplace_back(mFrameBuffers.back().get());
        }
        return buffers;
    }

    void addPass(const std::string& src, NodeType type, std::vector<DoubleBufferedFB> target, std::vector<Channel> channels,
                 bool clampOutput) override {
        mRenderPasses.push_back(std::make_unique<RenderPass>(src, type, std::move(target), std::move(channels), clampOutput));
    }

    void render(const ImVec2 frameBufferSize, const ImVec2 clipMin, const ImVec2 clipMax, ImVec2 size,
                const ShaderToyUniform& uniform) override {
        for(auto& [tex, data, update] : mDynamicTextures) {
            update(data.data());
            const auto texId = static_cast<GLuint>(tex->getTexture());
            glBindTexture(GL_TEXTURE_2D, texId);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(tex->size().x), static_cast<GLsizei>(tex->size().y), 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, data.data());  // R8G8B8A8
            glBindTexture(GL_TEXTURE_2D, GL_NONE);
        }
        for(const auto& pass : mRenderPasses)
            pass->render(frameBufferSize, clipMin, clipMax, size, uniform,
                         pass->getType() == NodeType::Image ? mVAOImage : mVAOCubeMap, mVBO);
    }

    TextureId createDynamicTexture(uint32_t width, uint32_t height, std::function<void(uint32_t*)> update) override {
        mDynamicTextures.push_back(DynamicTexture{ std::make_unique<GLTextureObject>(width, height, nullptr),
                                                   std::vector<uint32_t>(static_cast<size_t>(width) * height),
                                                   std::move(update) });
        return mDynamicTextures.back().tex->getTexture();
    }
};

std::unique_ptr<Pipeline> createPipeline() {
    try {
        return std::make_unique<OpenGLPipeline>();
    } catch(const Error&) {
        return {};
    }
}

SHADERTOY_NAMESPACE_END
