#include "editor_core.h"

namespace Electron {
    std::string ImplDylibs::LayerPropertiesDylib = "layer_properties_impl";
    int UICounters::LayerPropertiesCounter;

    LayerProperties::LayerProperties() {
        this->impl =
            Libraries::GetFunction<void(AppInstance *)>(
                ImplDylibs::LayerPropertiesDylib, "LayerPropertiesRender");
        UICounters::LayerPropertiesCounter++;
    }

    LayerProperties::~LayerProperties() { UICounters::LayerPropertiesCounter = 0; }

    void LayerProperties::Render(AppInstance *instance) { impl(instance); }
}; // namespace Electron