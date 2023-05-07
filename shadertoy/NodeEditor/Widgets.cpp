//------------------------------------------------------------------------------
// LICENSE
//   This software is dual-licensed to the public domain and under the following
//   license: you are granted a perpetual, irrevocable license to copy, modify,
//   publish, and distribute this file as you see fit.
//
// CREDITS
//   Written by Michal Cichon
//   Modified by Yingwei Zheng
//------------------------------------------------------------------------------

#define IMGUI_DEFINE_MATH_OPERATORS
#include "shadertoy/NodeEditor/Widgets.hpp"
#pragma warning(push, 0)
#include <imgui.h>
#pragma warning(pop)

void ax::Widgets::icon(const ImVec2& size, const IconType type, const bool filled, const ImVec4& color /* = ImVec4(1, 1, 1, 1)*/,
                       const ImVec4& innerColor /* = ImVec4(0, 0, 0, 0)*/) {
    if(ImGui::IsRectVisible(size)) {
        const auto cursorPos = ImGui::GetCursorScreenPos();
        const auto drawList = ImGui::GetWindowDrawList();
        ax::Drawing::drawIcon(drawList, cursorPos, cursorPos + size, type, filled, ImColor(color), ImColor(innerColor));
    }

    ImGui::Dummy(size);
}
