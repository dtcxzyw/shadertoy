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
#include "shadertoy/Backend.hpp"
#include "shadertoy/Config.hpp"
#include "shadertoy/Support.hpp"

SHADERTOY_NAMESPACE_BEGIN

class ShaderToyContext final {
    Clock::time_point mStartTime;
    Clock::time_point mPauseTime;
    float mTime{};
    float mTimeDelta{};
    int32_t mFrameCount{};
    bool mRunning;
    ImVec2 mBase;
    ImVec2 mSize;
    ImVec4 mMouse{ 0.0f, 0.0f, 0.0f, 0.0f };
    ImVec2 mFBSize;

    std::unique_ptr<Pipeline> mPipeline;

public:
    ShaderToyContext();
    void tick();
    [[nodiscard]] bool isRunning() const noexcept {
        return mRunning;
    }
    [[nodiscard]] float getTime() const noexcept {
        return mTime;
    }
    void pause();
    void resume();
    void reset();
    void render(ImVec2 base, ImVec2 size, const std::optional<ImVec4>& mouse);
    void compile(const std::string& src);
};

SHADERTOY_NAMESPACE_END
