#include "app.h"
#include "drag_utils.h"
#include "editor_core.h"
#define LEGEND_LAYER_DRAG_DROP "LEGEND_LAYER_DRAG_DROP"

using namespace Electron;

extern "C" {
struct TimeStampTarget {
    int frame;
    ImVec2 position;

    TimeStampTarget() {}
};

struct KeyframeHolderInfo {
    float yCoord;
    float widgetHeight;
    std::vector<DragStructure> drags;

    KeyframeHolderInfo(){};
};

static void DrawRect(RectBounds bounds, ImVec4 color) {
    ImGui::GetWindowDrawList()->AddRectFilled(
        bounds.UL, bounds.BR, ImGui::ColorConvertFloat4ToU32(color));
}

static void PushClipRect(RectBounds bounds) {
    ImGui::GetWindowDrawList()->PushClipRect(bounds.UL, bounds.BR, true);
}

static void PopClipRect() { ImGui::GetWindowDrawList()->PopClipRect(); }

static bool MouseHoveringBounds(RectBounds bounds) {
    return ImGui::IsMouseHoveringRect(bounds.UL, bounds.BR);
}

static ImVec4 GetColor(ImGuiCol color) {
    ImVec4 col = ImGui::GetStyle().Colors[color];
    col.w = 1.0f;
    return col;
}


static float pixelsPerFrame = 3.0f;
static float beginPRF = 0;
static float targetPRF = 0;
static float percentagePRF = -1;

static void IncreasePixelsPerFrame() { 
    percentagePRF = 0;
    beginPRF = pixelsPerFrame;
    targetPRF = pixelsPerFrame + 0.2f;
}

static void DecreasePixelsPerFrame() { 
    percentagePRF = 0;
    beginPRF = pixelsPerFrame;
    targetPRF = pixelsPerFrame - 0.2f;
}

static void TimelineRenderLayerPopup(AppInstance *instance, int i,
                                     bool &firePopupsFlag,
                                     RenderLayer &copyContainer,
                                     int &layerDuplicationTarget,
                                     int &layerCopyTarget,
                                     int &layerDeleteionTarget) {
    RenderLayer *layer = &Shared::graphics->layers[i];
    ImGui::PopStyleVar(2);
    if (ImGui::BeginPopup(string_format("TimelineLayerPopup%i", i).c_str())) {
        firePopupsFlag = true;
        ImGui::SeparatorText(layer->layerUsername.c_str());
        if (ImGui::MenuItem(string_format("%s %s", ICON_FA_COPY,
                                          ELECTRON_GET_LOCALIZATION(
                                              "GENERIC_COPY"))
                                .c_str(),
                            "Ctrl+C")) {
            copyContainer = *layer;
        }
        if (ImGui::MenuItem(string_format("%s %s", ICON_FA_PASTE,
                                          ELECTRON_GET_LOCALIZATION(
                                              "GENERIC_PASTE"))
                                .c_str(),
                            "Ctrl+V")) {
            layerCopyTarget = i;
        }
        if (ImGui::MenuItem(string_format("%s %s", ICON_FA_PASTE,
                                          ELECTRON_GET_LOCALIZATION(
                                              "GENERIC_DUPLICATE"))
                                .c_str(),
                            "Ctrl+D")) {
            layerDuplicationTarget = i;
        }
        if (ImGui::MenuItem(string_format("%s %s", ICON_FA_TRASH_CAN,
                                          ELECTRON_GET_LOCALIZATION(
                                             "GENERIC_DELETE"))
                                .c_str(),
                            "Delete")) {
            layerDeleteionTarget = i;
            if (layer->id == Shared::selectedRenderLayer) {
                Shared::selectedRenderLayer = -1;
            }
        }
        ImGui::EndPopup();
    } else
        firePopupsFlag = false;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
}

static void TimelienRenderDragNDropTarget(AppInstance *instance, int &i) {
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                LEGEND_LAYER_DRAG_DROP,
                ImGuiDragDropFlags_SourceNoDisableHover)) {
            int from = *((int *)payload->Data);
            int to = i;
            std::swap(Shared::graphics->layers[from],
                      Shared::graphics->layers[i]);
        }
        ImGui::EndDragDropTarget();
    }
}

static bool TimelineRenderDragNDrop(AppInstance *instance, int &i) {
    bool dragging = false;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
        ImGui::SetDragDropPayload(LEGEND_LAYER_DRAG_DROP, &i, sizeof(i));
        dragging = true;
        ImGui::EndDragDropSource();
    }
    TimelienRenderDragNDropTarget(instance, i);
    return dragging;
}

