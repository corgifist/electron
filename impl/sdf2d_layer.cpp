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

        owner->anyData.resize(1);
        owner->anyData[0] = PipelineFrameBuffer(GraphicsCore::renderBuffer.width, GraphicsCore::renderBuffer.height);

        if (sdf2d_compute.fragment == 0) {
            sdf2d_compute = GraphicsCore::CompilePipelineShader("sdf2d.pipeline", ShaderType::Fragment);
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        PipelineFrameBuffer* pbo = &GraphicsCore::renderBuffer;

        PipelineFrameBuffer frb = std::any_cast<PipelineFrameBuffer>(owner->anyData[0]);
        if (frb.width != pbo->width || frb.height != pbo->height) {
            frb.Destroy();
            frb = PipelineFrameBuffer(pbo->width, pbo->height);
        }
        
        owner->anyData[0] = frb;

        static std::unordered_map<int, GLuint64> handlePool{};
        static std::vector<GLuint64> handles(TEXTURE_POOL_SIZE);
        static int handleIndex = 0;

        frb = std::any_cast<PipelineFrameBuffer>(owner->anyData[0]);
        GraphicsCore::RequestTextureCollectionCleaning(frb, 0.0f);

        auto position = vec2();
        auto size = vec2();
        auto color = vec3();
        auto uvOffset = vec2();
        auto angle = 0.0f; {
            auto positionVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Position"]), std::vector<float>);
            auto sizeVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Size"]), std::vector<float>);
            auto colorVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Color"]), std::vector<float>);
            auto angleFloat = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Angle"]).at(0), float);
            auto uvOffsetVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["UvOffset"]), std::vector<float>);
            position = vec2(positionVector[0], positionVector[1]);
            size = vec2(sizeVector[0], sizeVector[1]); 
            color = vec3(colorVector[0], colorVector[1], colorVector[2]);
            angle = angleFloat;
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
        GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uColor", color);
        GraphicsCore::ShaderSetUniform(GraphicsCore::basic.vertex, "uMatrix", projection * transform);
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
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnable", true);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfCircleRadius", radius);
                break;
            }
            case SDFShape::RoundedRect: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["BoxRadius"]).at(0), float);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnable", true);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfCircleRadius", radius);
                break;
            }
            case SDFShape::Triangle: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["TriangleRadius"]).at(0), float);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfEnable", true);
                GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uSdfCircleRadius", radius);
                break;
            }
        }

        static GLuint samplersBuffer = 0;
        if (samplersBuffer == 0) {
            glCreateBuffers(1, &samplersBuffer);
            glNamedBufferStorage(samplersBuffer, TEXTURE_POOL_SIZE * sizeof(GLuint64), NULL, GL_DYNAMIC_STORAGE_BIT | PERSISTENT_FLAG);
        }

        if (canTexture) {
            if (handlePool.find(asset->id) == handlePool.end()) {
                handlePool[asset->id] = glGetTextureHandleARB(asset->pboGpuTexture);
                if (!handlePool[asset->id]) {
                    throw std::runtime_error("cannot get asset texture handle! (sdf2d)");
                }
            }

            if (std::find(handles.begin(), handles.end(), handlePool[asset->id]) == handles.end()) {
                if (handleIndex >= TEXTURE_POOL_SIZE) {
                    glMakeTextureHandleNonResidentARB(handles[0]);
                    handleIndex = 0;
                    handles[handleIndex++] = handlePool[asset->id];
                    glMakeTextureHandleResidentARB(handles[0]);
                } else {
                    if (handles[handleIndex] != 0) {
                        glMakeTextureHandleNonResidentARB(handles[handleIndex]);
                    }
                    handles[handleIndex++] = handlePool[asset->id];
                    glMakeTextureHandleResidentARB(handlePool[asset->id]);
                }
                glNamedBufferSubData(samplersBuffer, (handleIndex - 1) * sizeof(GLuint64), sizeof(GLuint64), handles.data() + (handleIndex - 1));
            }

            GLuint64 desiredHandle = handlePool[asset->id];
            int uTextureID = 0;
            for (int i = 0; i < handles.size(); i++) {
                if (handles[i] == desiredHandle)
                    uTextureID = i;
            }
            GraphicsCore::ShaderSetUniform(sdf2d_compute.fragment, "uTextureID", uTextureID);
        }
        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, samplersBuffer);
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
        PipelineFrameBuffer rbo = std::any_cast<PipelineFrameBuffer>(layer->anyData[0]);
        rbo.Destroy();
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

    ELECTRON_EXPORT void LayerMakeFramebufferResident(RenderLayer* layer, bool resident) {
        PipelineFrameBuffer frb = std::any_cast<PipelineFrameBuffer>(layer->anyData[0]);
        if (resident) frb.MakeResident();
        else frb.MakeNonResident();
        layer->anyData[0] = frb;
    }

    ELECTRON_EXPORT PipelineFrameBuffer LayerGetFramebuffer(RenderLayer* layer) {
        return std::any_cast<PipelineFrameBuffer>(layer->anyData[0]);
    }
}