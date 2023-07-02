#include "editor_core.h"


Electron::ProjectConfiguration::ProjectConfiguration() {
    this->impl = LoadLibraryA("project_configuration_impl.dll");
    if (!this->impl) {
        FreeLibrary(impl);
        throw std::runtime_error("project configuration implementation is not found!");
    }

    this->implFunction = (Electron_ImplF) GetProcAddress(this->impl, "ProjectConfigurationRender");
    if (!this->implFunction) {
        FreeLibrary(impl);
        throw std::runtime_error("project configuration implementation is found, but it is likely corrupted");
    }

}

Electron::ProjectConfiguration::~ProjectConfiguration() {
    FreeLibrary(impl);
}

void Electron::ProjectConfiguration::Render(AppInstance* instance) {
    this->implFunction(instance);
}