#include "editor_core.h"

namespace Electron {
std::string ImplDylibs::RenderPreviewDylib = "render_preview_impl";
int UICounters::RenderPreviewCounter;

RenderPreview::RenderPreview() {
    this->implFunction =
            Libraries::GetFunction<void(AppInstance *)>(
                ImplDylibs::RenderPreviewDylib, "RenderPreviewRender");
    UICounters::RenderPreviewCounter++;
}

RenderPreview::~RenderPreview() { UICounters::RenderPreviewCounter--; }

void RenderPreview::Render(AppInstance *instance) {
    this->implFunction(instance);
}
} // namespace Electron