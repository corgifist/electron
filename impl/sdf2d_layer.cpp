#include "editor_core.h"
#include "app.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define TEXTURE_POOL_SIZE 128

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "SDF2D";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {0.58, 0.576, 1, 1};
    static PipelineShader sdf2d_compute;

    enum class SDFShape {
        None = 0,
        Circle = 1,
        RoundedRect = 2,
        Triangle = 3
    };

    struct SDF2DUserData {
        PipelineFrameBuffer frb;

        SDF2DUserData() {}
    };

    ELECTRON_EXPORT void LayerInitialize(RenderLayer* owner) {
        owner->properties["Position"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0, 0}
        };
        owner->properties["Size"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0.5f, 0.5f},
        };
        owner->properties["Color"] = {
            GeneralizedPropertyType::Color3,
            {0, 1, 1, 1}
        };

        owner->properties["Angle"] = {
            GeneralizedPropertyType::Float,
            {0, 0}
        };
        owner->properties["UvOffset"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0, 0}
        };

        owner->properties["TextureID"] = "";
        owner->properties["EnableTexturing"] = true;
        owner->properties["PropertyAlias"] = {
            {"Position", ICON_FA_UP_DOWN_LEFT_RIGHT " Position"},
            {"Size", ICON_FA_SCALE_BALANCED " Size"},
            {"Color", ICON_FA_DROPLET " Color"},
            {"Angle", ICON_FA_ROTATE " Angle"},
            {"Texturing", ICON_FA_IMAGE " Texturing"},
            {"Radius", ICON_FA_CIRCLE " Radius"},
            {"CircleRadius", ICON_FA_CIRCLE " Radius"},
            {"BoxRadius", ICON_FA_SQUARE " Radius"},
            {"TriangleRadius", ICON_FA_SHAPES " Radius"},
            {"UvOffset", ICON_FA_ARROWS_LEFT_RIGHT " UV Offset"}
        };
        owner->internalData["TimelineSliders"] = {
            {"CircleRadius", 0, 2},
            {"BoxRadius", 0, 2},
            {"TriangleRadius", 0, 2}
        };

        owner->properties["SelectedSDFShape"] = 0;

        owner->userData = new SDF2DUserData();

        if (sdf2d_compute.fragment == 0) {
            sdf2d_compute = GraphicsCore::CompilePipelineShader("sdf2d.pipeline", ShaderType::Fragment);
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        PipelineFrameBuffer* pbo = &GraphicsCore::renderBuffer;

        SDF2DUserData* userData = (SDF2DUserData*) owner->userData;
        PipelineFrameBuffer frb = userData->frb;
        if (frb.width != pbo->width || frb.height != pbo->height) {
            frb.Destroy();
            frb = PipelineFrameBuffer(pbo->width, pbo->height);
        }
        
        userData->frb = frb;

        GraphicsCore::RequestTextureCollectionCleaning(frb, 0.0f);

        auto position = vec2();
        auto size = vec2(0.5f);
        auto color = vec3(1.0f);
        auto uvOffset = vec2();
        auto angle = 0.0f; {
            auto positionVector = owner->InterpolateProperty(owner->properties["Position"]);
            auto sizeVector = owner->InterpolateProperty(owner->properties["Size"]);
            auto colorVector = owner->InterpolateProperty(owner->properties["Color"]);
            auto angleFloat = owner->InterpolateProperty(owner->properties["Angle"]);
            auto uvOffsetVector = owner->InterpolateProperty(owner->properties["UvOffset"]);
            position = vec2(positionVector[0], positionVector[1]);
            size = vec2(sizeVector[0], sizeVector[1]); 
            color = vec3(colorVector[0], colorVector[1], colorVector[2]);
            angle = angleFloat[0];
            uvOffset = vec2(uvOffsetVector[0], uvOffsetVector[1]);
        }

        bool texturingEnabled = JSON_AS_TYPE(owner->properties["EnableTexturing"], bool);
        std::string textureID = JSON_AS_TYPE(owner->properties["TextureID"], std::string);
        TextureUnion* asset = AssetCore::GetAsset(textureID);
        if (asset != nullptr) {
            owner->internalData["StatusText"] = asset->name;
        } else {
            owner->internalData["StatusText"] = "";
        }

        bool canTexture = (asset != nullptr && texturingEnabled);

        frb.Bind();
        GraphicsCore::UseShader(GL_FRAGMENT_SHADER_BIT, sdf2d_compute.fragment);
        mat4 transform = glm::identity<mat4>();
        transform = glm::scale(transform, vec3(size, 1.0f));
        transform = glm::rotate(transform, glm::radians(angle), vec3(0, 0, 1));
        transform = glm::translate(transform, vec3(position, 0.0f));
        float aspect = (float) frb.width / (float) frb.height;
        mat4 projection = ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        GraphicsCore::ShaderSetUniform(GraphicsCore::basic.vertex, "uMatrix", projection * transform);
        GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uColor", color);
        GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfShape", JSON_AS_TYPE(owner->properties["SelectedSDFShape"], int));
        GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSize", size);
        GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uUvOffset", uvOffset);
        switch ((SDFShape) JSON_AS_TYPE(owner->properties["SelectedSDFShape"], int)) {
            case SDFShape::None: {
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnabled", false);
                break;
            };
            case SDFShape::Circle: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["CircleRadius"]).at(0), float);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnabled", true);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfCircleRadius", radius);
                break;
            }
            case SDFShape::RoundedRect: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["BoxRadius"]).at(0), float);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnabled", true);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfCircleRadius", radius);
                break;
            }
            case SDFShape::Triangle: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["TriangleRadius"]).at(0), float);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnabled", true);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfCircleRadius", radius);
                break;
            }
        }
        
        if (canTexture) {
            GraphicsCore::BindGPUTexture(asset->pboGpuTexture, sdf2d_compute.fragment, 0, "uTexture");
        }
        GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uCanTexture", canTexture);
        GraphicsCore::DrawArrays(Shared::fsVAO, fsQuadVertices.size() * 0.5);

        GraphicsCore::CallCompositor(frb);
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        PipelineFrameBuffer* pbo = &GraphicsCore::renderBuffer;

        bool texturingEnabled = JSON_AS_TYPE(layer->properties["EnableTexturing"], bool);
        ImGui::Checkbox("Enable texturing", &texturingEnabled);
        layer->properties["EnableTexturing"] = texturingEnabled;
        ImGui::Separator();
        json_t& position = layer->properties["Position"];
        layer->RenderProperty(GeneralizedPropertyType::Vec2, position, "Position");

        json_t& size = layer->properties["Size"];
        layer->RenderProperty(GeneralizedPropertyType::Vec2, size, "Size");

        json_t& color = layer->properties["Color"];
        layer->RenderProperty(GeneralizedPropertyType::Color3, color, "Color");

        json_t& angle = layer->properties["Angle"];
        layer->RenderProperty(GeneralizedPropertyType::Float, angle, "Angle");

        json_t& uvOffset = layer->properties["UvOffset"];
        layer->RenderProperty(GeneralizedPropertyType::Vec2, uvOffset, "UvOffset");

        static std::vector<std::string> shapesCollection = {
            ICON_FA_XMARK " None",
            ICON_FA_CIRCLE " Circle",
            ICON_FA_SQUARE " Rounded Rectangle",
            ICON_FA_SHAPES " Triangle"
        };

        if (ImGui::CollapsingHeader(ICON_FA_SHAPES " Shape")) {
            ImGui::Text("SDF Shape: ");
            ImGui::SameLine();
            if (ImGui::Selectable(shapesCollection[JSON_AS_TYPE(layer->properties["SelectedSDFShape"], int)].c_str())) {
                ImGui::OpenPopup("sdf2d_shape_selector");
            }
            switch ((SDFShape) JSON_AS_TYPE(layer->properties["SelectedSDFShape"], int)) {
                case SDFShape::None: {
                    break;
                }
                case SDFShape::Circle: {
                    json_t& circleRadius = layer->properties["CircleRadius"];
                    layer->RenderProperty(GeneralizedPropertyType::Float, circleRadius, "CircleRadius");
                    break;
                }
                case SDFShape::RoundedRect: {
                    json_t& boxRadius = layer->properties["BoxRadius"];
                    layer->RenderProperty(GeneralizedPropertyType::Float, boxRadius, "BoxRadius");
                    break;
                }
                case SDFShape::Triangle: {
                    json_t& triangleRadius = layer->properties["TriangleRadius"];
                    layer->RenderProperty(GeneralizedPropertyType::Float, triangleRadius, "TriangleRadius");
                    break;
                }
            }
            ImGui::Separator();
        }

        if (ImGui::BeginPopup("sdf2d_shape_selector")) {
            ImGui::SeparatorText(ICON_FA_SHAPES " Shapes");
            for (int i = 0; i < shapesCollection.size(); i++) {
                if (ImGui::Selectable(shapesCollection[i].c_str())) {
                    layer->properties["SelectedSDFShape"] = i;

                    // populate sdf properties
                    switch ((SDFShape) i) {
                        case SDFShape::None: {
                            break;
                        }
                        case SDFShape::Circle: {
                            layer->properties["CircleRadius"] = {
                                GeneralizedPropertyType::Float,
                                {0, 0.5f}
                            };
                            break;
                        }
                        case SDFShape::RoundedRect: {
                            layer->properties["BoxRadius"] = {
                                GeneralizedPropertyType::Float,
                                {0, 0.2f}
                            };
                            break;
                        }
                        case SDFShape::Triangle: {
                            layer->properties["TriangleRadius"] = {
                                GeneralizedPropertyType::Float,
                                {0, 0.4f}
                            };
                            break;
                        }
                    }
                }
            }
            ImGui::EndPopup();
        }

        json_t& textureID = layer->properties["TextureID"];
        layer->RenderAssetProperty(textureID, "Texturing", TextureUnionType::Texture);
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        json_t& position = layer->properties["Position"];
        layer->SortKeyframes(position);

        json_t& size = layer->properties["Size"];
        layer->SortKeyframes(size);

        json_t& color = layer->properties["Color"];
        layer->SortKeyframes(color);

        json_t& angle = layer->properties["Angle"];
        layer->SortKeyframes(angle);
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
        PipelineFrameBuffer frb = userData->frb;
        frb.Destroy();
        delete userData;
    }

    ELECTRON_EXPORT json_t LayerGetPreviewProperties(RenderLayer* layer) {
        json_t base = {
            "Position", "Size", "Color", "Angle", "UvOffset"
        };
        switch ((SDFShape) JSON_AS_TYPE(layer->properties["SelectedSDFShape"], int)) {
            case SDFShape::Circle: {
                base.push_back("CircleRadius");
                break;
            }
            case SDFShape::RoundedRect: {
                base.push_back("BoxRadius");
                break;
            }
            case SDFShape::Triangle: {
                base.push_back("TriangleRadius");
                break;
            }
        }
        return base;
    }

    ELECTRON_EXPORT void LayerTimelineRender(RenderLayer* layer, TimelineLayerRenderDesc desc) {
        TextureUnion* asset = AssetCore::GetAsset(JSON_AS_TYPE(layer->properties["TextureID"], std::string));
        if (!asset) return;
        ImVec2 previewSize = {desc.layerSizeY, desc.layerSizeY};
        glm::vec2 assetDimensions = asset->GetDimensions();
        ImVec2 rectSize = FitRectInRect(previewSize, ImVec2{assetDimensions.x, assetDimensions.y});
        ImVec2 upperLeft = ImGui::GetCursorScreenPos();
        ImVec2 bottomRight = ImGui::GetCursorScreenPos();
        bottomRight +=  ImVec2{rectSize.x, rectSize.y};
        ImVec2 dragSize = ImVec2((layer->endFrame - layer->beginFrame) * desc.pixelsPerFrame / 10, desc.layerSizeY);
        dragSize.x = glm::clamp(dragSize.x, 1.0f, 30.0f);
        upperLeft.x += dragSize.x;
        bottomRight.x += dragSize.x;
        float dragBounds = desc.legendOffset.x + ((layer->endFrame * desc.pixelsPerFrame) - dragSize.x) - ImGui::GetScrollX();
        if (upperLeft.x < desc.legendOffset.x) {
            upperLeft.x = desc.legendOffset.x;
            bottomRight.x = upperLeft.x + rectSize.x;
        }
        upperLeft.x = glm::min(upperLeft.x, dragBounds);
        bottomRight.x = glm::min(bottomRight.x, dragBounds);
        ImTextureID texID = (ImTextureID) (uint64_t) asset->pboGpuTexture;
        ImGui::GetWindowDrawList()->AddImage(texID, upperLeft, bottomRight);
    }

    ELECTRON_EXPORT PipelineFrameBuffer LayerGetFramebuffer(RenderLayer* layer) {
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
        PipelineFrameBuffer frb = userData->frb;
        return frb;
    }
}