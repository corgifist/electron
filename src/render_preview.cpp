#include "editor_core.h"

dylib Electron::ImplDylibs::RenderPreviewDylib{};
int Electron::UICounters::RenderPreviewCounter;

Electron::RenderPreview::RenderPreview() {
    this->implFunction = ImplDylibs::RenderPreviewDylib.get_function<void(AppInstance*, RenderPreview*)>("RenderPreviewRender");
    UICounters::RenderPreviewCounter++;
}

Electron::RenderPreview::~RenderPreview() {
}

void Electron::RenderPreview::Render(AppInstance* instance) {
    this->implFunction(instance, this);
}