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

#define IMGUI_DEFINE_MATH_OPERATORS
#include "shadertoy/ShaderToyContext.hpp"
#include <cassert>
#include <cmath>
#include <ctime>

#include "shadertoy/SuppressWarningPush.hpp"

#include <imgui.h>

#include "shadertoy/SuppressWarningPop.hpp"

SHADERTOY_NAMESPACE_BEGIN

ShaderToyContext::ShaderToyContext() : mRunning{ true } {
    reset();
}
void ShaderToyContext::tick() {
    if(!mRunning)
        return;
    const auto now = SystemClock::now();
    const auto time = static_cast<float>(
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - mStartTime).count()) * 1e-9);
    const auto timeScale = std::exp2(mTimeScale);
    mTimeDelta = (time - mTime) * timeScale;
    mTime = time * timeScale;
    ++mFrameCount;
    mFrameRate = ImGui::GetIO().Framerate;

    const auto current = SystemClock::to_time_t(now);
    const auto tm = std::localtime(&current);  // NOLINT(concurrency-mt-unsafe)
    mDate = { static_cast<float>(tm->tm_year + 1900 - 1), static_cast<float>(tm->tm_mon), static_cast<float>(tm->tm_mday),
              static_cast<float>(tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec) +
                  static_cast<float>(now.time_since_epoch().count() % SystemClock::period::den) /
                      static_cast<float>(SystemClock::period::den) };
}
void ShaderToyContext::pause() {
    assert(mRunning);
    mRunning = false;
    mTimeDelta = 0.0f;
    mPauseTime = SystemClock::now();
}
void ShaderToyContext::resume() {
    assert(!mRunning);
    mRunning = true;
    if(mTime == 0.0f)
        mStartTime = SystemClock::now();
    else
        mStartTime += SystemClock::now() - mPauseTime;
}
void ShaderToyContext::reset() {
    mStartTime = SystemClock::now();
    mTime = mTimeDelta = mTimeScale = 0.0f;
    mFrameCount = 0;
}
void ShaderToyContext::render(const ImVec2 base, const ImVec2 size, const std::optional<ImVec4>& mouse) {
    auto* drawList = ImGui::GetWindowDrawList();
    mBase = base;
    mSize = size;
    // Please see also https://shadertoyunofficial.wordpress.com/2016/07/20/special-shadertoy-features/
    if(mouse) {
        const auto m = mouse.value();
        mMouse.x = m.x;
        mMouse.y = m.y;
        if(m.w > 0.0f) {  // just clicked
            mMouse.z = mMouse.x;
            mMouse.w = mMouse.y;
        } else {
            mMouse.w = -std::fabs(mMouse.w);
        }
    } else {
        mMouse.z = -std::fabs(mMouse.z);
        mMouse.w = -std::fabs(mMouse.w);
    }

    if(mPipeline) {
        drawList->AddCallback(
            [](const ImDrawList*, const ImDrawCmd* cmd) {
                const auto drawData = ImGui::GetDrawData();
                const ImVec2 fbSize = drawData->DisplaySize * drawData->FramebufferScale;
                const ImVec2 clipOff = drawData->DisplayPos;
                const ImVec2 clipScale = drawData->FramebufferScale;

                const auto ctx = static_cast<ShaderToyContext*>(cmd->UserCallbackData);
                const ImVec2 clipMin((cmd->ClipRect.x - clipOff.x) * clipScale.x, (cmd->ClipRect.y - clipOff.y) * clipScale.y);
                const ImVec2 clipMax((cmd->ClipRect.z - clipOff.x) * clipScale.x, (cmd->ClipRect.w - clipOff.y) * clipScale.y);
                if(clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    return;
                ctx->mBound = { clipMin.x, clipMin.y, clipMax.x, clipMax.y };
                ctx->mPipeline->render(
                    fbSize, clipMin, clipMax, ctx->mSize,
                    { ctx->mTime, ctx->mTimeDelta, ctx->mFrameRate, ctx->mFrameCount, ctx->mMouse, ctx->mDate });
            },
            this);
        drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    } else
        drawList->AddRect(mBase, ImVec2{ mBase.x + mSize.x, mBase.y + mSize.y }, IM_COL32(255, 255, 0, 255));
}
void ShaderToyContext::reset(std::unique_ptr<Pipeline> pipeline) {
    mPipeline = std::move(pipeline);
    reset();
}

SHADERTOY_NAMESPACE_END
