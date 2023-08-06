#include "editor_core.h"

dylib Electron::ImplDylibs::LayerPropertiesDylib{};
int Electron::UICounters::LayerPropertiesCounter;

Electron::LayerProperties::LayerProperties() {
    this->impl = ImplDylibs::LayerPropertiesDylib.get_function<void(AppInstance*)>("LayerPropertiesRender");
    UICounters::LayerPropertiesCounter++;
}

void Electron::LayerProperties::Render(AppInstance* instance) {
    impl(instance);
}