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

    static void DrawRect(RectBounds bounds, ImVec4 color) {
        ImGui::GetWindowDrawList()->AddRectFilled(bounds.UL, bounds.BR, ImGui::ColorConvertFloat4ToU32(color));
    }

    static void PushClipRect(RectBounds bounds) {
        ImGui::GetWindowDrawList()->PushClipRect(bounds.UL, bounds.BR);
    }

    static void PopClipRect() {
        ImGui::GetWindowDrawList()->PopClipRect();
    }

    static bool MouseHoveringBounds(RectBounds bounds) {
        return ImGui::IsMouseHoveringRect(bounds.UL, bounds.BR) && ImGui::IsWindowFocused();
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
        ImGui::Begin(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_TITLE"), &pOpen, 0);
        if (!pOpen) {
            ImGui::End();
            throw ElectronSignal_CloseWindow;
        }
        bool isWindowFocused = ImGui::IsWindowFocused();
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();
        ImVec2 canvasPos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        static float legendWidth = 0.2f;
        static float zoom = TIMELINE_DEFAULT_ZOOM;
        zoom = glm::clamp(zoom, TIMELINE_MIN_ZOOM, TIMELINE_MAX_ZOOM);
        ImVec2 legendSize = {canvasSize.x * legendWidth, canvasSize.y};

        static float timelineOffsetX = 0.0f;
        static float timelineOffsetY = 0.0f;
        static float pixelsPerFrame = 3.0f;

        /* Ticks background handler */ {
            float backgroundHandlerHeight = canvasSize.y * 0.05f;
            RectBounds ticksBackground = RectBounds(ImVec2(0, 0), ImVec2(canvasSize.x, backgroundHandlerHeight));
            DrawRect(ticksBackground, ImVec4(0.045f, 0.045f, 0.045f, 1));

            // TODO: Ticks rendering
            int desiredTicksCount = 5;
            float tickStep = (float) instance->graphics.renderFramerate / (float) desiredTicksCount;
            float tickPositionStep = tickStep * pixelsPerFrame;
            float tickPositionAccumulator = 0;
            float tickAccumulator = 0;
            while (tickAccumulator <= instance->graphics.renderLength) {
                bool majorTick = remainder(tickAccumulator, instance->graphics.renderFramerate) == 0.0f;
                float tickHeight = majorTick ? 0 : 2.0f;
                RectBounds tickBounds = RectBounds(ImVec2(legendSize.x + tickPositionAccumulator + timelineOffsetX, 0), ImVec2(TIMELINE_TICK_WIDTH, majorTick ? canvasSize.y : 6.0f));
                DrawRect(tickBounds, ImVec4(0.1f, 0.1f, 0.1f, 1));

                tickPositionAccumulator += tickPositionStep;
                tickAccumulator += tickStep;
            }
        }

        /* Legend handler */ {
            legendWidth = glm::clamp(legendWidth, 0.15f, 0.6f);
            RectBounds legendBoxBounds = RectBounds(ImVec2(0, 0), legendSize);
            DrawRect(legendBoxBounds, ImVec4(0.1f, 0.1f, 0.1f, 1));
        }

        /* Timline rulers handler */ {
            PushClipRect(RectBounds(ImVec2(legendSize.x, 0), canvasSize - ImVec2(legendSize.x, 0)));
                RectBounds beginTimelineRuler = RectBounds(ImVec2(legendSize.x + timelineOffsetX, 0), ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
                DrawRect(beginTimelineRuler, ImVec4(0.89, 0.345, 0.345, 1));

                RectBounds mainTimelineRuler = RectBounds(ImVec2(legendSize.x + timelineOffsetX + instance->graphics.renderFrame * pixelsPerFrame, 0), ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
                DrawRect(mainTimelineRuler, ImVec4(0.224, 0.859, 0.949, 1));

            PopClipRect();
        }

        /* Legend splitter handler */ {
            static float splitterLogicWidth = 16.0f;
            static float splitterRenderWidth = 8.0f;
            RectBounds splitterLogicBox = RectBounds(ImVec2(legendSize.x - splitterLogicWidth, 0), ImVec2(splitterLogicWidth, canvasSize.y));
            RectBounds splitterRenderBox = RectBounds(ImVec2(legendSize.x - splitterRenderWidth, 0), ImVec2(splitterRenderWidth, canvasSize.y));
            static DragStructure splitterDrag{};
 
            if (MouseHoveringBounds(splitterLogicBox)) {
                DrawRect(splitterRenderBox, GetColor(ImGuiCol_Border));
                splitterDrag.Activate();
                if (isWindowFocused) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);

            }

            float dragAmount;
            if (splitterDrag.GetDragDistance(dragAmount)) {
                legendWidth += dragAmount / canvasSize.x;
            } else {
                if (isWindowFocused && !MouseHoveringBounds(splitterLogicBox)) ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
                splitterDrag.Deactivate();
            }
        }


        static DragStructure xDrag{};
        /* X Scrollbar handler */ {
            RectBounds clipRect = RectBounds(ImVec2(legendSize.x, 0), ImVec2(canvasSize.x - legendSize.x, canvasSize.y));
            PushClipRect(clipRect);

            float minTimelineX = -50;
            float maxTimelineX = instance->graphics.renderLength * pixelsPerFrame + 50;

            float percentScrolled = -timelineOffsetX / (instance->graphics.renderLength * pixelsPerFrame);
            DUMP_VAR(percentScrolled);
            percentScrolled = glm::clamp(percentScrolled, 0, 1);
            ImVec2 scrollerWidth = ImVec2(, 10);
            RectBounds scrollBarRect = RectBounds(ImVec2(legendSize.x + (percentScrolled * (canvasSize.x - legendSize.x)), canvasSize.y - TIMELINE_SCROLLBAR_SIZE.y), TIMELINE_SCROLLBAR_SIZE);
            DrawRect(scrollBarRect, ImVec4(0, 1, 0, 1));

            if (MouseHoveringBounds(scrollBarRect)) {
                xDrag.Activate();
            }

            float scrollDist;
            if (xDrag.GetDragDistance(scrollDist)) {
                timelineOffsetX -= scrollDist;
            } else xDrag.Deactivate();

            timelineOffsetX = glm::clamp(timelineOffsetX, minTimelineX, maxTimelineX);

            PopClipRect();
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }
}