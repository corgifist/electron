#include "editor_core.h"
#define LEGEND_LAYER_DRAG_DROP "LEGEND_LAYER_DRAG_DROP"

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


        static RenderLayer copyContainer;
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
        static std::vector<float> layersPropertiesOffset{};
        if (layersPropertiesOffset.size() != instance->graphics.layers.size()) {
            layersPropertiesOffset = std::vector<float>(instance->graphics.layers.size());
        }

        static float legendWidth = 0.2f;
        ImVec2 legendSize(canvasSize.x * legendWidth, canvasSize.y);
        float ticksBackgroundHeight = canvasSize.y * 0.05f;
        RectBounds fillerTicksBackground = RectBounds(ImVec2(0, 0 + ImGui::GetScrollY()), ImVec2(legendSize.x, ticksBackgroundHeight));
        RectBounds ticksBackground = RectBounds(ImVec2(0, 2 + ImGui::GetScrollY()), ImVec2(canvasSize.x, ticksBackgroundHeight));
        RectBounds fullscreenTicksMask = RectBounds(ImVec2(0, 2), ImVec2(canvasSize.x, canvasSize.y));
        static float legendScrollY = 0;

        std::vector<float> propertiesSeparatorsY{};

        ImGui::BeginChild("projectlegend", legendSize, true, ImGuiWindowFlags_NoScrollbar);
        ImGui::SetScrollY(legendScrollY);
        DrawRect(fillerTicksBackground, ImVec4(0.045f, 0.045f, 0.045f, 1));
        float propertiesStep = 22;
        float propertiesCoordAcc = 0;
        for (int i = instance->graphics.layers.size() - 1; i >= 0; i--) {
            RenderLayer* layer = &instance->graphics.layers[i];
            ImGui::SetCursorPosY(ticksBackgroundHeight + 2 + propertiesCoordAcc);
            bool active = false;
            ImGuiDragDropFlags src_flags = 0;
            src_flags |= ImGuiDragDropFlags_SourceNoDisableHover; 
            src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers;
            ImGuiDragDropFlags target_flags = 0;
            target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
            ImGui::SetItemAllowOverlap();
            if (ImGui::CollapsingHeader((layer->layerUsername + "##" + std::to_string(i)).c_str())) {
                active = true;
                if (ImGui::BeginDragDropSource(src_flags)) {
                    ImGui::SetDragDropPayload(LEGEND_LAYER_DRAG_DROP, &i, sizeof(i));
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(LEGEND_LAYER_DRAG_DROP, target_flags)) {
                        int from = *((int*) payload->Data);
                        int to = i;
                        std::swap(instance->graphics.layers[from], instance->graphics.layers[i]);
                    }
                    ImGui::EndDragDropTarget();
                }
                propertiesSeparatorsY.push_back(ImGui::GetCursorPosY());
                ImGui::PopStyleVar(2);
                float firstCursorY = ImGui::GetCursorPosY();

                json_t previewTargets = layer->previewProperties;
                float layerViewTime = instance->graphics.renderFrame - layer->beginFrame;
                for (int i = 0; i < previewTargets.size(); i++) {
                    json_t& propertyMap = layer->properties[previewTargets.at(i)];
                    GeneralizedPropertyType propertyType = static_cast<GeneralizedPropertyType>(JSON_AS_TYPE(propertyMap.at(0), int));
                    bool keyframeAlreadyExists = false;
                    bool customKeyframesExist = (propertyMap.size() - 1) > 1;
                    int keyframeIndex = 1;
                    for (int j = 1; j < propertyMap.size(); j++) {
                        json_t specificKeyframe = propertyMap.at(j);
                        if (JSON_AS_TYPE(specificKeyframe.at(0), int) == (int) layerViewTime) {
                            keyframeAlreadyExists = true;
                            keyframeIndex = j;
                            break;
                        }
                    }
                    ImGui::SetCursorPosX(8.0f);
                    int propertySize = 1;
                    switch (propertyType) {
                        case GeneralizedPropertyType::Float: {
                            float x = JSON_AS_TYPE(layer->InterpolateProperty(propertyMap).at(0), float);
                            if (ImGui::InputFloat((JSON_AS_TYPE(previewTargets.at(i), std::string) + "##" + std::to_string(i)).c_str(), &x, 0, 0, "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
                                if (!keyframeAlreadyExists && customKeyframesExist) {
                                    propertyMap.push_back({(int) layerViewTime, x});
                                } else {
                                    propertyMap.at(keyframeIndex).at(1) = x;
                                }
                            }
                            propertySize = 1;
                            break;
                        }
                        case GeneralizedPropertyType::Vec2: {
                            json_t x = layer->InterpolateProperty(propertyMap);
                            std::vector<float> raw = {JSON_AS_TYPE(x.at(0), float), JSON_AS_TYPE(x.at(1), float)};
                            if (ImGui::InputFloat2((JSON_AS_TYPE(previewTargets.at(i), std::string) + "##" + std::to_string(i)).c_str(), raw.data(), "%.3f", ImGuiInputTextFlags_EnterReturnsTrue)) {
                                if (!keyframeAlreadyExists && customKeyframesExist) {
                                    propertyMap.push_back({(int) layerViewTime, raw.at(0), raw.at(1)});
                                } else {
                                    propertyMap.at(keyframeIndex).at(1) = raw.at(0);
                                    propertyMap.at(keyframeIndex).at(2) = raw.at(1);
                                }
                            }
                            propertySize = 2;
                            break;
                        }
                        case GeneralizedPropertyType::Vec3:
                        case GeneralizedPropertyType::Color3: {
                            json_t x = layer->InterpolateProperty(propertyMap);
                            std::vector<float> raw = {JSON_AS_TYPE(x.at(0), float), JSON_AS_TYPE(x.at(1), float), JSON_AS_TYPE(x.at(2), float)};
                            if (propertyType == GeneralizedPropertyType::Vec3 ? 
                                        ImGui::InputFloat3((JSON_AS_TYPE(previewTargets.at(i), std::string) + "##" + std::to_string(i)).c_str(), raw.data(), "%.3f", ImGuiInputTextFlags_EnterReturnsTrue) :
                                        ImGui::ColorEdit3((JSON_AS_TYPE(previewTargets.at(i), std::string) + "##" + std::to_string(i)).c_str(), raw.data(), 0)) {
                                if (!keyframeAlreadyExists && customKeyframesExist) {
                                    propertyMap.push_back({(int) layerViewTime, raw.at(0), raw.at(1), raw.at(1)});
                                } else {
                                    propertyMap.at(keyframeIndex).at(1) = raw.at(0);
                                    propertyMap.at(keyframeIndex).at(2) = raw.at(1);
                                    propertyMap.at(keyframeIndex).at(3) = raw.at(2);
                                }
                            }
                            propertySize = 3;
                            break;
                        }
                    }
                    if (ImGui::IsItemHovered() && ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                        ImGui::OpenPopup(string_format("LegendProperty%i%i", i, layer->id).c_str());
                    }
                    if (ImGui::BeginPopup(string_format("LegendProperty%i%i", i, layer->id).c_str())) {
                        ImGui::SeparatorText(string_format("%s", (JSON_AS_TYPE(previewTargets.at(i), std::string)).c_str()).c_str());
                        if (ImGui::Selectable(ELECTRON_GET_LOCALIZATION(instance, "TIMELINE_ADD_KEYFRAME"))) {
                            json_t xKeyframe = propertyMap.at(keyframeIndex);
                            xKeyframe.at(0) = (int) layerViewTime;
                            propertyMap.push_back(xKeyframe);
                        }
                        ImGui::EndPopup();
                    }
                    if (i + 1 != previewTargets.size()) propertiesSeparatorsY.push_back(ImGui::GetCursorPosY());
                    ImGui::SetCursorPosX(0);
                }

                float propertiesHeight = ImGui::GetCursorPosY() - firstCursorY;
                layersPropertiesOffset[i] = propertiesHeight;
                propertiesSeparatorsY.push_back(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
            } else layersPropertiesOffset[i] = 0;

            if (!active) {
                if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
                    ImGui::SetDragDropPayload(LEGEND_LAYER_DRAG_DROP, &i, sizeof(i));
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(LEGEND_LAYER_DRAG_DROP, ImGuiDragDropFlags_SourceNoDisableHover)) {
                        int from = *((int*) payload->Data);
                        int to = i;
                        std::swap(instance->graphics.layers[from], instance->graphics.layers[i]);
                    }
                    ImGui::EndDragDropTarget();
                }
            }

            propertiesCoordAcc += propertiesStep + layersPropertiesOffset[i];
        }
            
        ImGui::EndChild();
 
        ImGui::SameLine();

        ImGui::BeginChild("projectTimeline", ImVec2(canvasSize.x - legendSize.x, canvasSize.y), false, ImGuiWindowFlags_AlwaysHorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);
        windowMouseCoords = ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
        legendScrollY = ImGui::GetScrollY();
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
            RectBounds tickBounds = RectBounds(ImVec2(tickPositionAccumulator, 0 + ImGui::GetScrollY()), ImVec2(TIMELINE_TICK_WIDTH, majorTick ? canvasSize.y : 6.0f));
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
        RectBounds innerTicksZone = RectBounds(ImVec2(0 + ImGui::GetScrollX(), ticksBackgroundHeight + ImGui::GetScrollY() + 2), canvasSize);
        PushClipRect(innerTicksZone);

        static std::vector<float> layerSeparatorTargets{};
        static std::vector<RectBounds> layerPreviewHeights{};
        for (auto& height : layerPreviewHeights) {
            ImGui::SetCursorPos(height.pos);
            ImGui::Dummy(height.size);
            DrawRect(height, ImVec4(0.1f, 0.1f, 0.1f, 1));
        }
        for (auto& separatorY : propertiesSeparatorsY) {
            ImGui::SetCursorPos({0, 0});
            DrawRect(RectBounds(ImVec2(0 + ImGui::GetScrollX(), separatorY), ImVec2(canvasSize.x, 2.0f)), ImVec4(0, 0, 0, 1));
        }
        for (auto& separatorY : layerSeparatorTargets) {
            ImGui::SetCursorPos({0, 0});
            RectBounds separatorBounds = RectBounds(ImVec2(0 + ImGui::GetScrollX(), separatorY), ImVec2(canvasSize.x, 2.0f));
            DrawRect(separatorBounds, ImVec4(0, 0, 0, 1));
        }
        layerSeparatorTargets.clear();
        layerPreviewHeights.clear();
        int layerDeleteionTarget = -1;
        int layerDuplicationTarget = -1;
        int layerCopyTarget = -1;
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
                instance->selectedRenderLayer = layer->id;
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

            if (MouseHoveringBounds(forwardDragBounds) && !anyOtherButtonsDragged && !MouseHoveringBounds(ticksBackground) && !timelineDrag.isActive) {
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
                if (ImGui::Selectable(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_COPY"))) {
                    copyContainer = *layer;
                }
                if (ImGui::Selectable(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_PASTE"))) {
                    layerCopyTarget = i;
                }
                if (ImGui::Selectable(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_DUPLICATE"))) {
                    layerDuplicationTarget = i;
                }
                if (ImGui::Selectable(ELECTRON_GET_LOCALIZATION(instance, "GENERIC_DELETE"))) {
                    layerDeleteionTarget = i;
                    if (layer->id == instance->selectedRenderLayer) {
                        instance->selectedRenderLayer = -1;
                    }
                }
                ImGui::EndPopup();
            }
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

            ImGui::PopStyleColor();
            layerSeparatorTargets.push_back(layerOffsetY);
            layerPreviewHeights.push_back(RectBounds(ImVec2(0 + ImGui::GetScrollX(), layerOffsetY + layerSizeY), ImVec2(canvasSize.x, layersPropertiesOffset[i])));
            layerOffsetY += layerSizeY + layersPropertiesOffset[i];
        }


        ImGui::SetCursorPos({0, 0});
        RectBounds timelineBounds = RectBounds(ImVec2(pixelsPerFrame * instance->graphics.renderFrame, 0 + ImGui::GetScrollY()), ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
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
            instance->graphics.renderFrame = (int) ((windowMouseCoords.x) / pixelsPerFrame);
        } else timelineDrag.Deactivate();

        RectBounds endTimelineBounds = RectBounds(ImVec2(pixelsPerFrame * instance->graphics.renderLength, 0 + ImGui::GetScrollY()), ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
        DrawRect(endTimelineBounds, ImVec4(1, 1, 0, 1));
        PopClipRect();

        if (instance->graphics.isPlaying) {
            ImGui::SetScrollX(pixelsPerFrame * instance->graphics.renderFrame - ((canvasSize.x - legendSize.x) * 0.4f));
        }

        ImGui::EndChild();

        if (ImGui::Shortcut(ImGuiKey_Space | ImGuiMod_Ctrl)) {
            instance->graphics.firePlay = true;
        }

        ImGui::End();
        ImGui::PopStyleVar(2);
        if (layerDeleteionTarget != -1) {
            auto& layers = instance->graphics.layers;
            layers.erase(layers.begin() + layerDeleteionTarget);
        }

        if (layerDuplicationTarget != -1) {
            auto& layers = instance->graphics.layers;
            RenderLayer layer = layers[layerDuplicationTarget];
            layer.id = seedrand();
            layers.insert(layers.begin() + layerDuplicationTarget, layer);
        }

        if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_L | ImGuiKey_V)) {
            layerCopyTarget = instance->graphics.GetLayerIndexByID(instance->selectedRenderLayer);
        }

        if (layerCopyTarget != -1) {
            auto& layers = instance->graphics.layers;
            RenderLayer layer = copyContainer;
            layer.id = seedrand();
            layers.insert(layers.begin() + layerCopyTarget + 1, layer);
        }


    }
}