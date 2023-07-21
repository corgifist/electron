#include "editor_core.h"

dylib Electron::ImplDylibs::RenderPreviewDylib{};

Electron::RenderPreview::RenderPreview() {
    this->implFunction = ImplDylibs::RenderPreviewDylib.get_function<void(AppInstance*, RenderPreview*)>("RenderPreviewRender");
}

Electron::RenderPreview::~RenderPreview() {
}

void Electron::RenderPreview::Render(AppInstance* instance) {
    this->implFunction(instance, this);
}