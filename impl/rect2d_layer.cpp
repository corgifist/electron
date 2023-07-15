#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;
using namespace glm;

#define SDF_2D_ACCEPT_THRESHOLD 0.0

extern "C" {

    ELECTRON_EXPORT std::string LayerName = "Rect2D";

    void LayerInitialize(RenderLayer* owner) {
        owner->properties["position"] = {0, 0};
    }

    ELECTRON_EXPORT void LayerRender(RenderLayer* owner) {
        if (!owner->initialized) {
            LayerInitialize(owner);
            owner->initialized = true;
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
                vec2 b = vec2(0.2f, 0.2f);

                vec2 d = abs(p)-b;
                float dist = length(max(d,0.0f)) + min(max(d.x,d.y),0.0f);

                if (dist < 0.0) {
                    rbo->color.SetPixel(x, y, Pixel(0, 0, 1, 1));
                    rbo->uv.SetPixel(x, y, Pixel(uv.x, uv.y, 0, 1));
                }
            }
        }
    }
}