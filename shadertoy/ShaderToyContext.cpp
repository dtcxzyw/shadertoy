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

#include "shadertoy/ShaderToyContext.hpp"
#include <cassert>
#include <imgui.h>

SHADERTOY_NAMESPACE_BEGIN

ShaderToyContext::ShaderToyContext() : mRunning{ true } {
    reset();
}
void ShaderToyContext::tick() {
    mFBSize = ImGui::GetWindowViewport()->Size;

    if(!mRunning)
        return;
    const auto time = static_cast<float>(
        static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - mStartTime).count()) * 1e-9);
    mTimeDelta = time - mTime;
    mTime = time;
    ++mFrameCount;
}
void ShaderToyContext::pause() {
    assert(mRunning);
    mRunning = false;
    mTimeDelta = 0.0f;
    mPauseTime = Clock::now();
}
void ShaderToyContext::resume() {
    assert(!mRunning);
    mRunning = true;
    if(mTime == 0.0f)
        mStartTime = Clock::now();
    else
        mStartTime += Clock::now() - mPauseTime;
}
void ShaderToyContext::reset() {
    mStartTime = Clock::now();
    mTime = mTimeDelta = 0.0f;
    mFrameCount = 0;
}
void ShaderToyContext::render(ImVec2 base, ImVec2 size, const std::optional<ImVec4>& mouse) {
    auto* drawList = ImGui::GetWindowDrawList();
    mBase = base;
    mSize = size;
    if(mouse)
        mMouse = mouse.value();
    else {
        mMouse.z = mMouse.w = 0.0f;
    }

    if(mPipeline) {
        drawList->AddCallback(
            [](const ImDrawList*, const ImDrawCmd* cmd) {
                const ImVec2 clipOff = ImGui::GetDrawData()->DisplayPos;
                const ImVec2 clipScale = ImGui::GetDrawData()->FramebufferScale;

                const auto ctx = static_cast<ShaderToyContext*>(cmd->UserCallbackData);
                const ImVec2 clipMin((cmd->ClipRect.x - clipOff.x) * clipScale.x, (cmd->ClipRect.y - clipOff.y) * clipScale.y);
                const ImVec2 clipMax((cmd->ClipRect.z - clipOff.x) * clipScale.x, (cmd->ClipRect.w - clipOff.y) * clipScale.y);
                if(clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                    return;
                ctx->mPipeline->render(ctx->mFBSize, clipMin, clipMax, ctx->mBase, ctx->mSize,
                                       { ctx->mTime, ctx->mTimeDelta, ImGui::GetIO().Framerate, ctx->mFrameCount, ctx->mMouse });
            },
            this);
        drawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    } else
        drawList->AddRect(mBase, ImVec2{ mBase.x + mSize.x, mBase.y + mSize.y }, IM_COL32(255, 255, 0, 255));
}
void ShaderToyContext::compile(const std::string& src) {
    if(auto pipeline = createPipeline(src)) {
        mPipeline = std::move(pipeline);
        reset();
    }
}

SHADERTOY_NAMESPACE_END
