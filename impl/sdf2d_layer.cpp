#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;
using namespace glm;

#define SDF_2D_ACCEPT_THRESHOLD 0.0

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "SDF2D";

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
            {0, 0, 0},
            {60, 0.25f, 0.25f}
        };
        owner->properties["Size"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0.5f, 0.5f},
        };
        owner->properties["Color"] = {
            GeneralizedPropertyType::Color3,
            {0, 1, 0, 0}
        };

        owner->properties["Angle"] = {
            GeneralizedPropertyType::Float,
            {0, 0},
            {60, 90}
        };

        owner->properties["TextureID"] = "";
        owner->properties["EnableTexturing"] = false;
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner, RenderRequestMetadata metadata) {
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
        std::unique_ptr<PixelBuffer> maybePBO;
        if (canTexture) {
            maybePBO = std::make_unique<PixelBuffer>(std::get<PixelBuffer>(asset->as));
        }

        RenderBuffer* rbo = metadata.rbo;
        RenderBuffer* internalRbo = &owner->graphicsOwner->renderBuffer;
        for (int x = metadata.beginX; x < metadata.endX; x++) {
            for (int y = metadata.beginY; y < metadata.endY; y++) {
                vec2 uv = {(float) x / (float) internalRbo->color.width, (float) y / (float) internalRbo->color.height};
                vec2 fragCoord = {x, y};
                vec2 iResolution = {rbo->color.width, rbo->color.height};
                uv -= 0.5f;
                uv.x *= iResolution.x/iResolution.y; // fix aspect ratio

                vec2 softwarePosition = -(size / 2.0f);
                vec2 softwareP = uv;
                softwareP -= position;
                softwareP = rotate(softwareP, radians(angle));

                if (RectContains(Rect{softwarePosition.x, softwarePosition.y, size.x, size.y}, Point{softwareP.x, softwareP.y})) {
                    vec2 correctedShift = vec2(position.x * rbo->color.width, position.y * rbo->color.height);
                    vec2 correctedUV = softwareP;
                    Pixel uvPixel = Pixel((correctedUV.x - softwarePosition.x) / (size.x), (correctedUV.y - softwarePosition.y) / (size.y), 0, 1);
                    Pixel colorPixel = Pixel(color.x, color.y, color.z, 1);
                    if (canTexture) {
                        Pixel texturePixel = PixelBufferImplGetPixel(maybePBO.get(), uvPixel.r * maybePBO->width, uvPixel.g * maybePBO->height);
                        colorPixel.r *= texturePixel.r;
                        colorPixel.g *= texturePixel.g;
                        colorPixel.b *= texturePixel.b;
                        colorPixel.a *= texturePixel.a;
                    }
                    PixelBufferImplSetPixel(&rbo->color, x, y, colorPixel);
                    PixelBufferImplSetPixel(&rbo->uv, x, y, uvPixel);
                }
            }
        }
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        AppInstance* instance = layer->graphicsOwner->owner;

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
                UIText(CSTR("Asset name: " + textureAsset->name));
                UIText(CSTR("Asset type: " + textureAsset->strType));
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