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

#pragma once
#include "shadertoy/NodeEditor/Drawing.hpp"
#pragma warning(push, 0)
#include <imgui.h>
#pragma warning(pop)

namespace ax {
    namespace Widgets {

        using Drawing::IconType;

        void icon(const ImVec2& size, IconType type, bool filled, const ImVec4& color = ImVec4(1, 1, 1, 1),
                  const ImVec4& innerColor = ImVec4(0, 0, 0, 0));

    }  // namespace Widgets
}  // namespace ax
