#include "editor_core.h"
#include "ui_api.h"

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
        owner->properties["EnableTexturing"] = false;

        if (sdf2d_compute == -1) {
            sdf2d_compute = GraphicsImplCompileComputeShader(owner->graphicsOwner, "sdf2d.compute");
        }
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner, RenderRequestMetadata metadata) {
        RenderBuffer* pbo = &owner->graphicsOwner->renderBuffer;
        GraphicsCore* core = owner->graphicsOwner;
        if (owner->anyData.size() == 0) {
            owner->anyData.push_back(ResizableGPUTextureCreate(core, pbo->width, pbo->height));
            owner->anyData.push_back(ResizableGPUTextureCreate(core, pbo->width, pbo->height));
            owner->anyData.push_back(ResizableGPUTextureCreate(core, pbo->width, pbo->height));
        }

        ResizableGPUTexture colorTexture = std::any_cast<ResizableGPUTexture>(owner->anyData[0]);
        ResizableGPUTImplCheckForResize(&colorTexture, pbo);
        owner->anyData[0] = colorTexture;

        ResizableGPUTexture uvTexture = std::any_cast<ResizableGPUTexture>(owner->anyData[1]);
        ResizableGPUTImplCheckForResize(&uvTexture, pbo);
        owner->anyData[1] = uvTexture;

        ResizableGPUTexture depthTexture = std::any_cast<ResizableGPUTexture>(owner->anyData[2]);
        ResizableGPUTImplCheckForResize(&depthTexture, pbo);
        owner->anyData[2] = depthTexture;
        
        GraphicsImplRequestTextureCollectionCleaning(core, colorTexture.texture, uvTexture.texture, depthTexture.texture, pbo->width, pbo->height, metadata);

        auto position = vec2();
        auto size = vec2();
        auto color = vec3();
        auto angle = 0.0f; {
            auto positionVector = JSON_AS_TYPE(RenderLayerImplInterpolateProperty(owner, owner->properties["Position"]), std::vector<float>);
            auto sizeVector = JSON_AS_TYPE(RenderLayerImplInterpolateProperty(owner, owner->properties["Size"]), std::vector<float>);
            auto colorVector = JSON_AS_TYPE(RenderLayerImplInterpolateProperty(owner, owner->properties["Color"]), std::vector<float>);
            auto angleFloat = JSON_AS_TYPE(RenderLayerImplInterpolateProperty(owner, owner->properties["Angle"]), std::vector<float>);
            position = vec2(positionVector[0], positionVector[1]);
            size = vec2(sizeVector[0], sizeVector[1]); 
            color = vec3(colorVector[0], colorVector[1], colorVector[2]);
            angle = angleFloat[0];
        }

        bool texturingEnabled = JSON_AS_TYPE(owner->properties["EnableTexturing"], bool);
        std::string textureID = JSON_AS_TYPE(owner->properties["TextureID"], std::string);
        TextureUnion* asset = nullptr;
        auto& assets = owner->graphicsOwner->owner->assets.assets;
        for (int i = 0; i < assets.size(); i++) {
            if (intToHex(assets.at(i).id) == textureID) {
                if (assets.at(i).IsTextureCompatible())
                    asset = &assets.at(i);
            }
        }

        bool canTexture = (asset != nullptr && texturingEnabled);

        GraphicsImplBindGPUTexture(owner->graphicsOwner, colorTexture.texture, 0);
        GraphicsImplBindGPUTexture(owner->graphicsOwner, uvTexture.texture, 1);
        GraphicsImplBindGPUTexture(owner->graphicsOwner, depthTexture.texture, 2);
        if (canTexture) {
            GraphicsImplBindGPUTexture(owner->graphicsOwner, asset->pboGpuTexture, 3);
        }
        GraphicsImplUseShader(owner->graphicsOwner, sdf2d_compute);
        GraphicsImplShaderSetUniformII(owner->graphicsOwner, sdf2d_compute, "pboResolution", pbo->width, pbo->height);
        GraphicsImplShaderSetUniformFF(owner->graphicsOwner, sdf2d_compute, "position", position);
        GraphicsImplShaderSetUniformFF(owner->graphicsOwner, sdf2d_compute, "size", size);
        GraphicsImplShaderSetUniformF(owner->graphicsOwner, sdf2d_compute, "angle", angle);
        GraphicsImplShaderSetUniformFFF(owner->graphicsOwner, sdf2d_compute, "color", color);
        GraphicsImplShaderSetUniformI(owner->graphicsOwner, sdf2d_compute, "canTexture", canTexture ? 1 : 0);
        GraphicsImplDispatchComputeShader(owner->graphicsOwner, std::ceil(pbo->width / 8), std::ceil(pbo->height / 4), 1);
        GraphicsImplMemoryBarrier(owner->graphicsOwner, GL_ALL_BARRIER_BITS);

        GraphicsImplCallCompositor(core, colorTexture, uvTexture, depthTexture);

    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AppInstance* instance = layer->graphicsOwner->owner;
        RenderBuffer* pbo = &layer->graphicsOwner->renderBuffer;

        json_t& position = layer->properties["Position"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Vec2, position, "Position");

        json_t& size = layer->properties["Size"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Vec2, size, "Size");

        json_t& color = layer->properties["Color"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Color3, color, "Color");

        json_t& angle = layer->properties["Angle"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Float, angle, "Angle");

        if (UICollapsingHeader("Texturing")) {
            bool enableTexturing = JSON_AS_TYPE(layer->properties["EnableTexturing"], bool);
            UICheckbox("Enable texturing", &enableTexturing);
            layer->properties["EnableTexturing"] = enableTexturing;

            if (enableTexturing) {
            std::string textureID = JSON_AS_TYPE(layer->properties["TextureID"], std::string);
            UIInputField("Texture ID", &textureID, ImGuiInputTextFlags_AutoSelectAll);
            layer->properties["TextureID"] = textureID;

            TextureUnion* textureAsset = nullptr;
            auto& assets = instance->assets.assets;
            for (int i = 0; i < assets.size(); i++) {
                if (assets.at(i).id == hexToInt(textureID)) {
                    textureAsset = &assets.at(i);
                }
            }

            if (textureAsset == nullptr) {
                UIText(CSTR("No asset with ID '" + textureID + "' found"));
            } else if (!textureAsset->IsTextureCompatible()) {
                UIText(CSTR("Asset with ID '" + textureID + "' is not texture-compatible"));
            } else {
                glm::vec2 textureDimensions = TextureUnionImplGetDimensions(textureAsset);
                UIText(CSTR("Asset name: " + textureAsset->name));
                UIText(CSTR("Asset type: " + textureAsset->strType));
                UIText(CSTR("SDF-Type asset size" + std::string(": ") + std::to_string((textureDimensions.y / pbo->height)) + "x" + std::to_string((textureDimensions.x / pbo->width))));
            }
            }
        }
    }

    ELECTRON_EXPORT void LayerSortKeyframes(RenderLayer* layer) {
        json_t& position = layer->properties["Position"];
        RenderLayerImplSortKeyframes(layer, position);

        json_t& size = layer->properties["Size"];
        RenderLayerImplSortKeyframes(layer, size);

        json_t& color = layer->properties["Color"];
        RenderLayerImplSortKeyframes(layer, color);

        json_t& angle = layer->properties["Angle"];
        RenderLayerImplSortKeyframes(layer, angle);
    }
}