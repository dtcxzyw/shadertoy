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
#include "shadertoy/NodeEditor/Builders.hpp"
#pragma warning(push, 0)
#include <imgui-node-editor/imgui_node_editor.h>
#include <imgui.h>
#pragma warning(pop)

//------------------------------------------------------------------------------
namespace ed = ax::NodeEditor;
namespace util = ax::NodeEditor::Utilities;

util::BlueprintNodeBuilder::BlueprintNodeBuilder(const ImTextureID texture, const int textureWidth, const int textureHeight)
    : mHeaderTextureId(texture), mHeaderTextureWidth(textureWidth), mHeaderTextureHeight(textureHeight), mCurrentNodeId(0),
      mCurrentStage(Stage::Invalid), mHasHeader(false) {}

void util::BlueprintNodeBuilder::begin(const ed::NodeId id) {
    mHasHeader = false;
    mHeaderMin = mHeaderMax = ImVec2();

    ed::PushStyleVar(StyleVar_NodePadding, ImVec4(8, 4, 8, 8));

    ed::BeginNode(id);

    ImGui::PushID(id.AsPointer());
    mCurrentNodeId = id;

    setStage(Stage::Begin);
}

void util::BlueprintNodeBuilder::end() {
    setStage(Stage::End);

    ed::EndNode();

    if(ImGui::IsItemVisible()) {
        const auto alpha = static_cast<int>(255 * ImGui::GetStyle().Alpha);

        const auto drawList = ed::GetNodeBackgroundDrawList(mCurrentNodeId);

        const auto halfBorderWidth = ed::GetStyle().NodeBorderWidth * 0.5f;

        auto headerColor = IM_COL32(0, 0, 0, alpha) | (mHeaderColor & IM_COL32(255, 255, 255, 0));
        if((mHeaderMax.x > mHeaderMin.x) && (mHeaderMax.y > mHeaderMin.y) && mHeaderTextureId) {
            const auto uv = ImVec2((mHeaderMax.x - mHeaderMin.x) / (4.0f * static_cast<float>(mHeaderTextureWidth)),
                                   (mHeaderMax.y - mHeaderMin.y) / (4.0f * static_cast<float>(mHeaderTextureHeight)));

            drawList->AddImageRounded(mHeaderTextureId, mHeaderMin - ImVec2(8 - halfBorderWidth, 4 - halfBorderWidth),
                                      mHeaderMax + ImVec2(8 - halfBorderWidth, 0), ImVec2(0.0f, 0.0f), uv,
#if IMGUI_VERSION_NUM > 18101
                                      headerColor, GetStyle().NodeRounding, ImDrawFlags_RoundCornersTop);
#else
                                      headerColor, GetStyle().NodeRounding, 1 | 2);
#endif

            const auto headerSeparatorMin = ImVec2(mHeaderMin.x, mHeaderMax.y);
            const auto headerSeparatorMax = ImVec2(mHeaderMax.x, mHeaderMin.y);

            if((headerSeparatorMax.x > headerSeparatorMin.x) && (headerSeparatorMax.y > headerSeparatorMin.y)) {
                drawList->AddLine(headerSeparatorMin + ImVec2(-(8 - halfBorderWidth), -0.5f),
                                  headerSeparatorMax + ImVec2((8 - halfBorderWidth), -0.5f),
                                  ImColor(255, 255, 255, 96 * alpha / (3 * 255)), 1.0f);
            }
        }
    }

    mCurrentNodeId = 0;

    ImGui::PopID();

    ed::PopStyleVar();

    setStage(Stage::Invalid);
}

void util::BlueprintNodeBuilder::header(const ImVec4& color) {
    mHeaderColor = ImColor(color);
    setStage(Stage::Header);
}

void util::BlueprintNodeBuilder::endHeader() {
    setStage(Stage::Content);
}

void util::BlueprintNodeBuilder::input(ed::PinId id) {
    if(mCurrentStage == Stage::Begin)
        setStage(Stage::Content);

    const auto applyPadding = (mCurrentStage == Stage::Input);

    setStage(Stage::Input);

    if(applyPadding)
        ImGui::Spring(0);

    pin(id, PinKind::Input);

    ImGui::BeginHorizontal(id.AsPointer());
}

void util::BlueprintNodeBuilder::endInput() {
    ImGui::EndHorizontal();

    endPin();
}

