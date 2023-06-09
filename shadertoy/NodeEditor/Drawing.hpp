﻿//------------------------------------------------------------------------------
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
#include "shadertoy/SuppressWarningPush.hpp"

#include <imgui.h>

#include "shadertoy/SuppressWarningPop.hpp"

namespace ax {
    namespace Drawing {

        enum class IconType : ImU32 { Flow, Circle, Square, Grid, RoundSquare, Diamond };

        void drawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, IconType type, bool filled, ImU32 color,
                      ImU32 innerColor);

    }  // namespace Drawing
}  // namespace ax
