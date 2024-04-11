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
    static GPUExtendedHandle sdf2d_pipeline = 0;
    static GPUExtendedHandle sdf2d_layout = 0;

    enum class SDFShape {
        None = 0,
        Circle = 1,
        RoundedRect = 2,
        Triangle = 3
    };

    struct SDF2DSyncConstants {
        glm::vec2 position;
        glm::vec2 size;
        glm::vec4 color;
        glm::vec2 uvOffset;
        float angle;
        bool texturingEnabled;
        std::string textureID;
        SDFShape sdfShape;
        float sdfRadius;
    };

    struct SDF2DUserData {
        PipelineFrameBuffer frb;
        ManagedAssetDecoder decoder;
        GPUExtendedHandle ubo;
        SDF2DSyncConstants sync;

        SDF2DUserData() {
        }
    };

    struct SDF2DPushConstant {
        glm::mat4 uMatrix;
        alignas(4) float canTexture;
    };

    struct SDF2DUniformBuffer {
        glm::vec4 uColor;
        glm::vec2 uTextureOffset;
        glm::vec2 uSize;
        int uSdfEnabled;
        float uSdfRadius;
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
            GeneralizedPropertyType::Color4,
            {0, 1, 1, 1, 1}
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
        SDF2DUserData* userData = (SDF2DUserData*) owner->userData;
        userData->frb =  PipelineFrameBuffer(0, AsyncRendering::renderBuffer.width, AsyncRendering::renderBuffer.height);
        userData->ubo = DriverCore::GenerateUniformBuffers(sizeof(SDF2DUniformBuffer));

        if (sdf2d_pipeline == 0) {
            sdf2d_compute = GraphicsCore::CompilePipelineShader("sdf2d", ShaderType::VertexFragment);

            DescriptorSetLayoutBinding texturePoolBinding;
            texturePoolBinding.bindingPoint = 0;
            texturePoolBinding.descriptorType = DescriptorType::Sampler;
            texturePoolBinding.shaderType = ShaderType::Fragment;

            DescriptorSetLayoutBinding uniformBufferBinding;
            uniformBufferBinding.bindingPoint = 1;
            uniformBufferBinding.descriptorType = DescriptorType::UniformBuffer;
            uniformBufferBinding.shaderType = ShaderType::Fragment;
            
            DescriptorSetLayoutBuilder descriptorSetBuilder;
            descriptorSetBuilder.AddBinding(texturePoolBinding);
            descriptorSetBuilder.AddBinding(uniformBufferBinding);

            GPUExtendedHandle builtDescriptorSetLayout = descriptorSetBuilder.Build();

            RenderPipelinePushConstantRange vertexPushConstantRange;
            vertexPushConstantRange.offset = 0;
            vertexPushConstantRange.size = sizeof(SDF2DPushConstant);
            vertexPushConstantRange.shaderStage = ShaderType::VertexFragment;

            GPUExtendedHandle builtVertexPushConstantRange = vertexPushConstantRange.Build();

            RenderPipelineLayoutBuilder pipelineLayoutBuilder;
            pipelineLayoutBuilder.AddPushConstantRange(builtVertexPushConstantRange);
            pipelineLayoutBuilder.setLayouts.push_back(builtDescriptorSetLayout);

            sdf2d_layout = pipelineLayoutBuilder.Build();

            RenderPipelineShaderStageBuilder vertexShaderStageBuilder;
            vertexShaderStageBuilder.shaderModule = sdf2d_compute.vertex;
            vertexShaderStageBuilder.name = "main";
            vertexShaderStageBuilder.shaderStage = ShaderType::Vertex;
            GPUExtendedHandle vertexBuiltStage = vertexShaderStageBuilder.Build();

            RenderPipelineShaderStageBuilder fragmentShaderStageBuilder;
            fragmentShaderStageBuilder.shaderModule = sdf2d_compute.fragment;
            fragmentShaderStageBuilder.name = "main";
            fragmentShaderStageBuilder.shaderStage = ShaderType::Fragment;
            GPUExtendedHandle fragmentBuiltStage = fragmentShaderStageBuilder.Build();

            RenderPipelineBuilder renderPipelineBuilder;
            renderPipelineBuilder.pipelineLayout = sdf2d_layout;
            renderPipelineBuilder.stages.push_back(vertexBuiltStage);
            renderPipelineBuilder.stages.push_back(fragmentBuiltStage);
            
            sdf2d_pipeline = renderPipelineBuilder.Build();
            if (!sdf2d_pipeline) throw std::runtime_error("cannot create SDF2D rendering pipeline!");
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner, RenderLayerRenderDescription desc) {
        PipelineFrameBuffer* pbo = &AsyncRendering::renderBuffer;

        SDF2DUserData* userData = (SDF2DUserData*) owner->userData;
        ManagedAssetDecoder* decoder = &userData->decoder;
        PipelineFrameBuffer& frb = userData->frb;
        if (frb.width != pbo->width || frb.height != pbo->height) {
            if (frb.id) frb.Destroy();
            frb = PipelineFrameBuffer(pbo->width, pbo->height);
        }

        GraphicsCore::RequestTextureCollectionCleaning(desc.context, frb, 0.0f);

        auto position = userData->sync.position;
        auto size = userData->sync.size;
        auto color = userData->sync.color;
        auto uvOffset = userData->sync.uvOffset;
        auto angle = userData->sync.angle;

        bool texturingEnabled = userData->sync.texturingEnabled;
        std::string textureID = userData->sync.textureID;
        TextureUnion* asset = AssetCore::GetAsset(textureID);
 
        bool canTexture = (asset != nullptr && texturingEnabled);

        mat4 transform = glm::identity<mat4>();
        transform = glm::rotate(transform, glm::radians(angle), vec3(0, 0, 1));
        transform = glm::scale(transform, vec3(size, 1.0f));
        transform = glm::translate(transform, vec3(position, 0.0f));
        float aspect = (float) frb.width / (float) frb.height;
        mat4 projection = ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);

        SDF2DPushConstant pushConstants;
        pushConstants.uMatrix = projection * transform;
        pushConstants.canTexture = canTexture ? 1.0f : 0.0f;

        SDF2DUniformBuffer uniformBuffer;
        uniformBuffer.uColor = color;
        uniformBuffer.uTextureOffset = uvOffset;
        uniformBuffer.uSize = size;
        uniformBuffer.uSdfEnabled = (int) (userData->sync.sdfShape != SDFShape::None);
        
        if (canTexture && asset->type == TextureUnionType::Video) {
            VideoMetadata video = std::get<VideoMetadata>(asset->as);
            decoder->decoder.frame = std::ceil(double((GraphicsCore::renderFrame - owner->beginFrame) / GraphicsCore::renderFramerate) * (video.framerate));
        }

        auto sdfTexture = decoder->GetGPUTexture(asset, desc.context);

        DriverCore::UpdateUniformBuffers(desc.context, userData->ubo, &uniformBuffer);

        DriverCore::BeginRendering(desc.context, frb.fbo);

            if (canTexture) {
                DescriptorWriteBinding textureBinding;
                textureBinding.binding = 0;
                textureBinding.resource = sdfTexture;
                textureBinding.type = DescriptorType::Sampler;

                DescriptorWriteBinding uboBinding;
                uboBinding.binding = 1;
                uboBinding.resource = userData->ubo;
                uboBinding.type = DescriptorType::UniformBuffer;

                DriverCore::PushDescriptors(desc.context, {textureBinding, uboBinding}, sdf2d_layout, 0);    
            }

            DriverCore::BindPipeline(desc.context, sdf2d_pipeline);
            DriverCore::SetRenderingViewport(desc.context, frb.width, frb.height);
            DriverCore::PushConstants(desc.context, sdf2d_layout, ShaderType::VertexFragment, 0, sizeof(SDF2DPushConstant), &pushConstants);
            GraphicsCore::DrawArrays(desc.context, fsQuadVertices.size() / 2);
        DriverCore::EndRendering(desc.context);

        GraphicsCore::CallCompositor(frb);
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        PipelineFrameBuffer* pbo = &AsyncRendering::renderBuffer;
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
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
        layer->RenderAssetProperty(textureID, "Texturing", TextureUnionType::Texture, userData->decoder.previewHandle);
        TextureUnion* asset = AssetCore::GetAsset(layer->properties["TextureID"]);
        userData->decoder.Update(asset);
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
        json_t& position = layer->properties["Position"];
        layer->SortKeyframes(position);

        json_t& size = layer->properties["Size"];
        layer->SortKeyframes(size);

        json_t& color = layer->properties["Color"];
        layer->SortKeyframes(color);

        json_t& angle = layer->properties["Angle"];
        layer->SortKeyframes(angle);

        TextureUnion* asset = AssetCore::GetAsset(layer->properties["TextureID"]);
        if (asset != nullptr) {
            layer->internalData["StatusText"] = asset->name;
        } else {
            layer->internalData["StatusText"] = "";
        }

        if (IsInBounds(GraphicsCore::renderFrame, layer->beginFrame, layer->endFrame)) {
            SDF2DSyncConstants sync; {
                auto positionVector = layer->InterpolateProperty(layer->properties["Position"]);
                auto sizeVector = layer->InterpolateProperty(layer->properties["Size"]);
                auto colorVector = layer->InterpolateProperty(layer->properties["Color"]);
                auto angleFloat = layer->InterpolateProperty(layer->properties["Angle"]);
                auto uvOffsetVector = layer->InterpolateProperty(layer->properties["UvOffset"]);
                sync.position = vec2(positionVector[0], positionVector[1]);
                sync.size = vec2(sizeVector[0], sizeVector[1]); 
                sync.color = vec4(colorVector[0], colorVector[1], colorVector[2], colorVector[3]);
                sync.angle = angleFloat[0];
                sync.uvOffset = vec2(uvOffsetVector[0], uvOffsetVector[1]);
                sync.texturingEnabled = JSON_AS_TYPE(layer->properties["EnableTexturing"], bool);
                sync.textureID = JSON_AS_TYPE(layer->properties["TextureID"], std::string);

                switch ((SDFShape) JSON_AS_TYPE(layer->properties["SelectedSDFShape"], int)) {
                    case SDFShape::None: {
                        break;
                    };
                    case SDFShape::Circle: {
                        sync.sdfRadius = JSON_AS_TYPE(layer->InterpolateProperty(layer->properties["CircleRadius"]).at(0), float);
                        break;
                    }
                    case SDFShape::RoundedRect: {
                        sync.sdfRadius = JSON_AS_TYPE(layer->InterpolateProperty(layer->properties["BoxRadius"]).at(0), float);
                        break;
                    }
                    case SDFShape::Triangle: {
                        sync.sdfRadius = JSON_AS_TYPE(layer->InterpolateProperty(layer->properties["TriangleRadius"]).at(0), float);
                        break;
                    }
                } 
            }

            userData->sync = sync;
        }
    }

    ELECTRON_EXPORT void LayerDestroy(RenderLayer* layer) {
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
        PipelineFrameBuffer frb = userData->frb;
        frb.Destroy();
        DriverCore::DestroyUniformBuffers(userData->ubo);
        userData->decoder.Destroy();
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
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
        if (desc.dispose || !asset) {
            userData->decoder.Dispose();
            return;
        }
        if (asset->type == TextureUnionType::Video && !userData->decoder.decoder.readyToBePresented)
            return;
        if (asset && userData->decoder.IsDisposed()) {
            userData->decoder.Reinitialize(asset);
            return;
        }
        glm::vec2 assetDimensions = asset->GetDimensions();
        ImVec2 previewSize = {desc.layerSizeY * (assetDimensions.x / assetDimensions.y), desc.layerSizeY};
        ImVec2 rectSize = FitRectInRect(previewSize, ImVec2{assetDimensions.x, assetDimensions.y});
        ImVec2 upperLeft = ImGui::GetCursorScreenPos();
        ImVec2 bottomRight = ImGui::GetCursorScreenPos();
        bottomRight += ImVec2{rectSize.x, rectSize.y};
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
        ManagedAssetDecoder* decoder = &userData->decoder;
        if (asset && !decoder->IsDisposed())
            ImGui::GetWindowDrawList()->AddImage((ImTextureID) decoder->GetImageHandle(asset), upperLeft, bottomRight);
    }

    ELECTRON_EXPORT PipelineFrameBuffer LayerGetFramebuffer(RenderLayer* layer) {
        SDF2DUserData* userData = (SDF2DUserData*) layer->userData;
        PipelineFrameBuffer frb = userData->frb;
        return frb;
    }
}