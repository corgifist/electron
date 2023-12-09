#pragma once

#define IMGUI_DEFINE_MATH_OPERATORS
#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

namespace Electron {
        struct DragStructure {
        bool isActive;

        DragStructure() {
            isActive = false;
        }

        bool GetDragDistance(float& distance) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[ImGuiMouseButton_Left] && isActive && ImGui::IsWindowFocused() && ImGui::IsWindowHovered()) {
                distance = io.MouseDelta.x;
                return true;
            }
            return false;
        }

        void Activate() {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[ImGuiMouseButton_Left] && !isActive && ImGui::IsWindowFocused()) {
                isActive = true;
            }
        }

        void Deactivate() {
            isActive = false;
        }
    };

    struct RectBounds {
        ImVec2 UL, BR;
        ImVec2 pos, size;

        RectBounds(ImVec2 pos, ImVec2 size) {
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();  

            this->size = size;
            this->pos = pos;

            UL = canvasPos + pos;
            BR = canvasPos + pos + size; 
        }
    };
};