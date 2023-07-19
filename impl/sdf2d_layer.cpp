#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;
using namespace glm;

#define SDF_2D_ACCEPT_THRESHOLD 0.0

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "SDF2D";

    void LayerInitialize(RenderLayer* owner) {
        owner->properties["Position"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0, 0},
            {30, 0.2f, 0.2f},
            {60, 0.5, 0.23f}
        };
        owner->properties["Size"] = {
            GeneralizedPropertyType::Vec2,
            {0, 0, 0},
            {30, 0.2f, 0.5f},
            {60, 0.1f, 0.1f}
        };
        owner->properties["Color"] = {
            GeneralizedPropertyType::Vec3,
            {0, 1, 0, 0},
            {60, 0, 0, 1}
        };
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        if (!owner->initialized) {
            LayerInitialize(owner);
            owner->initialized = true;
        }

        auto position = vec2();
        auto size = vec2();
        auto color = vec3(); {
            auto positionVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Position"]), std::vector<float>);
            auto sizeVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Size"]), std::vector<float>);
            auto colorVector = JSON_AS_TYPE(owner->InterpolateProperty(owner->properties["Color"]), std::vector<float>);
            position = vec2(positionVector[0], positionVector[1]);
            size = vec2(sizeVector[0], sizeVector[1]); 
            color = vec3(colorVector[0], colorVector[1], colorVector[2]);
        }

        RenderBuffer* rbo = &owner->graphicsOwner->renderBuffer;
        for (int x = 0; x < rbo->color.width; x++) {
            for (int y = 0; y < rbo->color.height; y++) {
                vec2 uv = {(float) x / (float) rbo->color.width, (float) y / (float) rbo->color.height};
                vec2 fragCoord = {x, y};
                vec2 iResolution = {rbo->color.width, rbo->color.height};
                uv -= 0.5;
                uv.x *= iResolution.x/iResolution.y; // fix aspect ratio

                vec2 p = uv;
                p += position;
                vec2 b = size;

                vec2 d = abs(p)-b;
                float dist = length(max(d,0.0f)) + min(max(d.x,d.y),0.0f);

                if (dist < 0.0) {
                    rbo->color.SetPixel(x, y, Pixel(color.r, color.g, color.b, 1));
                    rbo->uv.SetPixel(x, y, Pixel(p.x, p.y, 0, 1));
                }
            }
        }
    }
}