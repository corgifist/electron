#include "editor_core.h"

namespace Electron {
dylib ImplDylibs::RenderPreviewDylib{};
int UICounters::RenderPreviewCounter;

RenderPreview::RenderPreview() {
    this->implFunction =
        ImplDylibs::RenderPreviewDylib
            .get_function<void(AppInstance *, RenderPreview *)>(
                "RenderPreviewRender");
    UICounters::RenderPreviewCounter++;
}

RenderPreview::~RenderPreview() { UICounters::RenderPreviewCounter--; }

void RenderPreview::Render(AppInstance *instance) {
    this->implFunction(instance, this);
}
} // namespace Electron