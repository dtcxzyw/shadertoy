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

#include "shadertoy/Backend.hpp"
#include "shadertoy/Support.hpp"
#include <GL/glew.h>
#include <array>
#include <hello_imgui/hello_imgui.h>

SHADERTOY_NAMESPACE_BEGIN

static const char* const shaderVertexSrc = R"(
#version 450 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 texCoord;

layout (location = 0) out vec2 f_fragCoord;

void main() {
    gl_Position = vec4(pos, 1.0f, 1.0f);
    f_fragCoord = texCoord;
}

)";

static const char* const shaderPixelHeader = R"(
#version 450 core

layout (location = 0) in vec2 f_fragCoord;

layout (location = 0) out vec4 out_frag_color;

uniform vec3      iResolution;           // viewport resolution (in pixels)
uniform float     iTime;                 // shader playback time (in seconds)
uniform float     iTimeDelta;            // render time (in seconds)
uniform float     iFrameRate;            // shader frame rate
uniform int       iFrame;                // shader playback frame
uniform vec4      iMouse;                // mouse pixel coords. xy: current (if MLB down), zw: click
uniform sampler2D iChannel0;
uniform sampler2D iChannel1;
uniform sampler2D iChannel2;
uniform sampler2D iChannel3;

)";

static const char* const shaderPixelFooter = R"(
void main() {
    vec4 output_color = vec4(1.0f);
    mainImage(output_color, f_fragCoord);
    out_frag_color = output_color;
}
)";

struct Vertex final {
    ImVec2 pos;
    ImVec2 coord;
};

static void checkShaderCompileError(const GLuint shader, const std::string_view type) {
    GLint success;
    std::array<GLchar, 1024> buffer = {};
    if(type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if(!success) {
            glGetShaderInfoLog(shader, static_cast<GLsizei>(buffer.size()), nullptr, buffer.data());
            Log(HelloImGui::LogLevel::Error, "%s", buffer.data());
            throw Error{};
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success) {
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

class RenderPass final {
    GLuint mProgram;
    DoubleBufferedFB mBuffer;
    GLint mResolutionLocation;
    GLint mTimeLocation;
    GLint mTimeDeltaLocation;
    GLint mFrameRateLocation;
    GLint mFrameLocation;
    GLint mMouseLocation;
    GLint mChannelLocations[4];
    std::vector<Channel> mChannels;

public:
    RenderPass(const std::string& src, DoubleBufferedFB buffer, std::vector<Channel> channels)
        : mBuffer{ buffer }, mChannels{ std::move(channels) } {
        const auto realSrc = shaderPixelHeader + src + shaderPixelFooter;
        const auto realSrcData = realSrc.c_str();

        const auto shaderVertex = glCreateShader(GL_VERTEX_SHADER);
        auto vertGuard = scopeExit([&] { glDeleteShader(shaderVertex); });
        glShaderSource(shaderVertex, 1, &shaderVertexSrc, nullptr);
        glCompileShader(shaderVertex);
        checkShaderCompileError(shaderVertex, "VERTEX");

        const auto shaderPixel = glCreateShader(GL_FRAGMENT_SHADER);
        auto pixelGuard = scopeExit([&] { glDeleteShader(shaderPixel); });
        glShaderSource(shaderPixel, 1, &realSrcData, nullptr);
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

        auto& mChannel0Location = mChannelLocations[0];
        auto& mChannel1Location = mChannelLocations[1];
        auto& mChannel2Location = mChannelLocations[2];
        auto& mChannel3Location = mChannelLocations[3];
#define SHADERTOY_GET_UNIFORM_LOCATION(NAME) m##NAME##Location = glGetUniformLocation(mProgram, "i" #NAME)
        SHADERTOY_GET_UNIFORM_LOCATION(Resolution);
        SHADERTOY_GET_UNIFORM_LOCATION(Time);
        SHADERTOY_GET_UNIFORM_LOCATION(TimeDelta);
        SHADERTOY_GET_UNIFORM_LOCATION(FrameRate);
        SHADERTOY_GET_UNIFORM_LOCATION(Frame);
        SHADERTOY_GET_UNIFORM_LOCATION(Mouse);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel0);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel1);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel2);
        SHADERTOY_GET_UNIFORM_LOCATION(Channel3);
#undef SHADERTOY_GET_UNIFORM_LOCATION
    }
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;
    ~RenderPass() {
        glDeleteProgram(mProgram);
    }
    void render(ImVec2 frameBufferSize, const ImVec2 clipMin, const ImVec2 clipMax, ImVec2 base, const ImVec2 size,
                const ShaderToyUniform& uniform, const GLuint vao, const GLuint vbo) {
        glDisable(GL_BLEND);
        const auto buffer = mBuffer.get();
        if(buffer) {
            glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
            glDisable(GL_SCISSOR_TEST);
            buffer->bind(static_cast<uint32_t>(size.x), static_cast<uint32_t>(size.y));
            base = { 0, 0 };
            frameBufferSize = size;
        } else {
            glViewport(0, 0, static_cast<GLsizei>(frameBufferSize.x), static_cast<GLsizei>(frameBufferSize.y));
            glEnable(GL_SCISSOR_TEST);
            glScissor(static_cast<GLint>(clipMin.x), static_cast<GLint>(frameBufferSize.y - clipMax.y),
                      static_cast<GLint>(clipMax.x - clipMin.x), static_cast<GLint>(clipMax.y - clipMin.y));
        }
        glUseProgram(mProgram);
        // update vertex array
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindVertexArray(vao);
        std::array vertices{
            Vertex{ ImVec2{ base.x, base.y + size.y }, ImVec2{ 0.0, 0.0 } },              // left-bottom
            Vertex{ ImVec2{ base.x, base.y }, ImVec2{ 0.0, size.y } },                    // left-top
            Vertex{ ImVec2{ base.x + size.x, base.y }, ImVec2{ size.x, size.y } },        // right-top
            Vertex{ ImVec2{ base.x + size.x, base.y + size.y }, ImVec2{ size.x, 0.0 } },  // right-bottom
        };
        for(auto& [pos, coord] : vertices) {
            pos.x = pos.x / frameBufferSize.x * 2.0f - 1.0f;
            pos.y = 1.0f - pos.y / frameBufferSize.y * 2.0f;
        }
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);

        // update texture
        for(auto& channel : mChannels) {
            if(mChannelLocations[channel.slot] == -1)
                continue;
            glUniform1i(mChannelLocations[channel.slot], static_cast<GLint>(channel.slot));
            glActiveTexture(GL_TEXTURE0 + channel.slot);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(channel.tex.get()));
            const GLint wrapMode = [&] {
                switch(channel.wrapMode) {
                    case Channel::Wrap::Clamp:
                        return GL_CLAMP_TO_EDGE;
                    case Channel::Wrap::Repeat:
                        return GL_REPEAT;
                }
                SHADERTOY_UNREACHABLE();
            }();
            const GLint minFilter = [&] {
                switch(channel.filter) {
                    case Channel::Filter::Mipmap:
                        return GL_LINEAR_MIPMAP_LINEAR;
                    case Channel::Filter::Nearest:
                        return GL_NEAREST;
                    case Channel::Filter::Linear:
                        return GL_LINEAR;
                }
                SHADERTOY_UNREACHABLE();
            }();
            const GLint magFilter = [&] {
                switch(channel.filter) {
                    case Channel::Filter::Nearest:
                        return GL_NEAREST;
                    case Channel::Filter::Mipmap:
                        [[fallthrough]];
                    case Channel::Filter::Linear:
                        return GL_LINEAR;
                }
                SHADERTOY_UNREACHABLE();
            }();
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapMode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapMode);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
        }

        // update uniform
        if(mResolutionLocation != -1)
            glUniform3f(mResolutionLocation, size.x, size.y, 0.0f);
        if(mTimeLocation != -1)
            glUniform1f(mTimeLocation, uniform.time);
        if(mTimeDeltaLocation != -1)
            glUniform1f(mTimeDeltaLocation, uniform.timeDelta);
        if(mFrameRateLocation != -1)
            glUniform1f(mFrameRateLocation, uniform.frameRate);
        if(mFrameLocation != -1)
            glUniform1i(mFrameLocation, uniform.frame);
        if(mMouseLocation != -1)
            glUniform4f(mMouseLocation, uniform.mouse.x, uniform.mouse.y, uniform.mouse.z, uniform.mouse.w);

        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        if(buffer)
            buffer->unbind();
        glActiveTexture(GL_TEXTURE0);  // restore
    }
};

