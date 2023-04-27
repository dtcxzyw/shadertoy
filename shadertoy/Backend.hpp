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
#include <imgui.h>
#include <memory>
#include <string>

SHADERTOY_NAMESPACE_BEGIN

struct ShaderToyUniform final {
    float time;
    float timeDelta;
    float frameRate;
    int32_t frame;
    ImVec4 mouse;
};

class Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;
    virtual ~Pipeline() = default;

    virtual void render(int32_t width, int32_t height, ImVec2 clipMin, ImVec2 clipMax, ImVec2 base, ImVec2 size, const ShaderToyUniform& uniform) = 0;
};

std::unique_ptr<Pipeline> createPipeline(const std::string& src);

SHADERTOY_NAMESPACE_END
