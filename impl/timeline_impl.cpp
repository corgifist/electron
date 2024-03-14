#include "app.h"
#include "utils/drag_utils.h"
#include "editor_core.h"
#define LEGEND_LAYER_DRAG_DROP "LEGEND_LAYER_DRAG_DROP"
#define ASSET_DRAG_PAYLOAD "ASSET_DRAG_PAYLOAD"

using namespace Electron;

extern "C" {

    static float pixelsPerFrame = 3.0f;
    static float beginPRF = 0;
    static float targetPRF = 0;
    static float percentagePRF = -1;

    static float layerSizeY =
            44 * JSON_AS_TYPE(Shared::configMap["UIScaling"], float);

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

    static bool BoundsContains(RectBounds x, RectBounds y) {
        return ((y.pos.x + y.size.y) < (x.pos.x + x.size.x)
            && (y.pos.x) > (x.pos.x)
            && (y.pos.y) > (x.pos.y)
            && (y.pos.y + y.size.y) < (x.pos.y + x.size.y)
            );
    }

    static ImVec4 GetColor(ImGuiCol color) {
        ImVec4 col = ImGui::GetStyle().Colors[color];
        col.w = 1.0f;
        return col;
    }

    static void TimelienRenderDragNDropTarget(int &i) {
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload *payload = ImGui::AcceptDragDropPayload(
                    LEGEND_LAYER_DRAG_DROP,
                    ImGuiDragDropFlags_SourceNoDisableHover | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
                int from = *((int *)payload->Data);
                int to = i;
                std::swap(GraphicsCore::layers[from],
                        GraphicsCore::layers[i]);
            }
            ImGui::EndDragDropTarget();
        }
    }

    static bool TimelineRenderDragNDrop(int &i) {
        bool dragging = false;
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceNoDisableHover)) {
            ImGui::SetDragDropPayload(LEGEND_LAYER_DRAG_DROP, &i, sizeof(i));
            dragging = true;
            RenderLayer& layer = GraphicsCore::layers[i];
            ImVec4 layerColor = ImVec4{layer.layerColor.r, layer.layerColor.g,
                    layer.layerColor.b, 0.7f};
            ImGui::PushStyleColor(ImGuiCol_Button, layerColor);
                ImGui::Button(layer.layerUsername.c_str(), ImVec2((GraphicsCore::renderFramerate + 10) * pixelsPerFrame, layerSizeY / 2));
            ImGui::PopStyleColor();
            ImGui::EndDragDropSource();
        }
        TimelienRenderDragNDropTarget(i);
        return dragging;
    }

    static bool CustomButtonEx(const char* label, ImVec2 size_arg, RenderLayer* layer, TimelineLayerRenderDesc desc, int layerIndex) {
        ImGuiWindow* window = ImGui::GetCurrentWindow();
        ImVec2 oldCursor = ImGui::GetCursorPos();
        ImGuiButtonFlags flags = 0;
        if (window->SkipItems)
            return false;

        ImGuiContext& g = *GImGui;
        const ImGuiStyle& style = g.Style;
        const ImGuiID id = window->GetID(label);
        const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

        ImVec2 pos = window->DC.CursorPos;
        if ((flags & ImGuiButtonFlags_AlignTextBaseLine) && style.FramePadding.y < window->DC.CurrLineTextBaseOffset) // Try to vertically align buttons that are smaller/have no padding so that text baseline matches (bit hacky, since it shouldn't be a flag)
            pos.y += window->DC.CurrLineTextBaseOffset - style.FramePadding.y;
        ImVec2 size = ImGui::CalcItemSize(size_arg, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

        const ImRect bb(pos, pos + size);
        ImGui::ItemSize(size, style.FramePadding.y);
        if (!ImGui::ItemAdd(bb, id))
            return false;

        bool hovered, held;
        bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

        // Render
        const ImU32 col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
        ImGui::RenderNavHighlight(bb, id);
        ImGui::RenderFrame(bb.Min, bb.Max, col, true, style.FrameRounding);

        ImGui::SetCursorPos(oldCursor);
            layer->timelineRenderProcedure(layer, desc);
        ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, style.ButtonTextAlign, &bb);

        TimelineRenderDragNDrop(layerIndex);

        return pressed;
    }

    static bool CustomCollapsingHeader(std::string label, ImVec2 size) {
        static std::unordered_map<std::string, bool> state;
        if (state.find(label) == state.end())
            state[label] = false;
        
        bool& b = state[label];
        if (ImGui::ButtonEx(label.c_str(), size, 0)) {
            b = !b;
        }
        return b;
    }

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

    static void TimelineRenderLayerPopup(int i,
                                        bool &firePopupsFlag,
                                        RenderLayer &copyContainer,
                                        int &layerDuplicationTarget,
                                        int &layerCopyTarget,
                                        int &layerDeleteionTarget,
                                        int &layerCutTarget) {
        RenderLayer *layer = &GraphicsCore::layers[i];
        ImGui::PopStyleVar(2);
        if (ImGui::BeginPopup(string_format("TimelineLayerPopup%i", i).c_str())) {
            firePopupsFlag = true;
            auto fbo = layer->framebufferProcedure(layer);
            ImGui::SeparatorText(layer->layerUsername.c_str());
            layer->menuProcedure(layer);
            if (fbo.id != 0 && ImGui::BeginMenu(string_format("%s %s", ICON_FA_IMAGES, ELECTRON_GET_LOCALIZATION("INSPECT_BUFFERS")).c_str())) {
                // ImGui::Image((ImTextureID) (uint64_t) fbo.rbo.colorBuffer, FitRectInRect(ImVec2(ImGui::GetWindowSize().x, 80), ImVec2(fbo.width, fbo.height)));
                // ImGui::Image((ImTextureID) (uint64_t) fbo.rbo.uvBuffer, FitRectInRect(ImVec2(ImGui::GetWindowSize().x, 80), ImVec2(fbo.width, fbo.height)));
                ImGui::EndMenu();
            }
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
            if (ImGui::MenuItem(string_format("%s %s", ICON_FA_SCISSORS, ELECTRON_GET_LOCALIZATION("GENERIC_CUT")).c_str())) {
                layerCutTarget = i;
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

    static RenderLayer DuplicateRenderLayer(RenderLayer& sourceLayer) {
        RenderLayer duplicateLayer = RenderLayer(sourceLayer.layerLibrary);
        duplicateLayer.properties = sourceLayer.properties;
        duplicateLayer.beginFrame = sourceLayer.beginFrame;
        duplicateLayer.endFrame = sourceLayer.endFrame;
        duplicateLayer.layerUsername = sourceLayer.layerUsername;
        return duplicateLayer;
    }

    ELECTRON_EXPORT void UIRender() {
        ImGui::SetCurrentContext(AppCore::context);
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
            ImGui::PopStyleVar(2);
            ImGui::End();
            throw Signal::_CloseWindow;
        }
        if (!AppCore::projectOpened) {
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
        ImVec2 originalMouseCoords = windowMouseCoords;
        pixelsPerFrame = glm::clamp(pixelsPerFrame, 0.5f, 20.0f);
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
        if (layersPropertiesOffset.size() != GraphicsCore::layers.size()) {
            layersPropertiesOffset =
                std::vector<float>(GraphicsCore::layers.size());
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
        int layerCutTarget = -1;

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
        DrawRect(fillerTicksBackground, style.Colors[ImGuiCol_WindowBg] * ImVec4{0.7f, 0.7f, 0.7f, 1.0f});
        float propertiesStep = layerSizeY;
        float propertiesCoordAcc = 0;
        for (int i = GraphicsCore::layers.size() - 1; i >= 0; i--) {
            RenderLayer *layer = &GraphicsCore::layers[i];
            if (keyframeInfos.find(layer->id) == keyframeInfos.end()) {
                keyframeInfos[layer->id] = std::vector<KeyframeHolderInfo>(
                    layer->GetPreviewProperties().size());
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
            if (ImGui::Button(string_format("%s", layer->visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH).c_str(), ImVec2{0, layerSizeY})) {
                layer->visible = !layer->visible;
            }
            if (visible) ImGui::PopStyleColor();
            ImGui::SameLine();
            std::string spinnerText = "";
            if (JSON_AS_TYPE(layer->internalData["ShowLoadingSpinner"], bool)) {
                spinnerText = ICON_FA_SPINNER " - ";
            }
            std::string statusText = JSON_AS_TYPE(layer->internalData["StatusText"], std::string);
            if (statusText != "") {
                statusText = " - " + statusText;
            }
            if (CustomCollapsingHeader(
                    (spinnerText + layer->layerUsername + statusText + "###" + layer->layerUsername +  std::to_string(i)).c_str(), ImVec2{ImGui::GetContentRegionAvail().x, layerSizeY})) {
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
                if (ImGui::IsItemHovered() &&
                    ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                    anyPopupsOpen = true;
                    ImGui::OpenPopup(
                        string_format("TimelineLayerPopup%i", i).c_str());
                }
                TimelineRenderLayerPopup(i, firePopupsFlag, copyContainer,
                                        layerDuplicationTarget, layerCopyTarget,
                                        layerDeleteionTarget, layerCutTarget);
                active = true;
                TimelineRenderDragNDrop(i);
                ImGui::PopStyleVar(2);
                float firstCursorY = ImGui::GetCursorPosY();

                json_t previewTargets = layer->GetPreviewProperties();
                float layerViewTime =
                    GraphicsCore::renderFrame - layer->beginFrame + JSON_AS_TYPE(layer->properties["InternalTimeShift"], float);
                for (int i = 0; i < previewTargets.size(); i++) {
                    std::string previewAlias = previewTargets.at(i);
                    json_t aliases = layer->properties["PropertyAlias"];
                    if (aliases.find(previewAlias) != aliases.end()) {
                        previewAlias = JSON_AS_TYPE(aliases[previewAlias], std::string);
                    }
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
                    holderInfo.yCoord = ImGui::GetCursorPosY() - 3;
                    bool addKeyframe = (ImGui::Button((ICON_FA_PLUS + std::string("##") + JSON_AS_TYPE(previewTargets.at(i), std::string) + std::to_string(i) + std::to_string(layer->id)).c_str()));
                    ImGui::SameLine();
                    float oldRenderFrame = GraphicsCore::renderFrame;
                    GraphicsCore::renderFrame += JSON_AS_TYPE(layer->properties["InternalTimeShift"], float);
                    switch (propertyType) {
                    case GeneralizedPropertyType::Float: {
                        float x = JSON_AS_TYPE(
                            layer->InterpolateProperty(propertyMap).at(0), float);
                        bool isSlider = false;
                        ImVec2 sliderBounds = {0, 0};
                        if (layer->internalData.find("TimelineSliders") != layer->internalData.end()) {
                            json_t& timelineSliders = layer->internalData["TimelineSliders"];
                            for (int j = 0; j < timelineSliders.size(); j++) {
                                if (JSON_AS_TYPE(timelineSliders.at(j).at(0), std::string) == JSON_AS_TYPE(previewTargets.at(i), std::string)) {
                                    isSlider = true;
                                    sliderBounds = ImVec2(JSON_AS_TYPE(timelineSliders.at(j).at(1), float), JSON_AS_TYPE(timelineSliders.at(j).at(2), float));
                                    break;
                                }
                            }
                        }
                        bool formInput = false;
                        if (!isSlider) {
                            formInput = ImGui::InputFloat(
                                (previewAlias +
                                "##" + std::to_string(i) + std::to_string(layer->id))
                                    .c_str(),
                                &x, 0, 0, "%.2f",
                                ImGuiInputTextFlags_EnterReturnsTrue);
                        } else {
                            formInput = ImGui::SliderFloat(
                                (previewAlias +
                                "##" + std::to_string(i) + std::to_string(layer->id)).c_str(),
                                &x, sliderBounds.x, sliderBounds.y, "%.2f"
                            );
                        }
                        if (formInput) {
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
                                (previewAlias +
                                "##" + std::to_string(i) + std::to_string(layer->id))
                                    .c_str(),
                                raw.data(), "%.2f",
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
                                    (previewAlias +
                                    "##" + std::to_string(i) + std::to_string(layer->id))
                                        .c_str(),
                                    raw.data(), "%.3f",
                                    ImGuiInputTextFlags_EnterReturnsTrue)
                                : ImGui::ColorEdit3((
                                    previewAlias +
                                    "##" + std::to_string(i) + std::to_string(layer->id))
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
                    GraphicsCore::renderFrame = oldRenderFrame;
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
                        ImGui::SeparatorText(previewAlias.c_str());
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
                    if (addKeyframe) {
                        json_t xKeyframe = propertyMap.at(keyframeIndex);
                        xKeyframe.at(0) = (int)layerViewTime;
                        propertyMap.push_back(xKeyframe);
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
                TimelineRenderDragNDrop(i);
                if (ImGui::IsItemHovered() &&
                    ImGui::GetIO().MouseDown[ImGuiMouseButton_Right]) {
                    ImGui::OpenPopup(
                        string_format("TimelineLayerPopup%i", i).c_str());
                }
                TimelineRenderLayerPopup(i, firePopupsFlag, copyContainer,
                                        layerDuplicationTarget, layerCopyTarget,
                                        layerDeleteionTarget, layerCutTarget);
            }

            propertiesCoordAcc += propertiesStep + layersPropertiesOffset[i];
        }

        ImGui::EndChild();

        ImGui::SameLine();

        static bool showHorizontalScrollbar = false;
        static bool showVerticalScrollbar = false;
        ImGuiWindowFlags timelineFlags = 0;
        if (showHorizontalScrollbar)
            timelineFlags |= ImGuiWindowFlags_AlwaysHorizontalScrollbar;
        if (showVerticalScrollbar)
            timelineFlags |= ImGuiWindowFlags_AlwaysVerticalScrollbar;
        ImGui::BeginChild("projectTimeline",
                        ImVec2(canvasSize.x - legendSize.x, canvasSize.y), false,
                        timelineFlags);
        windowMouseCoords = ImGui::GetIO().MousePos - ImGui::GetCursorScreenPos();
        ImVec2 timelineReservedMouseCoords = windowMouseCoords;
        bool previousVerticalBar = showVerticalScrollbar;
        bool previousHorizontalBar = showHorizontalScrollbar;
        showVerticalScrollbar = (windowMouseCoords.x - ImGui::GetScrollX() >= (ImGui::GetWindowSize().x - 20)) && windowMouseCoords.x - ImGui::GetScrollY() < ImGui::GetWindowSize().x + 5;
        showHorizontalScrollbar = (windowMouseCoords.y - ImGui::GetScrollY() >= (ImGui::GetWindowSize().y - 20)) && windowMouseCoords.y - ImGui::GetScrollY() < ImGui::GetWindowSize().y + 5;
        if (previousVerticalBar && ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])
            showVerticalScrollbar = true;
        if (previousHorizontalBar && ImGui::GetIO().MouseDown[ImGuiMouseButton_Left])
            showHorizontalScrollbar = true;
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
        DrawRect(ticksBackground, style.Colors[ImGuiCol_WindowBg] * ImVec4{0.5f, 0.5f, 0.5f, 1.0f});
        int desiredTicksCount = 5;
        float tickStep =
            (float)GraphicsCore::renderFramerate / (float)desiredTicksCount;
        float tickPositionStep = tickStep * pixelsPerFrame;
        float tickPositionAccumulator = 0;
        float tickAccumulator = 0;
        float previousMajorTick = 0;
        while (tickAccumulator <= GraphicsCore::renderLength) {
            bool majorTick = remainder((int) tickAccumulator,
                                    (int) GraphicsCore::renderFramerate) == 0.0f;
            float tickHeight = majorTick ? 0 : 2.0f;
            RectBounds tickBounds = RectBounds(
                ImVec2(tickPositionAccumulator, 0 + ImGui::GetScrollY()),
                ImVec2(TIMELINE_TICK_WIDTH, majorTick ? canvasSize.y : 6.0f));
            DrawRect(tickBounds, style.Colors[ImGuiCol_Border] * ImVec4{0.7f, 0.7f, 0.7f, 1.0f});

            if (majorTick) {
                TimeStampTarget stamp{};
                stamp.frame = tickAccumulator;
                stamp.position = ImVec2(tickPositionAccumulator, 0);
                stamps.push_back(stamp);
                previousMajorTick = tickAccumulator;
            }

            tickPositionAccumulator += tickPositionStep;
            tickAccumulator += tickStep;
        }

        for (auto &stamp : stamps) {
            ImGui::GetWindowDrawList()->AddText(
                canvasPos + stamp.position +
                    ImVec2(legendSize.x - ImGui::GetScrollX(), 0),
                ImGui::ColorConvertFloat4ToU32(ImVec4(1, 1, 1, 1)),
                formatToTimestamp(stamp.frame, GraphicsCore::renderFramerate)
                    .c_str());
        }
        PopClipRect();

        float layerOffsetY = ticksBackgroundHeight;
        if (universalLayerDrags.size() != GraphicsCore::layers.size()) {
            universalLayerDrags =
                std::vector<DragStructure>(GraphicsCore::layers.size());
        }
        if (forwardLayerDrags.size() != GraphicsCore::layers.size()) {
            forwardLayerDrags =
                std::vector<DragStructure>(GraphicsCore::layers.size());
        }
        if (backwardLayerDrags.size() != GraphicsCore::layers.size()) {
            backwardLayerDrags =
                std::vector<DragStructure>(GraphicsCore::layers.size());
        }
        RectBounds innerTicksZone =
            RectBounds(ImVec2(0 + ImGui::GetScrollX(),
                            ticksBackgroundHeight + ImGui::GetScrollY() + 2),
                    canvasSize);
        ImGui::SetCursorPos({0, 0});
        int dummyAccY = 0;
        for (int i = 0; i < GraphicsCore::layers.size(); i++) {
            RenderLayer& layer = GraphicsCore::layers[i];
            ImGui::SetCursorPos({0, (float) dummyAccY});
            ImGui::Dummy({ImGui::GetWindowSize().x, layerSizeY + layersPropertiesOffset[i]});
            dummyAccY += layerSizeY + layersPropertiesOffset[i];
        }
        ImGui::SetCursorPos({0, 0});
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
            RenderLayer* keyframesOwner = nullptr;
            try {
                keyframesOwner =
                    GraphicsCore::GetLayerByID(layerPair.first);
            } catch (std::runtime_error err) {
                continue;
            }
            RectBounds keyframesBoundsClip = RectBounds(
                ImVec2(keyframesOwner->beginFrame * pixelsPerFrame, 0),
                ImVec2((keyframesOwner->endFrame - keyframesOwner->beginFrame) *
                        pixelsPerFrame,
                    ImGui::GetWindowSize().y));
            PushClipRect(keyframesBoundsClip);
            std::vector<KeyframeHolderInfo>& info = layerPair.second;
            if (keyframesOwner->GetPreviewProperties().size() != info.size()) {
                info = std::vector<KeyframeHolderInfo>(keyframesOwner->GetPreviewProperties().size());
            }
            for (int i = 0; i < keyframesOwner->GetPreviewProperties().size(); i++) {
                KeyframeHolderInfo keyInfo = info.at(i);
                json_t propertyMap =
                    keyframesOwner
                        ->properties[keyframesOwner->GetPreviewProperties().at(i)];
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
                                time * pixelsPerFrame - JSON_AS_TYPE(keyframesOwner->properties["InternalTimeShift"], float) * pixelsPerFrame) -
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
                                    keyframesOwner->GetPreviewProperties().at(i),
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
                        !anyKeyframesDragged && IsInBounds(time, keyframesOwner->beginFrame - 1, keyframesOwner->endFrame + 1)) {
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
                    ->properties[keyframesOwner->GetPreviewProperties().at(i)] =
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
        for (int i = GraphicsCore::layers.size() - 1; i >= 0; i--) {
            ImGui::SetCursorPos({0 + ImGui::GetScrollX(), dragAccepterOffset});
            ImGui::Dummy({ImGui::GetWindowSize().x, layerSizeY});
            if (ImGui::GetIO().KeyCtrl) {
                TimelienRenderDragNDropTarget(i);
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
        for (int i = GraphicsCore::layers.size() - 1; i >= 0; i--) {
            if (layerOffsetY - ImGui::GetScrollY() < 0 || layerOffsetY - ImGui::GetScrollY() > ImGui::GetWindowSize().y) {
                layerOffsetY += layerSizeY + layersPropertiesOffset[i];
                continue;
            }
            RenderLayer *layer = &GraphicsCore::layers[i];
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
            TimelineLayerRenderDesc desc;
            desc.position = {pixelsPerFrame * layer->beginFrame, layerOffsetY};
            desc.size = ImVec2(pixelsPerFrame * layerDuration, layerSizeY);
            desc.layerSizeY = layerSizeY;
            desc.pixelsPerFrame = pixelsPerFrame;
            desc.legendOffset = {legendSize.x, ticksBackgroundHeight};
            if (CustomButtonEx(
                    (layer->layerUsername + "##" + std::to_string(i)).c_str(),
                    ImVec2(pixelsPerFrame * layerDuration, layerSizeY), layer, desc, i)) {
                if (ImGui::GetIO().KeyCtrl && !selected && !universalDrag.isActive && !backwardDrag.isActive && !forwardDrag.isActive) {
                    multipleDragSelectedLayers.push_back(i);
                } else if (ImGui::GetIO().KeyCtrl && selected) {
                    multipleDragSelectedLayers.erase(std::find(multipleDragSelectedLayers.begin(), multipleDragSelectedLayers.end(), i));
                }
                if (selected && !ImGui::GetIO().KeyCtrl) multipleDragSelectedLayers.clear();

                Shared::selectedRenderLayer = layer->id;
            }
            ImGui::PopStyleColor();

            if (ImGui::IsItemHovered()) {
                anyLayerHovered = true;
            }
            if (ImGui::IsItemHovered() &&
                ImGui::GetIO().MouseClicked[ImGuiMouseButton_Right]) {
                ImGui::OpenPopup(string_format("TimelineLayerPopup%i", i).c_str());
                anyPopupsOpen = true;
            }
            TimelineRenderLayerPopup(i, firePopupsFlag, copyContainer,
                                    layerDuplicationTarget, layerCopyTarget,
                                    layerDeleteionTarget, layerCutTarget);
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
                if (windowMouseCoords.x - ImGui::GetScrollX() > ImGui::GetWindowSize().x - ImGui::GetWindowSize().x / 10) {
                    ImGui::SetScrollX(ImGui::GetScrollX() + 1);
                }
                if (windowMouseCoords.x - ImGui::GetScrollX() < ImGui::GetWindowSize().x / 10) {
                    ImGui::SetScrollX(ImGui::GetScrollX() - 1);
                }
            } else
                forwardDrag.Deactivate();

            if (backwardDrag.isActive) backwardDragDistance = ImGui::GetIO().MouseDelta.x;

            if (ImGui::GetIO().MouseDown[ImGuiMouseButton_Left] && backwardDrag.isActive &&
                !timelineDrag.isActive && !ImGui::GetIO().KeyCtrl) {
                layer->beginFrame += backwardDragDistance / pixelsPerFrame;
                if (JSON_AS_TYPE(layer->properties["InternalTimeShift"], float) != 0.0) {
                    layer->properties["InternalTimeShift"] = JSON_AS_TYPE(layer->properties["InternalTimeShift"], float) + backwardDragDistance / pixelsPerFrame;
                }
                if (windowMouseCoords.x - ImGui::GetScrollX() > ImGui::GetWindowSize().x - ImGui::GetWindowSize().x / 10) {
                    ImGui::SetScrollX(ImGui::GetScrollX() + 1);
                }
                if (windowMouseCoords.x - ImGui::GetScrollX() < ImGui::GetWindowSize().x / 10) {
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

                layer->beginFrame = glm::max(layer->beginFrame, 0.0f);
                layer->endFrame = glm::max(layer->endFrame, 0.0f);
                RenderLayer* nextLayer = nullptr;
                if (i == GraphicsCore::layers.size() && GraphicsCore::layers.size() != 1) {
                    nextLayer = &GraphicsCore::layers[GraphicsCore::layers.size() - 2];
                } else {
                    nextLayer = &GraphicsCore::layers[i + 2];
                }
                if (nextLayer != nullptr) {
                    if ((int) layer->beginFrame - 1 == (int) nextLayer->beginFrame) {
                        float durationDifference = layer->endFrame - layer->beginFrame;
                        layer->beginFrame = nextLayer->beginFrame;
                        layer->endFrame = layer->beginFrame + durationDifference;
                        RectBounds stickRenderBounds = RectBounds(
                            ImVec2(layer->beginFrame * pixelsPerFrame, 0 + ImGui::GetScrollY()),
                            ImVec2(TTIMELINE_RULLER_WIDTH, ImGui::GetWindowSize().y)
                        );
                        DrawRect(stickRenderBounds, layerColor);
                    }
                    if ((int) layer->endFrame + 1 == (int) nextLayer->endFrame) {
                        float durationDifference = layer->endFrame - layer->beginFrame;
                        layer->endFrame = nextLayer->endFrame;
                        layer->beginFrame = layer->endFrame - durationDifference;
                        RectBounds stickRenderBounds = RectBounds(
                            ImVec2(layer->endFrame
                            * pixelsPerFrame, 0 + ImGui::GetScrollY()),
                            ImVec2(TTIMELINE_RULLER_WIDTH, ImGui::GetWindowSize().y)
                        );
                        DrawRect(stickRenderBounds, layerColor);
                    }
                }
                timelineDrag.Deactivate();
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (windowMouseCoords.x - ImGui::GetScrollX() > ImGui::GetWindowSize().x - ImGui::GetWindowSize().x / 10) {
                    ImGui::SetScrollX(ImGui::GetScrollX() + 1);
                }
                if (windowMouseCoords.x - ImGui::GetScrollX() < ImGui::GetWindowSize().x / 10) {
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
                        RenderLayer* l = &GraphicsCore::layers[selectedLayer];
                        l->beginFrame += frameAddition;
                        l->endFrame += frameAddition;
                    }
                    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                }

            layerSeparatorTargets.push_back(layerOffsetY);
            layerPreviewHeights.push_back(RectBounds(
                ImVec2(0, layerOffsetY + layerSizeY),
                ImVec2(canvasSize.x, layersPropertiesOffset[i])));
            layerOffsetY += layerSizeY + layersPropertiesOffset[i];
        }

        ImGui::SetCursorPos({0, 0});
        RectBounds timelineBounds =
            RectBounds(ImVec2(pixelsPerFrame * GraphicsCore::renderFrame,
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
            GraphicsCore::renderFrame =
                (int)((windowMouseCoords.x) / pixelsPerFrame);
            GraphicsCore::FireTimelineSeek();
        } else
            timelineDrag.Deactivate();

        RectBounds endTimelineBounds =
            RectBounds(ImVec2(pixelsPerFrame * GraphicsCore::renderLength,
                            0 + ImGui::GetScrollY()),
                    ImVec2(TTIMELINE_RULLER_WIDTH, canvasSize.y));
        DrawRect(endTimelineBounds, ImVec4(1, 1, 0, 1));
        PopClipRect();

        if (GraphicsCore::isPlaying) {
            ImGui::SetScrollX(pixelsPerFrame * GraphicsCore::renderFrame -
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
                    string_format("%s %s", ICON_FA_LAYER_GROUP, ELECTRON_GET_LOCALIZATION("TIMELINE_ADD_LAYER")).c_str())) {
                for (auto &entry : GraphicsCore::GetImplementationsRegistry()) {
                    Libraries::LoadLibrary("layers", entry);
                    
                    if (ImGui::MenuItem(
                            string_format("%s %s (%s)", ICON_FA_PLUS, Libraries::GetVariable<std::string>(entry, "LayerName").c_str(), entry.c_str()).c_str())) {
                        GraphicsCore::AddRenderLayer(entry);
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::MenuItem(string_format("%s %s", ICON_FA_RULER, ELECTRON_GET_LOCALIZATION("JUMP_HERE")).c_str())) {
                GraphicsCore::renderFrame = timelineReservedMouseCoords.x / pixelsPerFrame;
            }
            ImGui::Separator();
            ImGui::Text("%s %i %s", ELECTRON_GET_LOCALIZATION("TOTAL"), (int) GraphicsCore::layers.size(), ELECTRON_GET_LOCALIZATION("LAYERS"));
            ImGui::EndPopup();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        if (ImGui::Shortcut(ImGuiKey_Space | ImGuiMod_Ctrl)) {
            GraphicsCore::isPlaying = !GraphicsCore::isPlaying;
            // Trigger LayerOnPlaybackChange
            GraphicsCore::FirePlaybackChange();
        }

        if (ImGui::Shortcut(ImGuiKey_C | ImGuiMod_Ctrl)) {
            copyContainer =
                *GraphicsCore::GetLayerByID(Shared::selectedRenderLayer);
            print("Copied layer with ID " << copyContainer.id);
        }

        if (ImGui::Shortcut(ImGuiKey_V | ImGuiMod_Ctrl) &&
            copyContainer.initialized) {
            layerCopyTarget =
                GraphicsCore::GetLayerIndexByID(Shared::selectedRenderLayer);
        }

        if (ImGui::Shortcut(ImGuiKey_D | ImGuiMod_Ctrl)) {
            layerDuplicationTarget =
                GraphicsCore::GetLayerIndexByID(Shared::selectedRenderLayer);
        }

        if (ImGui::Shortcut(ImGuiKey_Delete) && Shared::selectedRenderLayer > 0 &&
            multipleDragSelectedLayers.empty()) {
            layerDeleteionTarget =
                GraphicsCore::GetLayerIndexByID(Shared::selectedRenderLayer);
            Shared::selectedRenderLayer = -1;
        }

        if (ImGui::Shortcut(ImGuiKey_Delete) && Shared::selectedRenderLayer > 0 && !multipleDragSelectedLayers.empty()) {
            std::vector<int> layerIndices{};
            for (auto& index : multipleDragSelectedLayers) {
                layerIndices.push_back(GraphicsCore::layers[index].id);
            }
            for (auto& layerIndex : layerIndices) {
                auto& layers = GraphicsCore::layers;
                layers[GraphicsCore::GetLayerIndexByID(layerIndex)].Destroy();
                layers.erase(layers.begin() + GraphicsCore::GetLayerIndexByID(layerIndex));
            }
            multipleDragSelectedLayers.clear();
            Shared::selectedRenderLayer = -1;
        }  
        ImVec2 scroll = {ImGui::GetScrollX(), ImGui::GetScrollY()};
        ImGui::EndChild();
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                ASSET_DRAG_PAYLOAD, ImGuiDragDropFlags_AcceptNoPreviewTooltip)) {
                    int assetIndex = *((int*) payload->Data);
                    TextureUnion& asset = AssetCore::assets[assetIndex];
                    switch (asset.type) {
                        case TextureUnionType::Texture: {
                            RenderLayer imageLayer(Shared::defaultImageLayer);
                            imageLayer.properties["TextureID"] = intToHex(asset.id);
                            imageLayer.layerUsername = asset.name;
                            GraphicsCore::AddRenderLayer(imageLayer);
                            break;
                        }
                        case TextureUnionType::Audio: {
                            RenderLayer audioLayer(Shared::defaultAudioLayer);
                            audioLayer.properties["AudioID"] = intToHex(asset.id);
                            audioLayer.layerUsername = asset.name;
                            GraphicsCore::AddRenderLayer(audioLayer);
                            break;
                        }
                    }
                }
                ImGui::SetCursorPos(originalMouseCoords);
                std::string library = Shared::assetManagerDragDropType;
                if (library != "" && ImGui::IsDragDropPayloadBeingAccepted()) {
                    Libraries::LoadLibrary("layers", library);
                    auto colorVector = Libraries::GetVariable<glm::vec4>(library, "LayerTimelineColor");
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(colorVector.r, colorVector.g, colorVector.b, colorVector.a));
                    if (!anyLayerHovered && ImGui::IsDragDropPayloadBeingAccepted() && std::string(ImGui::GetDragDropPayload()->DataType) == ASSET_DRAG_PAYLOAD) 
                        ImGui::Button(ELECTRON_GET_LOCALIZATION(library == Shared::defaultImageLayer ? "IMAGE_LAYER": "AUDIO_LAYER"), ImVec2(GraphicsCore::renderFramerate * 1.2f * pixelsPerFrame, layerSizeY));
                    ImGui::PopStyleColor();
                }
            ImGui::EndDragDropTarget();
        }

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
            auto &layers = GraphicsCore::layers;
            layers[layerDeleteionTarget].Destroy();
            layers.erase(layers.begin() + layerDeleteionTarget);
        }

        if (layerDuplicationTarget != -1) {
            auto &layers = GraphicsCore::layers;
            RenderLayer& sourceLayer = layers[layerDuplicationTarget];

            layers.insert(layers.begin() + layerDuplicationTarget, DuplicateRenderLayer(sourceLayer));
        }

        if (layerCopyTarget != -1 && copyContainer.initialized) {
            auto &layers = GraphicsCore::layers;
            RenderLayer layer = copyContainer;
            layer.FetchImplementation();
            layer.id = seedrand();
            layers.insert(layers.begin() + layerCopyTarget + 1, layer);
        }

        if (layerCutTarget != -1) {
            RenderLayer& sourceLayer = GraphicsCore::layers[layerCutTarget];
            if (!IsInBounds(GraphicsCore::renderFrame, sourceLayer.beginFrame, sourceLayer.endFrame))
                goto bypass_cut;
            int oldEndFrame = sourceLayer.endFrame;
            int oldBeginFrame = sourceLayer.beginFrame;
            sourceLayer.endFrame = GraphicsCore::renderFrame - 1;

            RenderLayer cutLayer = DuplicateRenderLayer(sourceLayer);
            cutLayer.beginFrame = sourceLayer.endFrame;
            cutLayer.endFrame = oldEndFrame;
            
            int previousTimeShift = 0;
            if (sourceLayer.properties.find("InternalTimeShift") != sourceLayer.properties.end()) {
                previousTimeShift = JSON_AS_TYPE(sourceLayer.properties["InternalTimeShift"], float);
            }
            cutLayer.properties["InternalTimeShift"] = sourceLayer.endFrame - oldBeginFrame + previousTimeShift;

            GraphicsCore::layers.insert(GraphicsCore::layers.begin() + layerCutTarget + 1, cutLayer);
        }
        bypass_cut:
        int x;
    }
}