#include "render_layer.h"

namespace Electron {
RenderLayer::RenderLayer(std::string layerLibrary) {
    this->visible = true;
    this->layerLibrary = layerLibrary;
    this->properties = {};
    this->previousProperties = {};
    // print("Loading dylib " + layerLibrary);

    this->beginFrame = Shared::graphics->renderFrame;
    this->endFrame =
        Shared::graphics->renderFrame + Shared::graphics->renderFramerate;
    this->frameOffset = 0;

    this->FetchImplementation();

    initializationProcedure(this);
    this->initialized = true;
    this->previousProperties = properties;

    this->layerUsername = layerPublicName + " Layer";
    this->id = seedrand();
}

RenderLayer::~RenderLayer() {
}

void RenderLayer::Destroy() {
    destructionProcedure(this);
}

void RenderLayer::FetchImplementation() {

    this->layerProcedure = TryGetLayerImplF("LayerRender");
    this->destructionProcedure = TryGetLayerImplF("LayerDestroy");
    this->onPlaybackChangeProcedure = TryGetLayerImplF("LayerOnPlaybackChange");
    this->onTimelineSeek = TryGetLayerImplF("LayerOnTimelineSeek");
    this->propertiesProcedure = TryGetLayerImplF("LayerPropertiesRender");
    this->initializationProcedure = TryGetLayerImplF("LayerInitialize");
    this->sortingProcedure = TryGetLayerImplF("LayerSortKeyframes");
    this->onPropertiesChange = TryGetLayerImplF("LayerOnPropertiesChange");
    this->layerPublicName =
        Libraries::GetVariable<std::string>(layerLibrary, "LayerName");
    this->layerColor =
        Libraries::GetVariable<glm::vec4>(layerLibrary, "LayerTimelineColor");
    this->previewProperties =
        Libraries::GetVariable<json_t>(layerLibrary, "LayerPreviewProperties");

    if (!layerProcedure)
        throw std::runtime_error("bad layer procedure!");
}

void RenderLayer::Render() {
    if (!visible) return;
    if (std::clamp((int)Shared::graphics->renderFrame, beginFrame, endFrame) ==
        (int)Shared::graphics->renderFrame)
        layerProcedure(this);
}

void RenderLayer::SortKeyframes(json_t &keyframes) {
    for (int step = 0; step < keyframes.size() - 1; ++step) {
        for (int i = 1; i < keyframes.size() - step - 1; ++i) {
            if (keyframes.at(i).at(0) > keyframes.at(i + 1).at(0)) {
                json_t temp = keyframes.at(i);
                keyframes.at(i) = keyframes.at(i + 1);
                keyframes.at(i + 1) = temp;
            }
        }
    }

    for (int i = 1; i < keyframes.size(); i++) {
        keyframes.at(i).at(0) = glm::clamp(
            JSON_AS_TYPE(keyframes.at(i).at(0), int), 0, endFrame - beginFrame);
    }

    for (int i = 1; i < keyframes.size(); i++) {
        json_t stamp = keyframes.at(i);
        for (int j = i + 1; j < keyframes.size(); j++) {
            if (JSON_AS_TYPE(stamp.at(0), int) ==
                JSON_AS_TYPE(keyframes.at(j).at(0), int)) {
                stamp.at(0) = JSON_AS_TYPE(keyframes.at(j).at(0), int) + 1;
            }
        }
        keyframes.at(i) = stamp;
    }
}

void RenderLayer::RenderProperties() { 
    propertiesProcedure(this); 
}

void RenderLayer::RenderProperty(GeneralizedPropertyType type,
                                           json_t &property,
                                           std::string propertyName, ImVec2 floatBoundaries) {
    ImVec2 windowSize = ImGui::GetWindowSize();
    float inputFieldDivider = 11;
    if (ImGui::CollapsingHeader(propertyName.c_str())) {
        if (ImGui::Button((std::string(ICON_FA_PLUS) +
                           std::string(" Add keyframe") + "##" + propertyName)
                              .c_str())) {
            float currentViewTime = Shared::graphics->renderFrame - beginFrame;
            json_t addedKeyframe = {currentViewTime};
            int typeSize = 1;
            switch (type) {
            case GeneralizedPropertyType::Vec2: {
                typeSize = 2;
                break;
            }
            case GeneralizedPropertyType::Color3:
            case GeneralizedPropertyType::Vec3: {
                typeSize = 3;
                break;
            }
            case GeneralizedPropertyType::Float: {
                typeSize = 1;
                break;
            }
            }
            for (int i = 0; i < typeSize; i++) {
                addedKeyframe.push_back(0.0f);
            }
            property.push_back(addedKeyframe);
        }
        ImGui::Text("Keyframes:");
        for (int i = 1; i < property.size(); i++) {
            bool breakLoop = false;
            switch (type) {
            case GeneralizedPropertyType::Color3:
            case GeneralizedPropertyType::Vec2:
            case GeneralizedPropertyType::Vec3: {
                float fKey = JSON_AS_TYPE(property.at(i).at(0), float);
                std::vector<float> vectorProperty = {};
                vectorProperty.push_back(
                    JSON_AS_TYPE(property.at(i).at(1), float));
                vectorProperty.push_back(
                    JSON_AS_TYPE(property.at(i).at(2), float));
                if (type == GeneralizedPropertyType::Vec3 ||
                    type == GeneralizedPropertyType::Color3) {
                    vectorProperty.push_back(
                        JSON_AS_TYPE(property.at(i).at(3), float));
                }
                float *fProperty = vectorProperty.data();
                ImGui::PushItemWidth(30);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{1, 0, 0, 1});

                if (ImGui::Button((std::string(ICON_FA_TRASH_CAN) +
                                   " Delete##" + propertyName +
                                   std::to_string(i) + "KeyframeDelete")
                                      .c_str())) {
                    property.erase(property.begin() + i);
                    breakLoop = true;
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                    break;
                }

                ImGui::PopStyleColor();
                ImGui::PopItemWidth();
                ImGui::SameLine();

                ImGui::PushItemWidth(windowSize.x / inputFieldDivider);
                ImGui::InputFloat(
                    ("##" + propertyName + std::to_string(i) + "0").c_str(),
                    &fKey, 0.0f, 0.0f, "%0.0f",
                    ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (type == GeneralizedPropertyType::Vec2)
                    ImGui::InputFloat2((std::string(ICON_FA_STOPWATCH) +
                                       std::to_string(i - 1) + "##" + propertyName +
                                       std::to_string(i))
                                          .c_str(),
                                       fProperty, "%.3f", 0);
                else if (type == GeneralizedPropertyType::Color3)
                    ImGui::ColorEdit3((std::string(ICON_FA_STOPWATCH) +
                                       std::to_string(i - 1) + "##" + propertyName +
                                       std::to_string(i))
                                          .c_str(),
                                      fProperty, 0);
                else
                    ImGui::InputFloat3((std::string(ICON_FA_STOPWATCH) +
                                       std::to_string(i - 1) + "##" + propertyName +
                                       std::to_string(i))
                                          .c_str(),
                                       fProperty, "%.3f", 0);

                property.at(i).at(0) = fKey;
                property.at(i).at(1) = fProperty[0];
                property.at(i).at(2) = fProperty[1];
                if (GeneralizedPropertyType::Vec3 == type ||
                    GeneralizedPropertyType::Color3 == type) {
                    property.at(i).at(3) = fProperty[2];
                }
                break;
            }
            case GeneralizedPropertyType::Float: {
                float fKey = JSON_AS_TYPE(property.at(i).at(0), float);
                float fValue = JSON_AS_TYPE(property.at(i).at(1), float);

                ImGui::PushItemWidth(30);
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{1, 0, 0, 1});

                if (ImGui::Button((std::string(ICON_FA_TRASH_CAN) +
                                   " Delete##" + propertyName +
                                   std::to_string(i) + "KeyframeDelete")
                                      .c_str())) {
                    property.erase(property.begin() + i);
                    breakLoop = true;
                    ImGui::PopStyleColor();
                    ImGui::PopItemWidth();
                    break;
                }

                ImGui::PopStyleColor();
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::SameLine();

                ImGui::PushItemWidth(windowSize.x / inputFieldDivider);
                    ImGui::InputFloat(
                    ("##" + propertyName + std::to_string(i) + "0").c_str(),
                    &fKey, 0.0f, 0.0f, "%0.1f",
                    ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (floatBoundaries.x + floatBoundaries.y == 0.0f) {
                    ImGui::InputFloat((std::string(ICON_FA_STOPWATCH) +
                                       std::to_string(i - 1) + "##" + propertyName +
                                       std::to_string(i))
                                          .c_str(),
                                      &fValue, 0, 0, "%.2f", 0);
                } else {
                    ImGui::SliderFloat(
                        (std::string(ICON_FA_STOPWATCH) +
                                       std::to_string(i - 1) + "##" + propertyName +
                                       std::to_string(i))
                                          .c_str(),
                        &fValue, floatBoundaries.x, floatBoundaries.y, "%.2f"
                    );
                }

                property.at(i).at(0) = fKey;
                property.at(i).at(1) = fValue;
                break;
            }
            }
            if (breakLoop)
                break;
        }
    }
}

void RenderLayer::RenderAssetProperty(json_t &property,
                                                  std::string label, TextureUnionType type) {
    if (ImGui::CollapsingHeader(label.c_str())) {
        ImGui::Spacing();
        std::string textureID = JSON_AS_TYPE(property, std::string);
        TextureUnion *textureAsset = nullptr;
        auto &assets = Shared::assets->assets;
        for (int i = 0; i < assets.size(); i++) {
            if (assets.at(i).id == hexToInt(textureID)) {
                textureAsset = &assets.at(i);
            }
        }
        std::string assetIcon = ICON_FA_QUESTION;
        if (textureAsset != nullptr)
            assetIcon = textureAsset->GetIcon();
        std::string assetName = "No Asset";
        if (textureAsset != nullptr) {
            assetName = textureAsset->name;
        }
        if (textureAsset != nullptr) {
            RenderBuffer *pbo = &Shared::graphics->renderBuffer;
            glm::vec2 textureDimensions = textureAsset->GetDimensions();
            ImVec2 imageCenterRect = FitRectInRect(ImVec2(150, 150), ImVec2(textureDimensions.x, textureDimensions.y));
            ImGui::SetCursorPosX(ImGui::GetWindowSize().x / 2.0f - imageCenterRect.x / 2.0f);
            ImGui::Image((ImTextureID) ((uint64_t) textureAsset->pboGpuTexture), imageCenterRect);
            ImGui::Text("%s %s", ICON_FA_INFO, textureAsset->ffprobeData.c_str());
        }
        bool selectedAssetCompatible = false;
        if (type == TextureUnionType::Texture && textureAsset != nullptr) {
            selectedAssetCompatible = textureAsset->IsTextureCompatible();
        } else if (type == TextureUnionType::Audio && textureAsset != nullptr) {
            selectedAssetCompatible = (textureAsset->type == TextureUnionType::Audio);
        }
        if (textureAsset == nullptr) {
            ImGui::Text("%s", ("No Asset with ID '" + textureID + "' Found").c_str());
        } else if (!selectedAssetCompatible) {
            ImGui::Text("%s %s", 
                ("Asset with ID '" + textureID + "' is not compatible to")
                    .c_str(), AssetRegistry::StringFromTextureUnionType(type).c_str());
        } else {
            ImGui::Text("Asset: ");
            ImGui::SameLine();
        }
        if (ImGui::Selectable(
                string_format("%s %s", assetIcon.c_str(), assetName.c_str())
                    .c_str())) {
            ImGui::OpenPopup(
                string_format("SelectAssetPopup%s", label.c_str()).c_str());
        }
        if (ImGui::IsItemHovered() &&
            ImGui::GetIO().MouseDown[ImGuiMouseButton_Right] &&
            textureAsset != nullptr) {
            ImGui::OpenPopup("AssetActionsPopup");
        }
        if (ImGui::BeginPopup("AssetActionsPopup")) {
            ImGui::SeparatorText(textureAsset->name.c_str());
            if (ImGui::MenuItem(string_format("%s %s", ICON_FA_FOLDER,
                                              "Reveal in Asset Manager")
                                    .c_str(),
                                "")) {
                Shared::selectedRenderLayer = textureAsset->id;
            }
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopup(
                string_format("SelectAssetPopup%s", label.c_str()).c_str())) {
            static std::string searchFilter = "";
            ImGui::InputText("##PopupSearchFilter", &searchFilter, 0);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", string_format("%s %s",
                                                ICON_FA_MAGNIFYING_GLASS,
                                                "Search filter")
                                      .c_str());
            for (auto &asset : assets) {
                std::string icon = asset.GetIcon();
                if (searchFilter != "" &&
                    asset.name.find(searchFilter) == std::string::npos)
                    continue;
                bool isCompatible = false;
                if (type == TextureUnionType::Texture) {
                    isCompatible = asset.IsTextureCompatible();
                } else if (type == TextureUnionType::Audio) {
                    isCompatible = (asset.type == TextureUnionType::Audio);
                }
                if (!isCompatible) continue;
                if (ImGui::Selectable(
                        string_format("%s %s", icon.c_str(), asset.name.c_str())
                            .c_str())) {
                    textureID = intToHex(asset.id);
                }
                if ((asset.type == TextureUnionType::Texture || asset.type == TextureUnionType::Audio) &&
                    ImGui::IsItemHovered() && ImGui::BeginTooltip()) {
                        glm::vec2 resolution = asset.GetDimensions();
                        ImGui::Image((ImTextureID) ((uint64_t) asset.pboGpuTexture), FitRectInRect(ImVec2(128, 128), ImVec2(resolution.x, resolution.y)));
                        ImGui::EndTooltip();
                }
            }
            ImGui::EndPopup();
        }

