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
#include "shadertoy/NodeEditor/Drawing.hpp"

#include "shadertoy/SuppressWarningPush.hpp"

#include <imgui_internal.h>

#include "shadertoy/SuppressWarningPop.hpp"

void ax::Drawing::drawIcon(ImDrawList* drawList, const ImVec2& a, const ImVec2& b, IconType type, bool filled, ImU32 color,
                           ImU32 innerColor) {
    auto rect = ImRect(a, b);
    auto rectX = rect.Min.x;
    auto rectY = rect.Min.y;
    auto rectW = rect.Max.x - rect.Min.x;
    auto rectH = rect.Max.y - rect.Min.y;
    auto rectCenterX = (rect.Min.x + rect.Max.x) * 0.5f;
    auto rectCenterY = (rect.Min.y + rect.Max.y) * 0.5f;
    auto rectCenter = ImVec2(rectCenterX, rectCenterY);
    const auto outlineScale = rectW / 24.0f;
    const auto extraSegments = static_cast<int>(2 * outlineScale);  // for full circle

    if(type == IconType::Flow) {
        const auto originScale = rectW / 24.0f;

        const auto offsetX = 1.0f * originScale;
        const auto offsetY = 0.0f * originScale;
        const auto margin = 2.0f * originScale;
        const auto rounding = 0.1f * originScale;
        constexpr auto tipRound = 0.7f;  // percentage of triangle edge (for tip)
        // const auto edge_round = 0.7f; // percentage of triangle edge (for corner)
        const auto canvas = ImRect(rect.Min.x + margin + offsetX, rect.Min.y + margin + offsetY, rect.Max.x - margin + offsetX,
                                   rect.Max.y - margin + offsetY);
        const auto canvasX = canvas.Min.x;
        const auto canvasY = canvas.Min.y;
        const auto canvasW = canvas.Max.x - canvas.Min.x;
        const auto canvasH = canvas.Max.y - canvas.Min.y;

        const auto left = canvasX + canvasW * 0.5f * 0.3f;
        const auto right = canvasX + canvasW - canvasW * 0.5f * 0.3f;
        const auto top = canvasY + canvasH * 0.5f * 0.2f;
        const auto bottom = canvasY + canvasH - canvasH * 0.5f * 0.2f;
        const auto centerY = (top + bottom) * 0.5f;
        // const auto angle = AX_PI * 0.5f * 0.5f * 0.5f;

        const auto tipTop = ImVec2(canvasX + canvasW * 0.5f, top);
        const auto tipRight = ImVec2(right, centerY);
        const auto tipBottom = ImVec2(canvasX + canvasW * 0.5f, bottom);

        drawList->PathLineTo(ImVec2(left, top) + ImVec2(0, rounding));
        drawList->PathBezierCubicCurveTo(ImVec2(left, top), ImVec2(left, top), ImVec2(left, top) + ImVec2(rounding, 0));
        drawList->PathLineTo(tipTop);
        drawList->PathLineTo(tipTop + (tipRight - tipTop) * tipRound);
        drawList->PathBezierCubicCurveTo(tipRight, tipRight, tipBottom + (tipRight - tipBottom) * tipRound);
        drawList->PathLineTo(tipBottom);
        drawList->PathLineTo(ImVec2(left, bottom) + ImVec2(rounding, 0));
        drawList->PathBezierCubicCurveTo(ImVec2(left, bottom), ImVec2(left, bottom), ImVec2(left, bottom) - ImVec2(0, rounding));

        if(!filled) {
            if(innerColor & 0xFF000000)
                drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

            drawList->PathStroke(color, true, 2.0f * outlineScale);
        } else
            drawList->PathFillConvex(color);
    } else {
        auto triangleStart = rectCenterX + 0.32f * rectW;

        auto rectOffset = -static_cast<int>(rectW * 0.25f * 0.25f);
        auto rectOffsetFloat = static_cast<float>(rectOffset);

        rect.Min.x += rectOffsetFloat;
        rect.Max.x += rectOffsetFloat;
        rectX += rectOffsetFloat;
        rectCenterX += rectOffsetFloat * 0.5f;
        rectCenter.x += rectOffsetFloat * 0.5f;

        if(type == IconType::Circle) {
            const auto c = rectCenter;

            if(!filled) {
                const auto r = 0.5f * rectW / 2.0f - 0.5f;

                if(innerColor & 0xFF000000)
                    drawList->AddCircleFilled(c, r, innerColor, 12 + extraSegments);
                drawList->AddCircle(c, r, color, 12 + extraSegments, 2.0f * outlineScale);
            } else {
                drawList->AddCircleFilled(c, 0.5f * rectW / 2.0f, color, 12 + extraSegments);
            }
        }

        if(type == IconType::Square) {
            if(filled) {
                const auto r = 0.5f * rectW / 2.0f;
                const auto p0 = rectCenter - ImVec2(r, r);
                const auto p1 = rectCenter + ImVec2(r, r);

#if IMGUI_VERSION_NUM > 18101
                drawList->AddRectFilled(p0, p1, color, 0, ImDrawFlags_RoundCornersAll);
#else
                drawList->AddRectFilled(p0, p1, color, 0, 15);
#endif
            } else {
                const auto r = 0.5f * rectW / 2.0f - 0.5f;
                const auto p0 = rectCenter - ImVec2(r, r);
                const auto p1 = rectCenter + ImVec2(r, r);

                if(innerColor & 0xFF000000) {
#if IMGUI_VERSION_NUM > 18101
                    drawList->AddRectFilled(p0, p1, innerColor, 0, ImDrawFlags_RoundCornersAll);
#else
                    drawList->AddRectFilled(p0, p1, innerColor, 0, 15);
#endif
                }

#if IMGUI_VERSION_NUM > 18101
                drawList->AddRect(p0, p1, color, 0, ImDrawFlags_RoundCornersAll, 2.0f * outlineScale);
#else
                drawList->AddRect(p0, p1, color, 0, 15, 2.0f * outline_scale);
#endif
            }
        }

        if(type == IconType::Grid) {
            const auto r = 0.5f * rectW / 2.0f;
            const auto w = ceilf(r / 3.0f);

            const auto baseTl = ImVec2(floorf(rectCenterX - w * 2.5f), floorf(rectCenterY - w * 2.5f));
            const auto baseBr = ImVec2(floorf(baseTl.x + w), floorf(baseTl.y + w));

            auto tl = baseTl;
            auto br = baseBr;
            for(int i = 0; i < 3; ++i) {
                tl.x = baseTl.x;
                br.x = baseBr.x;
                drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                if(i != 1 || filled)
                    drawList->AddRectFilled(tl, br, color);
                tl.x += w * 2;
                br.x += w * 2;
                drawList->AddRectFilled(tl, br, color);

                tl.y += w * 2;
                br.y += w * 2;
            }

            triangleStart = br.x + w + 1.0f / 24.0f * rectW;
        }

        if(type == IconType::RoundSquare) {
            if(filled) {
                const auto r = 0.5f * rectW / 2.0f;
                const auto cr = r * 0.5f;
                const auto p0 = rectCenter - ImVec2(r, r);
                const auto p1 = rectCenter + ImVec2(r, r);

#if IMGUI_VERSION_NUM > 18101
                drawList->AddRectFilled(p0, p1, color, cr, ImDrawFlags_RoundCornersAll);
#else
                drawList->AddRectFilled(p0, p1, color, cr, 15);
#endif
            } else {
                const auto r = 0.5f * rectW / 2.0f - 0.5f;
                const auto cr = r * 0.5f;
                const auto p0 = rectCenter - ImVec2(r, r);
                const auto p1 = rectCenter + ImVec2(r, r);

                if(innerColor & 0xFF000000) {
#if IMGUI_VERSION_NUM > 18101
                    drawList->AddRectFilled(p0, p1, innerColor, cr, ImDrawFlags_RoundCornersAll);
#else
                    drawList->AddRectFilled(p0, p1, innerColor, cr, 15);
#endif
                }

#if IMGUI_VERSION_NUM > 18101
                drawList->AddRect(p0, p1, color, cr, ImDrawFlags_RoundCornersAll, 2.0f * outlineScale);
#else
                drawList->AddRect(p0, p1, color, cr, 15, 2.0f * outline_scale);
#endif
            }
        } else if(type == IconType::Diamond) {
            if(filled) {
                const auto r = 0.607f * rectW / 2.0f;
                const auto c = rectCenter;

                drawList->PathLineTo(c + ImVec2(0, -r));
                drawList->PathLineTo(c + ImVec2(r, 0));
                drawList->PathLineTo(c + ImVec2(0, r));
                drawList->PathLineTo(c + ImVec2(-r, 0));
                drawList->PathFillConvex(color);
            } else {
                const auto r = 0.607f * rectW / 2.0f - 0.5f;
                const auto c = rectCenter;

                drawList->PathLineTo(c + ImVec2(0, -r));
                drawList->PathLineTo(c + ImVec2(r, 0));
                drawList->PathLineTo(c + ImVec2(0, r));
                drawList->PathLineTo(c + ImVec2(-r, 0));

                if(innerColor & 0xFF000000)
                    drawList->AddConvexPolyFilled(drawList->_Path.Data, drawList->_Path.Size, innerColor);

                drawList->PathStroke(color, true, 2.0f * outlineScale);
            }
        } else {
            const auto triangleTip = triangleStart + rectW * (0.45f - 0.32f);

            drawList->AddTriangleFilled(ImVec2(ceilf(triangleTip), rectY + rectH * 0.5f),
                                        ImVec2(triangleStart, rectCenterY + 0.15f * rectH),
                                        ImVec2(triangleStart, rectCenterY - 0.15f * rectH), color);
        }
    }
}
