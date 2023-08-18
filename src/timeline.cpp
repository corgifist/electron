#include "editor_core.h"

namespace Electron {

    struct DragStructure {
        bool isActive;

        bool GetDragDistance(float& distance) {
            ImGuiIO& io = ImGui::GetIO();
            if (io.MouseDown[ImGuiMouseButton_Left] && isActive && ImGui::IsWindowFocused()) {
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

        RectBounds(ImVec2 pos, ImVec2 size) {
            ImVec2 canvasPos = ImGui::GetCursorScreenPos();
            ImVec2 canvasSize = ImGui::GetContentRegionAvail();  

            UL = canvasPos + pos;
            BR = canvasPos + pos + size; 
        }
    };

    struct TimeStampTarget {
        int frame;
        ImVec2 position;

        TimeStampTarget() {}
    };

    static void DrawRect(RectBounds bounds, ImVec4 color) {
        ImGui::GetWindowDrawList()->AddRectFilled(bounds.UL, bounds.BR, ImGui::ColorConvertFloat4ToU32(color));
    }

    static void PushClipRect(RectBounds bounds) {
        ImGui::GetWindowDrawList()->PushClipRect(bounds.UL, bounds.BR, true);
    }

    static void PopClipRect() {
        ImGui::GetWindowDrawList()->PopClipRect();
    }

    static bool MouseHoveringBounds(RectBounds bounds) {
        return ImGui::IsMouseHoveringRect(bounds.UL, bounds.BR);
    }

    static ImVec4 GetColor(ImGuiCol color) {
        ImVec4 col = ImGui::GetStyle().Colors[color];
        col.w = 1.0f;
        return col;
    }

    Timeline::Timeline() {
        
    }

    void Timeline::Render(AppInstance* instance) {
        bool pOpen = true;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::Begin(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_TITLE"), &pOpen, 0);
        if (!pOpen) {
            ImGui::End();
            throw ElectronSignal_CloseWindow;
        }
        bool isWindowFocused = ImGui::IsWindowFocused();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        static float pixelsPerFrame = 3.0f;


        static float legendWidth = 0.2f;
        ImVec2 legendSize(canvasSize.x * legendWidth, canvasSize.y);
        float ticksBackgroundHeight = canvasSize.y * 0.05f;
        RectBounds fillerTicksBackground = RectBounds(ImVec2(0, 0), ImVec2(legendSize.x, ticksBackgroundHeight));
        RectBounds ticksBackground = RectBounds(ImVec2(0, 2), ImVec2(canvasSize.x, ticksBackgroundHeight));
        RectBounds fullscreenTicksMask = RectBounds(ImVec2(0, 2), ImVec2(canvasSize.x, canvasSize.y));
        ImGui::BeginChild("projectlegend", legendSize, true);
        DrawRect(fillerTicksBackground, ImVec4(0.045f, 0.045f, 0.045f, 1));
        float propertiesStep = 22;
        float propertiesCoordAcc = 0;
        for (int i = instance->graphics.layers.size() - 1; i >= 0; i--) {
            RenderLayer* layer = &instance->graphics.layers[i];
            ImGui::SetCursorPosY(ticksBackgroundHeight + 2 + propertiesCoordAcc);
            if (ImGui::CollapsingHeader((layer->layerPublicName + "##" + std::to_string(i)).c_str())) {

            }
            propertiesCoordAcc += propertiesStep;
        }
            
        ImGui::EndChild();
 
        ImGui::SameLine();

        ImGui::BeginChild("projectTimeline", ImVec2(canvasSize.x - legendSize.x, canvasSize.y), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        PushClipRect(fullscreenTicksMask);
        std::vector<TimeStampTarget> stamps{};
        DrawRect(ticksBackground, ImVec4(0.045f, 0.045f, 0.045f, 1));
        int desiredTicksCount = 5;
        float tickStep = (float) instance->graphics.renderFramerate / (float) desiredTicksCount;
        float tickPositionStep = tickStep * pixelsPerFrame;
        float tickPositionAccumulator = 0;
        float tickAccumulator = 0;
        while (tickAccumulator <= instance->graphics.renderLength) {
            bool majorTick = remainder(tickAccumulator, instance->graphics.renderFramerate) == 0.0f;
            float tickHeight = majorTick ? 0 : 2.0f;
            RectBounds tickBounds = RectBounds(ImVec2(tickPositionAccumulator, 0), ImVec2(TIMELINE_TICK_WIDTH, majorTick ? canvasSize.y : 6.0f));
            DrawRect(tickBounds, ImVec4(0.1f, 0.1f, 0.1f, 1));

            if (majorTick) {
                TimeStampTarget stamp{};
                stamp.frame = tickAccumulator;
                stamp.position = ImVec2(tickPositionAccumulator, 0);
                stamps.push_back(stamp);
            }

            tickPositionAccumulator += tickPositionStep;
            tickAccumulator += tickStep;
        }

        for (auto& stamp : stamps) {
            ImGui::GetWindowDrawList()->AddText(canvasPos + stamp.position + ImVec2(legendSize.x - ImGui::GetScrollX(), 0), ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1)), formatToTimestamp(stamp.frame, instance->graphics.renderFramerate).c_str());
        }
        PopClipRect();

        float layerOffsetY = ticksBackgroundHeight;
        float layerSizeY = 22;
        static std::vector<DragStructure> universalLayerDrags;
        if (universalLayerDrags.size() != instance->graphics.layers.size()) {
            universalLayerDrags = std::vector<DragStructure>(instance->graphics.layers.size());
        }
        static std::vector<float> layerSeparatorTargets{};
        for (auto& separatorY : layerSeparatorTargets) {
            RectBounds separatorBounds = RectBounds(ImVec2(0 + ImGui::GetScrollX(), separatorY + ImGui::GetScrollY()), ImVec2(canvasSize.x, 2.0f));
            DrawRect(separatorBounds, ImVec4(0, 0, 0, 1));
        }
        layerSeparatorTargets.clear();
        for (int i = instance->graphics.layers.size() - 1; i >= 0; i--) {
            RenderLayer* layer = &instance->graphics.layers[i];
            DragStructure& universalDrag = universalLayerDrags[i];
            ImGui::SetCursorPosY(layerOffsetY + 1);
            ImGui::SetCursorPosX(pixelsPerFrame * layer->beginFrame);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{layer->layerColor.r, layer->layerColor.g, layer->layerColor.b, layer->layerColor.a});
            if (ImGui::Button((layer->layerPublicName + "##" + std::to_string(i)).c_str(), ImVec2(pixelsPerFrame * (layer->endFrame - layer->beginFrame), layerSizeY))) {
                instance->selectedRenderLayer = i;
            } 

            bool anyOtherButtonsDragged = false;
            for (auto& drag : universalLayerDrags) {
                if (drag.isActive) anyOtherButtonsDragged = true;
            }

            if (ImGui::IsItemHovered() && !anyOtherButtonsDragged) {
                universalDrag.Activate();
            }

            float universalDragDistance;
            if (universalDrag.GetDragDistance(universalDragDistance)) {
                int frameAddition = glm::ceil(universalDragDistance / pixelsPerFrame);
                layer->beginFrame += frameAddition;
                layer->endFrame += frameAddition;

                layer->beginFrame = glm::max(layer->beginFrame, 0);
                layer->endFrame = glm::max(layer->endFrame, 0);
            } else universalDrag.Deactivate();
            ImGui::PopStyleColor();
            layerSeparatorTargets.push_back(layerOffsetY);
            layerOffsetY += layerSizeY;
        }
        ImGui::EndChild();

        ImGui::End();
        ImGui::PopStyleVar(2);

        if (glfwGetKey(instance->displayHandle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(instance->displayHandle, GLFW_KEY_KP_ADD) == GLFW_PRESS) {
            pixelsPerFrame += 0.1f;
        } 
        if (glfwGetKey(instance->displayHandle, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && glfwGetKey(instance->displayHandle, GLFW_KEY_KP_SUBTRACT) == GLFW_PRESS) {
            pixelsPerFrame -= 0.1f;
        }
    }
}