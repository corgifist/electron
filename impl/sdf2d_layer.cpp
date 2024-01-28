#include "editor_core.h"
#include "app.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define SDF_2D_ACCEPT_THRESHOLD 0.0

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "SDF2D";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {0.58, 0.576, 1, 1};
    GLuint sdf2d_compute = UNDEFINED;
    GLuint sdf2d_vao = UNDEFINED;

    enum class SDFShape {
        None = 0,
        Circle = 1,
        RoundedRect = 2
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
            {"BoxRadius", ICON_FA_SQUARE " Radius"}
        };

        owner->properties["SelectedSDFShape"] = 0;

        owner->anyData.resize(1);
        owner->anyData[0] = PipelineFrameBuffer(Shared::graphics->renderBuffer.width, Shared::graphics->renderBuffer.height);

        if (sdf2d_compute == UNDEFINED) {
            sdf2d_compute = GraphicsCore::CompilePipelineShader("sdf2d.pipeline");
        }

        if (sdf2d_vao == UNDEFINED) {
            sdf2d_vao = GraphicsCore::GenerateVAO(fsQuadVertices, fsQuadUV);
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        PipelineFrameBuffer* pbo = &Shared::graphics->renderBuffer;
        GraphicsCore* core = Shared::graphics;


        PipelineFrameBuffer frb = std::any_cast<PipelineFrameBuffer>(owner->anyData[0]);
        if (frb.width != pbo->width || frb.height != pbo->height) {
            frb.Destroy();
            frb = PipelineFrameBuffer(pbo->width, pbo->height);
        }
        
        owner->anyData[0] = frb;
        frb = std::any_cast<PipelineFrameBuffer>(owner->anyData[0]); // update sdf2d_fbo (fixes some small issues)
        Shared::graphics->RequestTextureCollectionCleaning(frb, 0.0f);

        auto position = vec2();
        auto size = vec2();
        auto color = vec3();
        auto angle = 0.0f; {
            auto positionVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Position"]), std::vector<float>);
            auto sizeVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Size"]), std::vector<float>);
            auto colorVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Color"]), std::vector<float>);
            auto angleFloat = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Angle"]), std::vector<float>);
            position = vec2(positionVector[0], positionVector[1]);
            size = vec2(sizeVector[0], sizeVector[1]); 
            color = vec3(colorVector[0], colorVector[1], colorVector[2]);
            angle = angleFloat[0];
        }

        bool texturingEnabled = JSON_AS_TYPE(owner->properties["EnableTexturing"], bool);
        std::string textureID = JSON_AS_TYPE(owner->properties["TextureID"], std::string);
        TextureUnion* asset = nullptr;
        auto& assets = Shared::assets->assets;
        for (int i = 0; i < assets.size(); i++) {
            if (intToHex(assets.at(i).id) == textureID) {
                if (assets.at(i).IsTextureCompatible())
                    asset = &assets.at(i);
            }
        }

        bool canTexture = (asset != nullptr && texturingEnabled);

        frb.Bind();
        GraphicsCore::UseShader(sdf2d_compute);
        mat4 transform = glm::identity<mat4>();
        transform = glm::scale(transform, vec3(size, 1.0f));
        transform = glm::rotate(transform, glm::radians(angle), vec3(0, 0, 1));
        transform = glm::translate(transform, vec3(position, 0.0f));
        float aspect = (float) frb.width / (float) frb.height;
        mat4 projection = ortho(-aspect, aspect, -1.0f, 1.0f, -1.0f, 1.0f);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uColor", color);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uTransform", transform);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uProjection", projection);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uSdfShape", JSON_AS_TYPE(owner->properties["SelectedSDFShape"], int));
        switch ((SDFShape) JSON_AS_TYPE(owner->properties["SelectedSDFShape"], int)) {
            case SDFShape::None: break;
            case SDFShape::Circle: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["CircleRadius"]), std::vector<float>)[0];
                GraphicsCore::ShaderSetUniform(sdf2d_compute, "uSdfCircleRadius", radius);
                break;
            }
            case SDFShape::RoundedRect: {
                float radius = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["BoxRadius"]), std::vector<float>)[0];
                GraphicsCore::ShaderSetUniform(sdf2d_compute, "uSdfCircleRadius", radius);
                break;
            }
        }
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uSize", size);
        if (canTexture) {
            GraphicsCore::BindGPUTexture(asset->pboGpuTexture, sdf2d_compute, 0, "uTexture");
        }
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "uCanTexture", (int) canTexture);
        glBindVertexArray(sdf2d_vao);
        glDrawArrays(GL_TRIANGLES, 0, fsQuadVertices.size() / 2);
        frb.Unbind();

        Shared::graphics->CallCompositor(frb);
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AppInstance* instance = Shared::app;
        PipelineFrameBuffer* pbo = &Shared::graphics->renderBuffer;

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

        static std::vector<std::string> shapesCollection = {
            ICON_FA_XMARK " None",
            ICON_FA_CIRCLE " Circle",
            ICON_FA_SQUARE " Rounded Rectangle"
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
            "Position", "Size", "Color", "Angle"
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
        }
        return base;
    }
}