ELECTRON_EXPORT void UIRender(AppInstance *instance) {
    ImGui::SetCurrentContext(instance->context);
    bool pOpen = true;
    bool anyLayerDragged = false;

    ImGuiStyle& style = ImGui::GetStyle();
    static RenderLayer copyContainer;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::Begin((std::string(ICON_FA_TIMELINE " ") +
                  ELECTRON_GET_LOCALIZATION("TIMELINE_TITLE"))
                     .c_str(),
                 &pOpen, 0);
    if (!pOpen) {
        ImGui::End();
        throw Signal::_CloseWindow;
    }
    if (!instance->projectOpened) {
        ImVec2 windowSize = ImGui::GetWindowSize();
        std::string noProjectOpened =
            ELECTRON_GET_LOCALIZATION("GENERIC_NO_PROJECT_IS_OPENED");
        ImVec2 textSize = ImGui::CalcTextSize(noProjectOpened.c_str());
        ImGui::SetCursorPos(ImVec2{windowSize.x / 2.0f - textSize.x / 2.0f,
                                   windowSize.y / 2.0f - textSize.y / 2.0f});
        ImGui::Text("%s", noProjectOpened.c_str());
        ImGui::End();
        ImGui::PopStyleVar(2);
        return;
    }
    static bool prfSetup = false;
    if (!prfSetup) {
        pixelsPerFrame = JSON_AS_TYPE(Shared::project.propertiesMap["TimelineZoom"], float);
        prfSetup = true;
    }
    Shared::project.propertiesMap["TimelineZoom"] = pixelsPerFrame;
    bool isWindowFocused = ImGui::IsWindowFocused();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImDrawList *drawList = ImGui::GetWindowDrawList();
    ImVec2 windowMouseCoords =
        ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
    pixelsPerFrame = glm::clamp(pixelsPerFrame, 0.5f, 10.0f);
    if (percentagePRF >= 0) {
        pixelsPerFrame = ImLerp(beginPRF, targetPRF, percentagePRF);
        percentagePRF += 0.05f;
        percentagePRF = glm::clamp(percentagePRF, 0.0f, 1.0f);
    }
    if (percentagePRF >= 1.0f) {
        percentagePRF = -1;
    }

    static DragStructure timelineDrag{};
    static std::vector<float> layersPropertiesOffset{};
    if (layersPropertiesOffset.size() != Shared::graphics->layers.size()) {
        layersPropertiesOffset =
            std::vector<float>(Shared::graphics->layers.size());
    }

    static float legendWidth =
        JSON_AS_TYPE(Shared::project.propertiesMap["LegendWidth"], float);
    Shared::project.propertiesMap["LegendWidth"] = legendWidth;
    legendWidth = glm::clamp(legendWidth, 0.1f, 0.8f);
    ImVec2 legendSize(canvasSize.x * legendWidth, canvasSize.y);
    float ticksBackgroundHeight = canvasSize.y * 0.05f;
    RectBounds fillerTicksBackground =
        RectBounds(ImVec2(0, 0 + ImGui::GetScrollY()),
                   ImVec2(legendSize.x, ticksBackgroundHeight));
    RectBounds ticksBackground =
        RectBounds(ImVec2(0, 2 + ImGui::GetScrollY()),
                   ImVec2(canvasSize.x, ticksBackgroundHeight));
    RectBounds fullscreenTicksMask =
        RectBounds(ImVec2(0, 2), ImVec2(canvasSize.x, canvasSize.y));
    static float legendScrollY = 0;

    std::vector<float> propertiesSeparatorsY{};
    int layerDeleteionTarget = -1;
    int layerDuplicationTarget = -1;
    int layerCopyTarget = -1;

    bool anyPopupsOpen = false;
    bool firePopupsFlag = false;
    bool blockTimelineDrag = false;
    if (firePopupsFlag)
        anyPopupsOpen = true;

    static std::unordered_map<int, std::vector<KeyframeHolderInfo>>
        keyframeInfos{};
    ImGui::BeginChild("projectlegend", legendSize, true,
                      ImGuiWindowFlags_NoScrollbar);
    ImGui::SetScrollY(legendScrollY);
    DrawRect(fillerTicksBackground, style.Colors[ImGuiCol_WindowBg] * 0.7f);
    float propertiesStep =
        glm::floor(22 * JSON_AS_TYPE(Shared::configMap["UIScaling"], float));
    float propertiesCoordAcc = 0;
    for (int i = Shared::graphics->layers.size() - 1; i >= 0; i--) {
        RenderLayer *layer = &Shared::graphics->layers[i];
        if (keyframeInfos.find(layer->id) == keyframeInfos.end()) {
            keyframeInfos[layer->id] = std::vector<KeyframeHolderInfo>(
                layer->previewProperties.size());
        }
        std::vector<KeyframeHolderInfo> &info = keyframeInfos[layer->id];
        ImGui::SetCursorPosY(ticksBackgroundHeight + 2 + propertiesCoordAcc);
        bool active = false;
        ImGuiDragDropFlags src_flags = 0;
        src_flags |= ImGuiDragDropFlags_SourceNoDisableHover;
        src_flags |= ImGuiDragDropFlags_SourceNoHoldToOpenOthers;
        ImGuiDragDropFlags target_flags = 0;
        target_flags |= ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        bool visible = layer->visible;
        if (visible) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.17f, 0.17f, 0.17f, 1));
        if (ImGui::Button(string_format("%s", layer->visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH).c_str())) {
            layer->visible = !layer->visible;
        }
        if (visible) ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::CollapsingHeader(
                (layer->layerUsername + "##" + std::to_string(i)).c_str())) {
            ImGui::Spacing();
            if (ImGui::IsItemHovered() &&
                ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                anyPopupsOpen = true;
                ImGui::OpenPopup(
                    string_format("TimelineLayerPopup%i", i).c_str());
            }
            TimelineRenderLayerPopup(instance, i, firePopupsFlag, copyContainer,
                                     layerDuplicationTarget, layerCopyTarget,
                                     layerDeleteionTarget);
            active = true;
            TimelineRenderDragNDrop(instance, i);
            ImGui::PopStyleVar(2);
            float firstCursorY = ImGui::GetCursorPosY();

            json_t previewTargets = layer->previewProperties;
            float layerViewTime =
                Shared::graphics->renderFrame - layer->beginFrame;
            for (int i = 0; i < previewTargets.size(); i++) {
                json_t &propertyMap = layer->properties[previewTargets.at(i)];
                GeneralizedPropertyType propertyType =
                    static_cast<GeneralizedPropertyType>(
                        JSON_AS_TYPE(propertyMap.at(0), int));
                KeyframeHolderInfo &holderInfo = info[i];
                bool keyframeAlreadyExists = false;
                bool customKeyframesExist = (propertyMap.size() - 1) > 1;
                int keyframeIndex = 1;
                for (int j = 1; j < propertyMap.size(); j++) {
                    json_t specificKeyframe = propertyMap.at(j);
                    if (JSON_AS_TYPE(specificKeyframe.at(0), int) ==
                        (int)layerViewTime) {
                        keyframeAlreadyExists = true;
                        keyframeIndex = j;
                        break;
                    }
                }
                ImGui::SetCursorPosX(8.0f);
                int propertySize = 1;
                float beginWidgetY = ImGui::GetCursorPosY();
                holderInfo.yCoord = ImGui::GetCursorPosY();
                switch (propertyType) {
                case GeneralizedPropertyType::Float: {
                    float x = JSON_AS_TYPE(
                        layer->InterpolateProperty(propertyMap).at(0), float);
                    if (ImGui::InputFloat(
                            (JSON_AS_TYPE(previewTargets.at(i), std::string) +
                             "##" + std::to_string(i))
                                .c_str(),
                            &x, 0, 0, "%.3f",
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (!keyframeAlreadyExists && customKeyframesExist) {
                            propertyMap.push_back({(int)layerViewTime, x});
                        } else {
                            propertyMap.at(keyframeIndex).at(1) = x;
                        }
                    }
                    propertySize = 1;
                    break;
                }
                case GeneralizedPropertyType::Vec2: {
                    json_t x = layer->InterpolateProperty(propertyMap);
                    std::vector<float> raw = {JSON_AS_TYPE(x.at(0), float),
                                              JSON_AS_TYPE(x.at(1), float)};
                    if (ImGui::InputFloat2(
                            (JSON_AS_TYPE(previewTargets.at(i), std::string) +
                             "##" + std::to_string(i))
                                .c_str(),
                            raw.data(), "%.3f",
                            ImGuiInputTextFlags_EnterReturnsTrue)) {
                        if (!keyframeAlreadyExists && customKeyframesExist) {
                            propertyMap.push_back(
                                {(int)layerViewTime, raw.at(0), raw.at(1)});
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
                    std::vector<float> raw = {JSON_AS_TYPE(x.at(0), float),
                                              JSON_AS_TYPE(x.at(1), float),
                                              JSON_AS_TYPE(x.at(2), float)};
                    if (propertyType == GeneralizedPropertyType::Vec3
                            ? ImGui::InputFloat3(
                                  (JSON_AS_TYPE(previewTargets.at(i),
                                                std::string) +
                                   "##" + std::to_string(i))
                                      .c_str(),
                                  raw.data(), "%.3f",
                                  ImGuiInputTextFlags_EnterReturnsTrue)
                            : ImGui::ColorEdit3(
                                  (JSON_AS_TYPE(previewTargets.at(i),
                                                std::string) +
                                   "##" + std::to_string(i))
                                      .c_str(),
                                  raw.data(), 0)) {
                        if (!keyframeAlreadyExists && customKeyframesExist) {
                            propertyMap.push_back({(int)layerViewTime,
                                                   raw.at(0), raw.at(1),
                                                   raw.at(1)});
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
                if (ImGui::IsItemHovered() &&
                    ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                    ImGui::OpenPopup(
                        string_format("LegendProperty%i%i", i, layer->id)
                            .c_str());
                }
                if (ImGui::BeginPopup(
                        string_format("LegendProperty%i%i", i, layer->id)
                            .c_str())) {
                    anyPopupsOpen = true;
                    ImGui::SeparatorText(
                        string_format("%s", (JSON_AS_TYPE(previewTargets.at(i),
                                                          std::string))
                                                .c_str())
                            .c_str());
                    if (ImGui::MenuItem(
                            string_format(
                                "%s %s", ICON_FA_PLUS,
                                ELECTRON_GET_LOCALIZATION(
                                    "TIMELINE_ADD_KEYFRAME"))
                                .c_str())) {
                        json_t xKeyframe = propertyMap.at(keyframeIndex);
                        xKeyframe.at(0) = (int)layerViewTime;
                        propertyMap.push_back(xKeyframe);
                    }
                    ImGui::EndPopup();
                }
                if (i + 1 != previewTargets.size())
                    propertiesSeparatorsY.push_back(ImGui::GetCursorPosY());
                ImGui::SetCursorPosX(0);
                holderInfo.widgetHeight = ImGui::GetCursorPosY() - beginWidgetY;
            }

            float propertiesHeight = ImGui::GetCursorPosY() - firstCursorY;
            layersPropertiesOffset[i] = propertiesHeight;
            propertiesSeparatorsY.push_back(ImGui::GetCursorPosY() -
                                            ImGui::GetStyle().ItemSpacing.y);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        } else {
            layersPropertiesOffset[i] = 0;
            keyframeInfos.erase(layer->id);
        }

        if (!active) {
            TimelineRenderDragNDrop(instance, i);
            if (ImGui::IsItemHovered() &&
                ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                ImGui::OpenPopup(
                    string_format("TimelineLayerPopup%i", i).c_str());
            }
            TimelineRenderLayerPopup(instance, i, firePopupsFlag, copyContainer,
                                     layerDuplicationTarget, layerCopyTarget,
                                     layerDeleteionTarget);
        }

        propertiesCoordAcc += propertiesStep + layersPropertiesOffset[i];
    }

    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("projectTimeline",
                      ImVec2(canvasSize.x - legendSize.x, canvasSize.y), false,
                      ImGuiWindowFlags_AlwaysHorizontalScrollbar |
                          ImGuiWindowFlags_AlwaysVerticalScrollbar);
    windowMouseCoords = ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
    legendScrollY = ImGui::GetScrollY();
    bool anyOtherButtonsDragged = false;
    bool anyKeyframesDragged = false;
    static std::vector<DragStructure> universalLayerDrags;
    static std::vector<DragStructure> forwardLayerDrags;
    static std::vector<DragStructure> backwardLayerDrags;
    for (auto &drag : universalLayerDrags) {
        if (drag.isActive)
            anyOtherButtonsDragged = true;
    }

    for (auto &drag : forwardLayerDrags) {
        if (drag.isActive)
            anyOtherButtonsDragged = true;
    }

    for (auto &drag : backwardLayerDrags) {
        if (drag.isActive)
            anyOtherButtonsDragged = true;
    }
    for (auto &keyPairs : keyframeInfos) {
        bool oneDragActive = false;
        for (auto &info : keyPairs.second) {
            for (auto &keyDrag : info.drags) {
                if (keyDrag.isActive) {
                    anyOtherButtonsDragged = true;
                    if (oneDragActive) {
                        anyKeyframesDragged = true;
                    } else
                        oneDragActive = true;
                }
            }
        }
    }
    PushClipRect(fullscreenTicksMask);
    std::vector<TimeStampTarget> stamps{};
    DrawRect(ticksBackground, style.Colors[ImGuiCol_WindowBg] * 0.5f);
    int desiredTicksCount = pixelsPerFrame * 2;
    float tickStep =
        (float)Shared::graphics->renderFramerate / (float)desiredTicksCount;
    float tickPositionStep = tickStep * pixelsPerFrame;
    float tickPositionAccumulator = 0;
    float tickAccumulator = 0;
    while (tickAccumulator <= Shared::graphics->renderLength) {
        bool majorTick = remainder(tickAccumulator,
                                   Shared::graphics->renderFramerate) == 0.0f;
        float tickHeight = majorTick ? 0 : 2.0f;
        RectBounds tickBounds = RectBounds(
            ImVec2(tickPositionAccumulator, 0 + ImGui::GetScrollY()),
            ImVec2(TIMELINE_TICK_WIDTH, majorTick ? canvasSize.y : 6.0f));
        DrawRect(tickBounds, style.Colors[ImGuiCol_Border] * 0.7f);

        if (majorTick) {
            TimeStampTarget stamp{};
            stamp.frame = tickAccumulator;
            stamp.position = ImVec2(tickPositionAccumulator, 0);
            stamps.push_back(stamp);
        }

        tickPositionAccumulator += tickPositionStep;
        tickAccumulator += tickStep;
    }

    for (auto &stamp : stamps) {
        ImGui::GetWindowDrawList()->AddText(
            canvasPos + stamp.position +
                ImVec2(legendSize.x - ImGui::GetScrollX(), 0),
            ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1)),
            formatToTimestamp(stamp.frame, Shared::graphics->renderFramerate)
                .c_str());
    }
    PopClipRect();

    float layerOffsetY = ticksBackgroundHeight;
    float layerSizeY =
        22 * JSON_AS_TYPE(Shared::configMap["UIScaling"], float);
    if (universalLayerDrags.size() != Shared::graphics->layers.size()) {
        universalLayerDrags =
            std::vector<DragStructure>(Shared::graphics->layers.size());
    }
    if (forwardLayerDrags.size() != Shared::graphics->layers.size()) {
        forwardLayerDrags =
            std::vector<DragStructure>(Shared::graphics->layers.size());
    }
    if (backwardLayerDrags.size() != Shared::graphics->layers.size()) {
        backwardLayerDrags =
            std::vector<DragStructure>(Shared::graphics->layers.size());
    }
    RectBounds innerTicksZone =
        RectBounds(ImVec2(0 + ImGui::GetScrollX(),
                          ticksBackgroundHeight + ImGui::GetScrollY() + 2),
                   canvasSize);
    PushClipRect(innerTicksZone);

    bool anyLayerHovered = false;

    static std::vector<float> layerSeparatorTargets{};
    static std::vector<RectBounds> layerPreviewHeights{};
    for (auto &separatorY : propertiesSeparatorsY) {
        ImGui::SetCursorPos({0, 0});
        DrawRect(RectBounds(ImVec2(0 + ImGui::GetScrollX(), separatorY),
                            ImVec2(canvasSize.x, 1.0f)),
                 style.Colors[ImGuiCol_Border]);
    }
    for (auto &layerPair : keyframeInfos) {
        RenderLayer *keyframesOwner =
            Shared::graphics->GetLayerByID(layerPair.first);
        PushClipRect(RectBounds(
            ImVec2(keyframesOwner->beginFrame * pixelsPerFrame, 0),
            ImVec2((keyframesOwner->endFrame - keyframesOwner->beginFrame) *
                       pixelsPerFrame,
                   ImGui::GetWindowSize().y)));
        std::vector<KeyframeHolderInfo> info = layerPair.second;
        for (int i = 0; i < keyframesOwner->previewProperties.size(); i++) {
            KeyframeHolderInfo keyInfo = info.at(i);
            json_t propertyMap =
                keyframesOwner
                    ->properties[keyframesOwner->previewProperties.at(i)];
            int keyframeDeletionTarget = -1;
            int keyframeDuplicationTarget = -1;
            for (int j = 1; j < propertyMap.size(); j++) {
                json_t &keyframe = propertyMap.at(j);
                float time = JSON_AS_TYPE(keyframe.at(0), float);
                if (keyInfo.drags.size() != propertyMap.size() - 1) {
                    keyInfo.drags =
                        std::vector<DragStructure>(propertyMap.size() - 1);
                }
                DragStructure &drag = keyInfo.drags[j - 1];
                ImVec2 keyframeSize =
                    ImVec2(TTIMELINE_RULLER_WIDTH * 2.0f, keyInfo.widgetHeight);
                ImVec2 logicSize =
                    ImVec2(ImGui::GetWindowSize().x + ImGui::GetScrollX(),
                           ImGui::GetWindowSize().y + ImGui::GetScrollY());
                RectBounds keyframeBounds = RectBounds(
                    ImVec2((keyframesOwner->beginFrame * pixelsPerFrame +
                            time * pixelsPerFrame) -
                               keyframeSize.x / 2.0f,
                           keyInfo.yCoord),
                    keyframeSize);
                RectBounds logicBounds = RectBounds(
                    ImVec2((keyframesOwner->beginFrame * pixelsPerFrame +
                            time * pixelsPerFrame) -
                               logicSize.x / 2.0f,
                           0),
                    logicSize);

                if (MouseHoveringBounds(keyframeBounds) && !drag.isActive &&
                    ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] && ImGui::IsWindowFocused() &&
                    !anyKeyframesDragged) {
                    drag.isActive = true;
                }

                if (MouseHoveringBounds(keyframeBounds) &&
                    ImGui::GetIO().MouseDown[ImGuiMouseButton_Right] &&
                    !anyOtherButtonsDragged) {
                    ImGui::OpenPopup(string_format("KeyframePopup%p%i%i",
                                                   keyframesOwner, i, j)
                                         .c_str());
                }

                ImGui::PopStyleVar(2);
                if (ImGui::BeginPopup(string_format("KeyframePopup%p%i%i",
                                                    keyframesOwner, i, j)
                                          .c_str())) {
                    anyPopupsOpen = true;
                    ImGui::SeparatorText(
                        string_format(
                            "%s %i",
                            JSON_AS_TYPE(
                                keyframesOwner->previewProperties.at(i),
                                std::string)
                                .c_str(),
                            j)
                            .c_str());
                    if (ImGui::MenuItem(
                            string_format("%s %s", ICON_FA_TRASH,
                                          ELECTRON_GET_LOCALIZATION(
                                              "GENERIC_DELETE"))
                                .c_str())) {
                        keyframeDeletionTarget = j;
                        DUMP_VAR(keyframeDeletionTarget);
                    }
                    if (ImGui::MenuItem(
                            string_format("%s %s", ICON_FA_PASTE,
                                          ELECTRON_GET_LOCALIZATION(
                                              "GENERIC_DUPLICATE"))
                                .c_str())) {
                        keyframeDuplicationTarget = j;
                    }
                    ImGui::EndPopup();
                }
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

                float dragDist;
                if (drag.isActive && MouseHoveringBounds(fullscreenTicksMask) &&
                    ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] &&
                    !anyKeyframesDragged) {
                    blockTimelineDrag = true;
                    float dragWindowCoord =
                        windowMouseCoords.x + ImGui::GetScrollX();

                    float transformedFrame =
                        ((dragWindowCoord / pixelsPerFrame) -
                         keyframesOwner->beginFrame);
                    transformedFrame =
                        glm::clamp((float)transformedFrame, 0.0f,
                                   (float)keyframesOwner->endFrame -
                                       keyframesOwner->beginFrame);
                    keyframe.at(0) = glm::floor(transformedFrame);
                    anyLayerDragged = true;
                    anyLayerHovered = true;
                } else {
                    drag.isActive = false;
                    anyLayerDragged = false;
                }
                DrawRect(keyframeBounds, MouseHoveringBounds(keyframeBounds)
                                             ? ImVec4(0.25f, 0.25f, 0.25f, 1)
                                             : ImVec4(0.2f, 0.2f, 0.2f, 1));
                propertyMap.at(j) = keyframe;
            }
            if (keyframeDeletionTarget != -1)
                propertyMap.erase(keyframeDeletionTarget);
            if (keyframeDuplicationTarget != -1) {
                json_t keyframeInstance =
                    propertyMap.at(keyframeDuplicationTarget);
                propertyMap.insert(propertyMap.begin() +
                                       keyframeDuplicationTarget + 1,
                                   keyframeInstance);
            }
            keyframesOwner
                ->properties[keyframesOwner->previewProperties.at(i)] =
                propertyMap;
            info.at(i) = keyInfo;
        }
        PopClipRect();

        keyframeInfos[layerPair.first] = info;
    }
    for (auto &separatorY : layerSeparatorTargets) {
        ImGui::SetCursorPos({0, 0});
        RectBounds separatorBounds =
            RectBounds(ImVec2(0 + ImGui::GetScrollX(), separatorY),
                       ImVec2(canvasSize.x, 2.0f));
        DrawRect(separatorBounds, ImVec4(0, 0, 0, 1));
    }
    float dragAccepterOffset = ticksBackgroundHeight;
    for (int i = Shared::graphics->layers.size() - 1; i >= 0; i--) {
        ImGui::SetCursorPos({0 + ImGui::GetScrollX(), dragAccepterOffset});
        ImGui::Dummy({ImGui::GetWindowSize().x, layerSizeY});
        if (ImGui::GetIO().KeyCtrl) {
            TimelienRenderDragNDropTarget(instance, i);
        }
        dragAccepterOffset += layerSizeY + layersPropertiesOffset[i];
    }
    layerSeparatorTargets.clear();
    layerPreviewHeights.clear();
    if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_KeypadAdd)) {
        IncreasePixelsPerFrame();
    }

    if (ImGui::Shortcut(ImGuiKey_ModCtrl | ImGuiKey_KeypadSubtract)) {
        DecreasePixelsPerFrame();
    }
    static std::vector<int> multipleDragSelectedLayers{};
    for (int i = Shared::graphics->layers.size() - 1; i >= 0; i--) {
        RenderLayer *layer = &Shared::graphics->layers[i];
        DragStructure &universalDrag = universalLayerDrags[i];
        DragStructure &forwardDrag = forwardLayerDrags[i];
        DragStructure &backwardDrag = backwardLayerDrags[i];
        bool selected = false;
        float layerDuration = layer->endFrame - layer->beginFrame;
        ImVec4 layerColor = ImVec4{layer->layerColor.r, layer->layerColor.g,
                                   layer->layerColor.b, layer->layerColor.a};
        if (std::count(multipleDragSelectedLayers.begin(), multipleDragSelectedLayers.end(), i)) {
            layerColor = ImVec4(0.8f, 0.8f, 0.0f, 1);
            selected = true;
        }
        ImGui::SetCursorPosY(layerOffsetY + 2);
        ImGui::SetCursorPosX(pixelsPerFrame * layer->beginFrame);
        ImGui::PushStyleColor(ImGuiCol_Button, layerColor);
        if (ImGui::Button(
                (layer->layerUsername + "##" + std::to_string(i)).c_str(),
                ImVec2(pixelsPerFrame * layerDuration, layerSizeY))) {
            if (ImGui::GetIO().KeyCtrl && !selected && !universalDrag.isActive && !backwardDrag.isActive && !forwardDrag.isActive) {
                multipleDragSelectedLayers.push_back(i);
            } else if (ImGui::GetIO().KeyCtrl && selected) {
                multipleDragSelectedLayers.erase(std::find(multipleDragSelectedLayers.begin(), multipleDragSelectedLayers.end(), i));
            }
            if (selected && !ImGui::GetIO().KeyCtrl) multipleDragSelectedLayers.clear();

            Shared::selectedRenderLayer = layer->id;
        }
        if (ImGui::GetIO().KeyCtrl)
            bool dragged = TimelineRenderDragNDrop(instance, i);

        if (ImGui::IsItemHovered()) {
            anyLayerHovered = true;
        }
        if (ImGui::IsItemHovered() &&
            ImGui::GetIO().MouseClicked[ImGuiMouseButton_Right]) {
            ImGui::OpenPopup(string_format("TimelineLayerPopup%i", i).c_str());
            anyPopupsOpen = true;
        }
        TimelineRenderLayerPopup(instance, i, firePopupsFlag, copyContainer,
                                 layerDuplicationTarget, layerCopyTarget,
                                 layerDeleteionTarget);
        ImGui::SetCursorPos(ImVec2{0, 0});
        ImVec2 dragSize =
            ImVec2(layerDuration * pixelsPerFrame / 10, layerSizeY);
        dragSize.x = glm::clamp(dragSize.x, 1.0f, 30.0f);
        RectBounds forwardDragBounds =
            RectBounds(ImVec2(pixelsPerFrame * layer->beginFrame +
                                  pixelsPerFrame * layerDuration - dragSize.x,
                              layerOffsetY + 2),
                       dragSize);
        DrawRect(forwardDragBounds, layerColor * ImVec4(0.9f, 0.9f, 0.9f, 1));
        RectBounds backwardDragBounds = RectBounds(
            ImVec2(pixelsPerFrame * layer->beginFrame, layerOffsetY + 2),
            dragSize);
        DrawRect(backwardDragBounds, layerColor * ImVec4(0.9f, 0.9f, 0.9f, 1));

        if (MouseHoveringBounds(forwardDragBounds) && !anyOtherButtonsDragged &&
            !MouseHoveringBounds(ticksBackground) && !timelineDrag.isActive &&
            !ImGui::GetIO().KeyCtrl) {
            forwardDrag.Activate();
        }

        if (MouseHoveringBounds(backwardDragBounds) &&
            !anyOtherButtonsDragged && !timelineDrag.isActive &&
            !ImGui::GetIO().KeyCtrl) {
            backwardDrag.Activate();
        }

        if (ImGui::IsItemHovered() && !anyOtherButtonsDragged &&
            !timelineDrag.isActive && !forwardDrag.isActive &&
            !ImGui::GetIO().KeyCtrl) {
            universalDrag.Activate();
        }

        float universalDragDistance;
        float forwardDragDistance;
        float backwardDragDistance;

        if (forwardDrag.isActive) forwardDragDistance = ImGui::GetIO().MouseDelta.x;

        if (ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] && forwardDrag.isActive && 
            !timelineDrag.isActive && !ImGui::GetIO().KeyCtrl) {
            layer->endFrame += forwardDragDistance / pixelsPerFrame;
            if (windowMouseCoords.x > ImGui::GetWindowSize().x - ImGui::GetWindowSize().x / 10) {
                ImGui::SetScrollX(ImGui::GetScrollX() + 1);
            }
            if (windowMouseCoords.x < ImGui::GetWindowSize().x / 10) {
                ImGui::SetScrollX(ImGui::GetScrollX() - 1);
            }
        } else
            forwardDrag.Deactivate();

        if (backwardDrag.isActive) backwardDragDistance = ImGui::GetIO().MouseDelta.x;

        if (ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] && backwardDrag.isActive &&
            !timelineDrag.isActive && !ImGui::GetIO().KeyCtrl) {
            layer->beginFrame += backwardDragDistance / pixelsPerFrame;
            if (windowMouseCoords.x > ImGui::GetWindowSize().x - ImGui::GetWindowSize().x / 10) {
                ImGui::SetScrollX(ImGui::GetScrollX() + 1);
            }
            if (windowMouseCoords.x < ImGui::GetWindowSize().x / 10) {
                ImGui::SetScrollX(ImGui::GetScrollX() - 1);
            }
        } else
            backwardDrag.Deactivate();

        if (forwardDrag.isActive || backwardDrag.isActive) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        RectBounds layerFullheightBounds =
            RectBounds(ImVec2(0 + ImGui::GetScrollX(), 0), ImGui::GetWindowSize());
        if ((((universalDrag.GetDragDistance(universalDragDistance) &&
              universalDragDistance != 0 && !timelineDrag.isActive) ||
             (MouseHoveringBounds(layerFullheightBounds) &&
              universalDrag.isActive &&
              ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])) && !selected) &&
            backwardDrag.isActive == false) {
            universalDragDistance = ImGui::GetIO().MouseDelta.x;
            float tempBeginFrame = layer->beginFrame + universalDragDistance / pixelsPerFrame;
            float tempEndFrame = layer->endFrame + universalDragDistance / pixelsPerFrame;
            if (tempBeginFrame >= 0) {
                layer->beginFrame = tempBeginFrame;
                layer->endFrame = tempEndFrame;
            }

            layer->beginFrame = glm::max(layer->beginFrame, 0);
            layer->endFrame = glm::max(layer->endFrame, 0);
            timelineDrag.Deactivate();
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (windowMouseCoords.x > ImGui::GetWindowSize().x - ImGui::GetWindowSize().x / 10) {
                ImGui::SetScrollX(ImGui::GetScrollX() + 1);
            }
            if (windowMouseCoords.x < ImGui::GetWindowSize().x / 10) {
                ImGui::SetScrollX(ImGui::GetScrollX() - 1);
            }
        } else if (!selected)
            universalDrag.Deactivate();

        if ((((universalDrag.GetDragDistance(universalDragDistance) &&
              universalDragDistance != 0 && !timelineDrag.isActive) ||
             (MouseHoveringBounds(layerFullheightBounds) &&
              universalDrag.isActive &&
              ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])) && selected) &&
            backwardDrag.isActive == false) {
                universalDragDistance = ImGui::GetIO().MouseDelta.x;
                int frameAddition = universalDragDistance / pixelsPerFrame;
                for (auto& selectedLayer : multipleDragSelectedLayers) {
                    RenderLayer* l = &Shared::graphics->layers[selectedLayer];
                    l->beginFrame += frameAddition;
                    l->endFrame += frameAddition;
                }
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            }

        ImGui::PopStyleColor();
        layerSeparatorTargets.push_back(layerOffsetY);
        layerPreviewHeights.push_back(RectBounds(
            ImVec2(0, layerOffsetY + layerSizeY),
            ImVec2(canvasSize.x, layersPropertiesOffset[i])));
        layerOffsetY += layerSizeY + layersPropertiesOffset[i];
    }

    ImGui::SetCursorPos({0, 0});
    RectBounds timelineBounds =
        RectBounds(ImVec2(pixelsPerFrame * Shared::graphics->renderFrame,
                          0 + ImGui::GetScrollY()),
                   ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
    DrawRect(timelineBounds, ImVec4(0.871, 0.204, 0.204, 1));

    for (auto &drag : universalLayerDrags) {
        if (drag.isActive)
            anyLayerDragged = true;
    }
    for (auto &drag : forwardLayerDrags) {
        if (drag.isActive)
            anyLayerDragged = true;
    }

    for (auto &drag : backwardLayerDrags) {
        if (drag.isActive)
            anyLayerDragged = true;
    }

    if (MouseHoveringBounds(timelineBounds) ||
        (MouseHoveringBounds(ticksBackground) && !anyLayerDragged &&
         !anyOtherButtonsDragged)) {
        timelineDrag.Activate();
    }

    float timelineDragDist;
    if (timelineDrag.GetDragDistance(timelineDragDist) && !anyLayerDragged &&
        !anyKeyframesDragged && !blockTimelineDrag) {
        Shared::graphics->renderFrame =
            (int)((windowMouseCoords.x) / pixelsPerFrame);
        Shared::graphics->FireTimelineSeek();
    } else
        timelineDrag.Deactivate();

    RectBounds endTimelineBounds =
        RectBounds(ImVec2(pixelsPerFrame * Shared::graphics->renderLength,
                          0 + ImGui::GetScrollY()),
                   ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
    DrawRect(endTimelineBounds, ImVec4(1, 1, 0, 1));
    PopClipRect();

    if (Shared::graphics->isPlaying) {
        ImGui::SetScrollX(pixelsPerFrame * Shared::graphics->renderFrame -
                          ((canvasSize.x - legendSize.x) * 0.4f));
    }

    if (MouseHoveringBounds(fullscreenTicksMask) &&
        !anyLayerHovered &&
        !anyLayerDragged && !anyPopupsOpen &&
        ImGui::GetIO().MouseClicked[ImGuiMouseButton_Right]) {
        ImGui::OpenPopup("TimelinePopup");
    }

    ImGui::PopStyleVar(2);
    if (ImGui::BeginPopup("TimelinePopup")) {
        anyPopupsOpen = true;
        ImGui::SeparatorText(string_format("%s %s", ICON_FA_TIMELINE, ELECTRON_GET_LOCALIZATION("TIMELINE_TITLE")).c_str());
        if (ImGui::BeginMenu(
                ELECTRON_GET_LOCALIZATION("TIMELINE_ADD_LAYER"))) {
            for (auto &entry : GraphicsCore::GetImplementationsRegistry()) {
                Libraries::LoadLibrary("layers", entry);
                
                if (ImGui::MenuItem(
                        string_format("%s %s (%s)", ICON_FA_PLUS, Libraries::GetVariable<std::string>(entry, "LayerName").c_str(), entry.c_str()).c_str())) {
                    Shared::graphics->AddRenderLayer(entry);
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndPopup();
    }
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    if (ImGui::Shortcut(ImGuiKey_Space | ImGuiMod_Ctrl)) {
        Shared::graphics->isPlaying = !Shared::graphics->isPlaying;
        // Trigger LayerOnPlaybackChange
        Shared::graphics->FirePlaybackChange();
    }

    if (ImGui::Shortcut(ImGuiKey_C | ImGuiMod_Ctrl)) {
        copyContainer =
            *Shared::graphics->GetLayerByID(Shared::selectedRenderLayer);
        print("Copied layer with ID " << copyContainer.id);
    }

    if (ImGui::Shortcut(ImGuiKey_V | ImGuiMod_Ctrl) &&
        copyContainer.initialized) {
        layerCopyTarget =
            Shared::graphics->GetLayerIndexByID(Shared::selectedRenderLayer);
    }

    if (ImGui::Shortcut(ImGuiKey_D | ImGuiMod_Ctrl)) {
        layerDuplicationTarget =
            Shared::graphics->GetLayerIndexByID(Shared::selectedRenderLayer);
    }

    if (ImGui::Shortcut(ImGuiKey_Delete) && Shared::selectedRenderLayer > 0 &&
         multipleDragSelectedLayers.empty()) {
        layerDeleteionTarget =
            Shared::graphics->GetLayerIndexByID(Shared::selectedRenderLayer);
        Shared::selectedRenderLayer = -1;
    }

    if (ImGui::Shortcut(ImGuiKey_Delete) && Shared::selectedRenderLayer > 0 && !multipleDragSelectedLayers.empty()) {
        std::vector<int> layerIndices{};
        for (auto& index : multipleDragSelectedLayers) {
            layerIndices.push_back(Shared::graphics->layers[index].id);
        }
        for (auto& layerIndex : layerIndices) {
            auto& layers = Shared::graphics->layers;
            layers[Shared::graphics->GetLayerIndexByID(layerIndex)].Destroy();
            layers.erase(layers.begin() + Shared::graphics->GetLayerIndexByID(layerIndex));
        }
        Shared::selectedRenderLayer = -1;
    }  
    ImGui::EndChild();

    ImVec2 oldCursor = ImGui::GetCursorPos();

    ImGui::SetCursorPos({0, 0});
    windowMouseCoords = ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
    RectBounds legendResizer = RectBounds(
        ImVec2(legendWidth * ImGui::GetWindowSize().x -
                   (TTIMELINE_RULLER_WIDTH * 2.0f / 2.0f),
               0),
        ImVec2(TTIMELINE_RULLER_WIDTH * 2.0f, ImGui::GetWindowSize().y));
    if (MouseHoveringBounds(legendResizer)) {
        DrawRect(legendResizer, ImVec4(0.1f, 0.1f, 0.1f, 1));
    }

    static bool resizerDragging = false;
    if (MouseHoveringBounds(legendResizer) &&
        ImGui::GetIO().MouseClicked[ImGuiMouseButton_Left]) {
        resizerDragging = true;
    }

    if (ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] && resizerDragging && !timelineDrag.isActive && !anyLayerDragged && !anyLayerHovered) {
        legendWidth = windowMouseCoords.x / ImGui::GetWindowSize().x;
    } else
        resizerDragging = false;

    ImGui::SetCursorPos(oldCursor); 

    ImGui::End();
    ImGui::PopStyleVar(2);
    if (layerDeleteionTarget != -1) {
        auto &layers = Shared::graphics->layers;
        layers[layerDeleteionTarget].Destroy();
        layers.erase(layers.begin() + layerDeleteionTarget);
    }

    if (layerDuplicationTarget != -1) {
        auto &layers = Shared::graphics->layers;
        RenderLayer layer = layers[layerDuplicationTarget];
        layer.id = seedrand();
        layers.insert(layers.begin() + layerDuplicationTarget, layer);
    }

    if (layerCopyTarget != -1 && copyContainer.initialized) {
        auto &layers = Shared::graphics->layers;
        RenderLayer layer = copyContainer;
        layer.FetchImplementation();
        layer.id = seedrand();
        layers.insert(layers.begin() + layerCopyTarget + 1, layer);
    }
}
}