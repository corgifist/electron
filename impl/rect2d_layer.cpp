#include "editor_core.h"
#include "ui_api.h"

using namespace Electron;

extern "C" {

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
                rbo->color.SetPixel(x, y, Pixel(1, 0, 1, 1));
                rbo->uv.SetPixel(x, y, Pixel((float) x / (float) rbo->color.width, (float) y / (float) rbo->color.height, 0, 1));
            }
        }
    }
}