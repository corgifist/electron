#include "editor_core.h"

namespace Electron {

    int UICounters::TimelineCounter = 0;

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
        UICounters::TimelineCounter++;
    }

    Timeline::~Timeline() {
        UICounters::TimelineCounter--;
    }

    static float pixelsPerFrame = 3.0f;

    static void IncreasePixelsPerFrame() {
        pixelsPerFrame += 0.1f;
    }

    static void DecreasePixelsPerFrame() {
        pixelsPerFrame -= 0.1f;
    }

    void Timeline::Render(AppInstance* instance) {
        static bool shortcutsSetup = false;
        if (!shortcutsSetup) {
            shortcutsSetup = true;
            instance->AddShortcut({ImGuiKey_KeypadAdd}, IncreasePixelsPerFrame);
            instance->AddShortcut({ImGuiKey_KeypadSubtract}, DecreasePixelsPerFrame);
        }
        bool pOpen = true;


        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        ImGui::Begin(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_TITLE"), &pOpen, 0);
        if (!pOpen) {
            ImGui::End();
            throw ElectronSignal_CloseWindow;
        }
        if (!instance->projectOpened) {
            ImVec2 windowSize = ImGui::GetWindowSize();
            std::string noProjectOpened = ELECTRON_GET_LOCALIZATION(instance, "GENERIC_NO_PROJECT_IS_OPENED");
            ImVec2 textSize = ImGui::CalcTextSize(noProjectOpened.c_str());
            ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - textSize.x / 2.0f, windowSize.y / 2.0f - textSize.y / 2.0f});
            ImGui::Text(noProjectOpened.c_str());
            ImGui::End();
            ImGui::PopStyleVar(2);
            return;
        }
        bool isWindowFocused = ImGui::IsWindowFocused();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 windowMouseCoords = ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
        pixelsPerFrame = glm::clamp(pixelsPerFrame, 2.0f, 10.0f);

        static DragStructure timelineDrag{};

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
            if (ImGui::CollapsingHeader((layer->layerUsername + "##" + std::to_string(i)).c_str())) {

            }
            propertiesCoordAcc += propertiesStep;
        }
            
        ImGui::EndChild();
 
        ImGui::SameLine();

        ImGui::BeginChild("projectTimeline", ImVec2(canvasSize.x - legendSize.x, canvasSize.y), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        windowMouseCoords = ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
        PushClipRect(fullscreenTicksMask);
        std::vector<TimeStampTarget> stamps{};
        DrawRect(ticksBackground, ImVec4(0.045f, 0.045f, 0.045f, 1));
        int desiredTicksCount = pixelsPerFrame * 2;
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
        static std::vector<DragStructure> forwardLayerDrags;
        static std::vector<DragStructure> backwardLayerDrags;
        if (universalLayerDrags.size() != instance->graphics.layers.size()) {
            universalLayerDrags = std::vector<DragStructure>(instance->graphics.layers.size());
        }
        if (forwardLayerDrags.size() != instance->graphics.layers.size()) {
            forwardLayerDrags = std::vector<DragStructure>(instance->graphics.layers.size());
        }
        if (backwardLayerDrags.size() != instance->graphics.layers.size()) {
            backwardLayerDrags = std::vector<DragStructure>(instance->graphics.layers.size());
        }
        static std::vector<float> layerSeparatorTargets{};
        for (auto& separatorY : layerSeparatorTargets) {
            RectBounds separatorBounds = RectBounds(ImVec2(0 + ImGui::GetScrollX(), separatorY + ImGui::GetScrollY()), ImVec2(canvasSize.x, 2.0f));
            DrawRect(separatorBounds, ImVec4(0, 0, 0, 1));
        }
        layerSeparatorTargets.clear();
        int layerDeleteionTarget = -1;
        for (int i = instance->graphics.layers.size() - 1; i >= 0; i--) {
            RenderLayer* layer = &instance->graphics.layers[i];
            DragStructure& universalDrag = universalLayerDrags[i];
            DragStructure& forwardDrag = forwardLayerDrags[i];
            DragStructure& backwardDrag = backwardLayerDrags[i];
            float layerDuration = layer->endFrame - layer->beginFrame;
            ImVec4 layerColor = ImVec4{layer->layerColor.r, layer->layerColor.g, layer->layerColor.b, layer->layerColor.a};
            ImGui::SetCursorPosY(layerOffsetY + 2);
            ImGui::SetCursorPosX(pixelsPerFrame * layer->beginFrame);
            ImGui::PushStyleColor(ImGuiCol_Button, layerColor);
            if (ImGui::Button((layer->layerUsername + "##" + std::to_string(i)).c_str(), ImVec2(pixelsPerFrame * layerDuration, layerSizeY))) {
                instance->selectedRenderLayer = i;
            } 
            if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                ImGui::OpenPopup(string_format("TimelineLayerPopup%i", i).c_str());
            }
            ImGui::SetCursorPos(ImVec2{0, 0});
            ImVec2 dragSize = ImVec2(layerDuration * pixelsPerFrame / 10, layerSizeY);
            dragSize.x = glm::clamp(dragSize.x, 1.0f, 30.0f);
            RectBounds forwardDragBounds = RectBounds(ImVec2(pixelsPerFrame * layer->beginFrame + pixelsPerFrame * layerDuration - dragSize.x, layerOffsetY + 2), dragSize);
            DrawRect(forwardDragBounds, layerColor * ImVec4(0.9f, 0.9f, 0.9f, 1));
            RectBounds backwardDragBounds = RectBounds(ImVec2(pixelsPerFrame * layer->beginFrame, layerOffsetY + 2), dragSize);
            DrawRect(backwardDragBounds, layerColor * ImVec4(0.9f, 0.9f, 0.9f, 1));

            bool anyOtherButtonsDragged = false;
            for (auto& drag : universalLayerDrags) {
                if (drag.isActive) anyOtherButtonsDragged = true;
            }

            for (auto& drag : forwardLayerDrags) {
                if (drag.isActive) anyOtherButtonsDragged = true;
            }

            for (auto& drag : backwardLayerDrags) {
                if (drag.isActive) anyOtherButtonsDragged = true;
            }

            if (MouseHoveringBounds(forwardDragBounds) && !anyOtherButtonsDragged && !timelineDrag.isActive) {
                forwardDrag.Activate();
            }

            if (MouseHoveringBounds(backwardDragBounds) && !anyOtherButtonsDragged && !timelineDrag.isActive) {
                backwardDrag.Activate();
            }

            if (ImGui::IsItemHovered() && !anyOtherButtonsDragged && !timelineDrag.isActive && !forwardDrag.isActive) {
                universalDrag.Activate();
            }

            float universalDragDistance;
            float forwardDragDistance;
            float backwardDragDistance;

            if (forwardDrag.GetDragDistance(forwardDragDistance) && !timelineDrag.isActive) {
                layer->endFrame = (windowMouseCoords.x + ImGui::GetScrollX()) / pixelsPerFrame;
            } else forwardDrag.Deactivate();

            if (backwardDrag.GetDragDistance(backwardDragDistance) && !timelineDrag.isActive) {
                layer->beginFrame = (windowMouseCoords.x + ImGui::GetScrollX()) / pixelsPerFrame;
            } else backwardDrag.Deactivate();

            if (isWindowFocused) {
                if (forwardDrag.isActive) {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                } else {
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                }
            }

            RectBounds layerFullheightBounds = RectBounds(ImVec2(pixelsPerFrame * layer->beginFrame, 0), ImVec2(pixelsPerFrame * layerDuration, canvasSize.y));
            if (((universalDrag.GetDragDistance(universalDragDistance) && universalDragDistance != 0 && !timelineDrag.isActive) || (MouseHoveringBounds(layerFullheightBounds) && universalDrag.isActive && ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])) && backwardDrag.isActive == false) {
                float midpointFrame = (windowMouseCoords.x) / pixelsPerFrame;
                float halfLayerDuration = layerDuration / 2.0f;
                float tempBeginFrame = midpointFrame - halfLayerDuration;
                float tempEndFrame = midpointFrame + halfLayerDuration;
                if (tempBeginFrame > 0 && glm::abs(universalDragDistance) > 1.0f) {
                    layer->beginFrame = tempBeginFrame;
                    layer->endFrame = tempEndFrame;
                }

                layer->beginFrame = glm::max(layer->beginFrame, 0);
                layer->endFrame = glm::max(layer->endFrame, 0);
                timelineDrag.Deactivate();
            } else universalDrag.Deactivate();

            ImGui::PopStyleVar(2);
            if (ImGui::BeginPopup(string_format("TimelineLayerPopup%i", i).c_str())) {
                ImGui::SeparatorText(layer->layerUsername.c_str());
                if (ImGui::Selectable("Delete layer")) {
                    layerDeleteionTarget = i;
                }
                ImGui::EndPopup();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            ImGui::PopStyleColor();
            layerSeparatorTargets.push_back(layerOffsetY);
            layerOffsetY += layerSizeY;
        }

        ImGui::SetCursorPos({0, 0});
        RectBounds timelineBounds = RectBounds(ImVec2(pixelsPerFrame * instance->graphics.renderFrame, 0), ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
        DrawRect(timelineBounds, ImVec4(0.871, 0.204, 0.204, 1));

        
        bool anyLayerDragged = false;
        for (auto& drag : universalLayerDrags) {
            if (drag.isActive) anyLayerDragged = true;
        }
        for (auto& drag : forwardLayerDrags) {
            if (drag.isActive) anyLayerDragged = true;
        }

        for (auto& drag : backwardLayerDrags) {
            if (drag.isActive) anyLayerDragged = true;
        }

        if (MouseHoveringBounds(timelineBounds) || (MouseHoveringBounds(ticksBackground) && !anyLayerDragged)) {
            timelineDrag.Activate();
        }

        float timelineDragDist;
        if (timelineDrag.GetDragDistance(timelineDragDist) && !anyLayerDragged) {
            instance->graphics.renderFrame = (windowMouseCoords.x) / pixelsPerFrame;
        } else timelineDrag.Deactivate();

        RectBounds endTimelineBounds = RectBounds(ImVec2(pixelsPerFrame * instance->graphics.renderLength, 0), ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
        DrawRect(endTimelineBounds, ImVec4(1, 1, 0, 1));

        ImGui::EndChild();

        ImGui::End();
        ImGui::PopStyleVar(2);
        if (layerDeleteionTarget != -1) {
            auto& layers = instance->graphics.layers;
            layers.erase(layers.begin() + layerDeleteionTarget);
        }

    }
}