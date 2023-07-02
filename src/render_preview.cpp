#include "editor_core.h"

Electron::RenderPreview::RenderPreview() {
    this->impl = LoadLibraryA("render_preview_impl.dll");
    if (!this->impl) {
        FreeLibrary(impl);
        throw std::runtime_error("render preview implementation is not found!");
    }

    this->implFunction = (Electron_RenderPreviewImplF) GetProcAddress(this->impl, "RenderPreviewRender");
    if (!this->implFunction) {
        FreeLibrary(impl);
        throw std::runtime_error("render preview implementation is found, but it is likely corrupted");
    }
}

Electron::RenderPreview::~RenderPreview() {
    FreeLibrary(impl);
}

void Electron::RenderPreview::Render(AppInstance* instance) {
    this->implFunction(instance, this);
}