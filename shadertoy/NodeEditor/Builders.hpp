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

//------------------------------------------------------------------------------
#pragma warning(push, 0)
#include <imgui-node-editor/imgui_node_editor.h>
#pragma warning(pop)

//------------------------------------------------------------------------------
namespace ax {
    namespace NodeEditor {
        namespace Utilities {

            //------------------------------------------------------------------------------
            struct BlueprintNodeBuilder {
                explicit BlueprintNodeBuilder(ImTextureID texture = nullptr, int textureWidth = 0, int textureHeight = 0);

                void begin(NodeId id);
                void end();

                void header(const ImVec4& color = ImVec4(1, 1, 1, 1));
                void endHeader();

                void input(PinId id);
                void endInput();

                void middle();

                void output(PinId id);
                void endOutput();

            private:
                enum class Stage { Invalid, Begin, Header, Content, Input, Output, Middle, End };

                bool setStage(Stage stage);

                void pin(PinId id, ax::NodeEditor::PinKind kind);
                void endPin();

                ImTextureID mHeaderTextureId;
                int mHeaderTextureWidth;
                int mHeaderTextureHeight;
                NodeId mCurrentNodeId;
                Stage mCurrentStage;
                ImU32 mHeaderColor{};
                ImVec2 mNodeMin;
                ImVec2 mNodeMax;
                ImVec2 mHeaderMin;
                ImVec2 mHeaderMax;
                ImVec2 mContentMin;
                ImVec2 mContentMax;
                bool mHasHeader;
            };

            //------------------------------------------------------------------------------
        }  // namespace Utilities
    }      // namespace NodeEditor
}  // namespace ax
