#include "editor_core.h"

namespace Electron {

    dylib ImplDylibs::DockspaceDylib{};

    Dockspace::Dockspace() {
        this->impl = ImplDylibs::DockspaceDylib.get_function<void(AppInstance*)>("DockspaceRender");
    }

    void Dockspace::Render(AppInstance* instance) {
        this->impl(instance);
    }

};