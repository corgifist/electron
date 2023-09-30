#include "editor_core.h"

namespace Electron {
dylib ImplDylibs::LayerPropertiesDylib{};
int UICounters::LayerPropertiesCounter;

LayerProperties::LayerProperties() {
    this->impl =
        ImplDylibs::LayerPropertiesDylib.get_function<void(AppInstance *)>(
            "LayerPropertiesRender");
    UICounters::LayerPropertiesCounter++;
}

LayerProperties::~LayerProperties() { UICounters::LayerPropertiesCounter = 0; }

void LayerProperties::Render(AppInstance *instance) { impl(instance); }
}; // namespace Electron