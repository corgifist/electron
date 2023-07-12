#include "editor_core.h"

Electron::RenderPreview::RenderPreview() {
    this->implLibrary = dylib(".", "render_preview_impl");
    this->implFunction = implLibrary.get_function<void(AppInstance*, RenderPreview*)>("RenderPreviewRender");
}

Electron::RenderPreview::~RenderPreview() {
}

void Electron::RenderPreview::Render(AppInstance* instance) {
    this->implFunction(instance, this);
}