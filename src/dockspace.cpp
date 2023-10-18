#include "editor_core.h"

namespace Electron {

    std::string ImplDylibs::DockspaceDylib = "dockspace_impl";

    Dockspace::Dockspace() {
        this->impl = Libraries::GetFunction<void(AppInstance *)>(
            ImplDylibs::DockspaceDylib, "DockspaceRender");
    }

    void Dockspace::Render(AppInstance *instance) { this->impl(instance); }

}; // namespace Electron