        property = textureID;
    }
}

json_t RenderLayer::InterpolateProperty(json_t keyframes) {
    int targetKeyframeIndex = -1;
    int keyframesLength = keyframes.size();
    int renderViewTime = Shared::graphics->renderFrame - beginFrame;
    GeneralizedPropertyType propertyType = keyframes.at(0);

    for (int i = 1; i < keyframesLength; i++) {
        int keyframeTimestamp = JSON_AS_TYPE(keyframes.at(i).at(0), int);
        if (renderViewTime <= keyframeTimestamp) {
            targetKeyframeIndex = i;
            break;
        }
    }

    // Make sure to keep last keyframe's value
    if (targetKeyframeIndex == -1) {
        return ExtractExactValue(keyframes.at(keyframesLength - 1));
    }

    // Set initial transformation position
    if (targetKeyframeIndex == 1) {
        return ExtractExactValue(keyframes.at(1));
    }

    json_t beginKeyframeValue =
        ExtractExactValue(keyframes.at(targetKeyframeIndex - 1));
    json_t endKeyframeValue =
        ExtractExactValue(keyframes.at(targetKeyframeIndex));
    json_t interpolatedValue = {};

    float keyframeFrame = keyframes.at(targetKeyframeIndex).at(0);
    float interpolationPercentage = 0;
    if (targetKeyframeIndex == 1) {
        interpolationPercentage = renderViewTime / keyframeFrame;
    } else {
        float previousFrame = keyframes.at(targetKeyframeIndex - 1).at(0);
        interpolationPercentage =
            (renderViewTime - previousFrame) / (keyframeFrame - previousFrame);
    }

    switch (propertyType) {
    case GeneralizedPropertyType::Float: {
        interpolatedValue.push_back(glm::mix(beginKeyframeValue.at(0),
                                             endKeyframeValue.at(0),
                                             interpolationPercentage));
        break;
    }
    // Vector interpolation
    case GeneralizedPropertyType::Vec2:
    case GeneralizedPropertyType::Vec3:
    case GeneralizedPropertyType::Color3: {
        if (beginKeyframeValue.size() != endKeyframeValue.size()) {
            throw std::runtime_error(
                "malformed interpolation targets (vector size mismatch)");
        }
        for (int i = 0; i < beginKeyframeValue.size(); i++) {
            float beginFloat = beginKeyframeValue.at(i);
            float endFloat = endKeyframeValue.at(i);
            interpolatedValue.push_back(
                glm::mix(beginFloat, endFloat, interpolationPercentage));
        }
        break;
    }
    }

    return interpolatedValue;
}

json_t RenderLayer::ExtractExactValue(json_t property) {
    std::vector<json_t> acc{};
    for (int i = 1; i < property.size(); i++) {
        acc.push_back(property.at(i));
    }
    return acc;
}

Electron_LayerImplF RenderLayer::TryGetLayerImplF(std::string key) {
    try {
        return Libraries::GetFunction<void(RenderLayer*)>(layerLibrary, key);
    } catch (internalDylib::symbol_error err) {
        return LayerImplPlaceholder;
    }
}

void LayerImplPlaceholder(RenderLayer* layer) {}
}