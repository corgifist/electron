#include "editor_core.h"

dylib Electron::ImplDylibs::LayerPropertiesDylib{};

Electron::LayerProperties::LayerProperties() {
    this->impl = ImplDylibs::LayerPropertiesDylib.get_function<void(AppInstance*)>("LayerPropertiesRender");
}

void Electron::LayerProperties::Render(AppInstance* instance) {
    impl(instance);
}