void util::BlueprintNodeBuilder::middle() {
    if(mCurrentStage == Stage::Begin)
        setStage(Stage::Content);

    setStage(Stage::Middle);
}

void util::BlueprintNodeBuilder::output(ed::PinId id) {
    if(mCurrentStage == Stage::Begin)
        setStage(Stage::Content);

    const auto applyPadding = (mCurrentStage == Stage::Output);

    setStage(Stage::Output);

    if(applyPadding)
        ImGui::Spring(0);

    pin(id, PinKind::Output);

    ImGui::BeginHorizontal(id.AsPointer());
}

void util::BlueprintNodeBuilder::endOutput() {
    ImGui::EndHorizontal();

    endPin();
}

bool util::BlueprintNodeBuilder::setStage(Stage stage) {
    if(stage == mCurrentStage)
        return false;

    const auto oldStage = mCurrentStage;
    mCurrentStage = stage;

    switch(oldStage) {
        case Stage::Header:
            ImGui::EndHorizontal();
            mHeaderMin = ImGui::GetItemRectMin();
            mHeaderMax = ImGui::GetItemRectMax();

            // spacing between header and content
            ImGui::Spring(0, ImGui::GetStyle().ItemSpacing.y * 2.0f);

            break;

        case Stage::Input:
            ed::PopStyleVar(2);

            ImGui::Spring(1, 0);
            ImGui::EndVertical();

            // #debug
            // ImGui::GetWindowDrawList()->AddRect(
            //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

            break;

        case Stage::Middle:
            ImGui::EndVertical();

            // #debug
            // ImGui::GetWindowDrawList()->AddRect(
            //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

            break;

        case Stage::Output:
            ed::PopStyleVar(2);

            ImGui::Spring(1, 0);
            ImGui::EndVertical();

            // #debug
            // ImGui::GetWindowDrawList()->AddRect(
            //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 255));

            break;
        case Stage::Content:  // NOLINT(bugprone-branch-clone)
            [[fallthrough]];
        case Stage::Begin:
            [[fallthrough]];
        case Stage::End:
            [[fallthrough]];
        case Stage::Invalid:
            break;
    }

    switch(stage) {
        case Stage::Begin:
            ImGui::BeginVertical("node");
            break;

        case Stage::Header:
            mHasHeader = true;

            ImGui::BeginHorizontal("header");
            break;

        case Stage::Content:
            if(oldStage == Stage::Begin)
                ImGui::Spring(0);

            ImGui::BeginHorizontal("content");
            ImGui::Spring(0, 0);
            break;

        case Stage::Input:
            ImGui::BeginVertical("inputs", ImVec2(0, 0), 0.0f);

            ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(0, 0.5f));
            ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

            if(!mHasHeader)
                ImGui::Spring(1, 0);
            break;

        case Stage::Middle:
            ImGui::Spring(1);
            ImGui::BeginVertical("middle", ImVec2(0, 0), 1.0f);
            break;

        case Stage::Output:
            if(oldStage == Stage::Middle || oldStage == Stage::Input)
                ImGui::Spring(1);
            else
                ImGui::Spring(1, 0);
            ImGui::BeginVertical("outputs", ImVec2(0, 0), 1.0f);

            ed::PushStyleVar(ed::StyleVar_PivotAlignment, ImVec2(1.0f, 0.5f));
            ed::PushStyleVar(ed::StyleVar_PivotSize, ImVec2(0, 0));

            if(!mHasHeader)
                ImGui::Spring(1, 0);
            break;

        case Stage::End:
            if(oldStage == Stage::Input)
                ImGui::Spring(1, 0);
            if(oldStage != Stage::Begin)
                ImGui::EndHorizontal();
            mContentMin = ImGui::GetItemRectMin();
            mContentMax = ImGui::GetItemRectMax();

            // ImGui::Spring(0);
            ImGui::EndVertical();
            mNodeMin = ImGui::GetItemRectMin();
            mNodeMax = ImGui::GetItemRectMax();
            break;

        case Stage::Invalid:
            break;
    }

    return true;
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void util::BlueprintNodeBuilder::pin(const ed::PinId id, const ed::PinKind kind) {
    ed::BeginPin(id, kind);
}

// ReSharper disable once CppMemberFunctionMayBeStatic
void util::BlueprintNodeBuilder::endPin() {
    ed::EndPin();

    // #debug
    // ImGui::GetWindowDrawList()->AddRectFilled(
    //     ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), IM_COL32(255, 0, 0, 64));
}
