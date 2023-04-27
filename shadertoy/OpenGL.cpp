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
#include <GL/glew.h>
#include <array>

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
            reportError(buffer.data());
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if(!success) {
            glGetProgramInfoLog(shader, static_cast<GLsizei>(buffer.size()), nullptr, buffer.data());
            reportError(buffer.data());
        }
    }
}

class OpenGLPipeline final : public Pipeline {
    GLuint mProgram;
    GLuint mVAO{};
    GLuint mVBO{};
    GLint mResolutionLocation;
    GLint mTimeLocation;
    GLint mTimeDeltaLocation;
    GLint mFrameRateLocation;
    GLint mFrameLocation;
    GLint mMouseLocation;

public:
    explicit OpenGLPipeline(const char* pixelSrc) {
        const auto shaderVertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(shaderVertex, 1, &shaderVertexSrc, nullptr);
        glCompileShader(shaderVertex);
        checkShaderCompileError(shaderVertex, "VERTEX");

        const auto shaderPixel = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(shaderPixel, 1, &pixelSrc, nullptr);
        glCompileShader(shaderPixel);
        checkShaderCompileError(shaderPixel, "FRAGMENT");

        mProgram = glCreateProgram();
        glAttachShader(mProgram, shaderVertex);
        glAttachShader(mProgram, shaderPixel);
        glLinkProgram(mProgram);
        checkShaderCompileError(mProgram, "PROGRAM");

        glDetachShader(mProgram, shaderVertex);
        glDetachShader(mProgram, shaderPixel);
        glDeleteShader(shaderVertex);
        glDeleteShader(shaderPixel);

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

#define SHADERTOY_GET_UNIFORM_LOCATION(NAME) m##NAME##Location = glGetUniformLocation(mProgram, "i" #NAME)
        SHADERTOY_GET_UNIFORM_LOCATION(Resolution);
        SHADERTOY_GET_UNIFORM_LOCATION(Time);
        SHADERTOY_GET_UNIFORM_LOCATION(TimeDelta);
        SHADERTOY_GET_UNIFORM_LOCATION(FrameRate);
        SHADERTOY_GET_UNIFORM_LOCATION(Frame);
        SHADERTOY_GET_UNIFORM_LOCATION(Mouse);
#undef SHADERTOY_GET_UNIFORM_LOCATION
    }

    ~OpenGLPipeline() override {
        glDeleteProgram(mProgram);
        glDeleteVertexArrays(1, &mVAO);
        glDeleteBuffers(1, &mVBO);
    }

    void render(int32_t width, int32_t height, ImVec2 clipMin, ImVec2 clipMax, ImVec2 base, ImVec2 size,
                const ShaderToyUniform& uniform) override {
        glScissor(static_cast<GLint>(clipMin.x), static_cast<GLint>(static_cast<float>(height) - clipMax.y),
                  static_cast<GLint>(clipMax.x - clipMin.x), static_cast<GLint>(clipMax.y - clipMin.y));

        glUseProgram(mProgram);
        // update vertex array
        glBindBuffer(GL_ARRAY_BUFFER, mVBO);
        glBindVertexArray(mVAO);
        std::array vertices{
            Vertex{ ImVec2{ base.x, base.y + size.y }, ImVec2{ 0.0, 0.0 } },              // left-bottom
            Vertex{ ImVec2{ base.x, base.y }, ImVec2{ 0.0, size.y } },                    // left-top
            Vertex{ ImVec2{ base.x + size.x, base.y }, ImVec2{ size.x, size.y } },        // right-top
            Vertex{ ImVec2{ base.x + size.x, base.y + size.y }, ImVec2{ size.x, 0.0 } },  // right-bottom
        };
        for(auto& [pos, coord] : vertices) {
            pos.x = pos.x / static_cast<float>(width) * 2.0f - 1.0f;
            pos.y = 1.0f - pos.y / static_cast<float>(height) * 2.0f;
        }
        glBufferData(GL_ARRAY_BUFFER, 4 * sizeof(Vertex), vertices.data(), GL_STREAM_DRAW);

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
    }
};

std::unique_ptr<Pipeline> createPipeline(const std::string& src) {
    return std::make_unique<OpenGLPipeline>((shaderPixelHeader + src + shaderPixelFooter).c_str());
}

SHADERTOY_NAMESPACE_END