class OpenGLPipeline final : public Pipeline {
    GLuint mVAO{};
    GLuint mVBO{};
    std::vector<std::unique_ptr<FrameBuffer>> mFrameBuffers;
    std::vector<std::unique_ptr<RenderPass>> mRenderPasses;

public:
    explicit OpenGLPipeline() {
        glGenBuffers(1, &mVBO);
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);

        glGenVertexArrays(1, &mVAO);
        glBindVertexArray(mVAO);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), std::bit_cast<void*>(offsetof(Vertex, pos)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), std::bit_cast<void*>(offsetof(Vertex, coord)));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindVertexArray(0);
    }

    ~OpenGLPipeline() override {
        glDeleteVertexArrays(1, &mVAO);
        glDeleteBuffers(1, &mVBO);
    }

    FrameBuffer* createFrameBuffer() override {
        mFrameBuffers.push_back(std::make_unique<GLFrameBuffer>());
        return mFrameBuffers.back().get();
    }

    void addPass(const std::string& src, DoubleBufferedFB target, std::vector<Channel> channels) override {
        mRenderPasses.push_back(std::make_unique<RenderPass>(src, target, std::move(channels)));
    }

    void render(const ImVec2 frameBufferSize, const ImVec2 clipMin, const ImVec2 clipMax, const ImVec2 base, const ImVec2 size,
                const ShaderToyUniform& uniform) override {
        for(const auto& pass : mRenderPasses)
            pass->render(frameBufferSize, clipMin, clipMax, base, size, uniform, mVAO, mVBO);
    }
};

std::unique_ptr<Pipeline> createPipeline() {
    try {
        return std::make_unique<OpenGLPipeline>();
    } catch(const Error&) {
        return {};
    }
}

class GLTextureObject final : public TextureObject {
    GLuint mTex;
    ImVec2 mSize;

public:
    GLTextureObject(const uint32_t width, const uint32_t height, const uint32_t* data)
        : mSize{ static_cast<float>(width), static_cast<float>(height) } {
        glGenTextures(1, &mTex);
        glBindTexture(GL_TEXTURE_2D, mTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA,
                     GL_UNSIGNED_BYTE, data);  // R8G8B8A8
        glGenerateMipmap(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, GL_NONE);
    }
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

SHADERTOY_NAMESPACE_END
