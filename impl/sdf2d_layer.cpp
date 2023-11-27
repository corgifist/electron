#include "editor_core.h"
#include "app.h"
#define CSTR(x) ((x).c_str())

using namespace Electron;
using namespace glm;

#define SDF_2D_ACCEPT_THRESHOLD 0.0

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "SDF2D";
    ELECTRON_EXPORT glm::vec4 LayerTimelineColor = {0.58, 0.576, 1, 1};
    ELECTRON_EXPORT json_t LayerPreviewProperties = {
        "Position", "Size", "Color", "Angle"
    };
    GLuint sdf2d_compute = -1;

    mat2 rotationMatrix(float angle) {
	    angle *= 3.1415f / 180.0f;
        float s=glm::sin(angle), c=glm::cos(angle);
        return mat2( c, -s, s, c );
    }

    vec2 rotate(vec2 uv, float th) {
        return mat2(cos(th), sin(th), -sin(th), cos(th)) * uv;
    }

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

        if (sdf2d_compute == -1) {
            sdf2d_compute = GraphicsCore::CompileComputeShader("sdf2d.compute");
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        RenderBuffer* pbo = &Shared::graphics->renderBuffer;
        GraphicsCore* core = Shared::graphics;
        if (owner->anyData.size() == 0) {
            owner->anyData.push_back(ResizableRenderBuffer(pbo->width, pbo->height));
        }

        ResizableRenderBuffer rrb = std::any_cast<ResizableRenderBuffer>(owner->anyData[0]);
        
        core->RequestTextureCollectionCleaning(rrb.color.texture, rrb.uv.texture, rrb.depth.texture, pbo->width, pbo->height);
        rrb.CheckForResize(pbo);
        owner->anyData[0] = rrb;

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

        GraphicsCore::BindGPUTexture(rrb.color.texture, 0, GL_WRITE_ONLY);
        GraphicsCore::BindGPUTexture(rrb.uv.texture, 1, GL_WRITE_ONLY);
        GraphicsCore::BindGPUTexture(rrb.depth.texture, 2, GL_WRITE_ONLY);
        if (canTexture) {
            GraphicsCore::BindGPUTexture(asset->pboGpuTexture, 3, GL_READ_ONLY);
        }
        GraphicsCore::UseShader(sdf2d_compute);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "pboResolution", pbo->width, pbo->height);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "position", position);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "size", size);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "angle", angle);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "color", color);
        GraphicsCore::ShaderSetUniform(sdf2d_compute, "canTexture", canTexture ? 1 : 0);
        if (asset) {
            GraphicsCore::ShaderSetUniform(sdf2d_compute, "assetSize", asset->GetDimensions());
        }
        GraphicsCore::DispatchComputeShader(pbo->width, pbo->height, 1);
        GraphicsCore::ComputeMemoryBarier(GL_ALL_BARRIER_BITS);

        Shared::graphics->CallCompositor(rrb.color, rrb.uv, rrb.depth);

    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AppInstance* instance = Shared::app;
        RenderBuffer* pbo = &Shared::graphics->renderBuffer;

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

        json_t& textureID = layer->properties["TextureID"];
        layer->RenderTextureProperty(textureID, "Texturing");
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
}