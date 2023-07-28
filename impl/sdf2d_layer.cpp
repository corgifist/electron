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
            GeneralizedPropertyType::Vec3,
            {0, 1, 0, 0}
        };

        owner->properties["Angle"] = {
            GeneralizedPropertyType::Float,
            {0, 0},
            {60, 90}
        };
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

        RenderBuffer* rbo = &owner->graphicsOwner->renderBuffer;
        for (int x = metadata.beginX; x < metadata.endX; x++) {
            for (int y = metadata.beginY; y < metadata.endY; y++) {
                vec2 uv = {(float) x / (float) rbo->color.width, (float) y / (float) rbo->color.height};
                vec2 fragCoord = {x, y};
                vec2 iResolution = {rbo->color.width, rbo->color.height};
                uv -= 0.5f;
                uv.x *= iResolution.x/iResolution.y; // fix aspect ratio

                vec2 softwarePosition = -(size / 2.0f);
                vec2 softwareP = uv;
                softwareP = softwareP * rotationMatrix(angle);

                if (RectContains(Rect{softwarePosition.x, softwarePosition.y, size.x, size.y}, Point{softwareP.x, softwareP.y})) {
                    vec2 correctedShift = vec2(position.x * rbo->color.width, position.y * rbo->color.height);
                    vec2 correctedUV = softwareP;
                    PixelBufferImplSetPixel(&rbo->color, x + correctedShift.x, y + correctedShift.y, Pixel(color.x, color.y, color.z, 1));
                    PixelBufferImplSetPixel(&rbo->uv, x + correctedShift.x, y + correctedShift.y, Pixel((correctedUV.x - softwarePosition.x) / (size.x), (correctedUV.y - softwarePosition.y) / (size.y), 0, 1));
                }
            }
        }
    }

    ELECTRON_EXPORT void LayerPropertiesRender(RenderLayer* layer) {
        json_t& position = layer->properties["Position"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Vec2, position, "Position");

        json_t& size = layer->properties["Size"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Vec2, size, "Size");

        json_t& color = layer->properties["Color"];
        RenderLayerImplRenderProperty(layer, GeneralizedPropertyType::Vec3, color, "Color");
    }
}