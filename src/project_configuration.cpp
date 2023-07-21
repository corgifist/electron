#include "editor_core.h"

dylib Electron::ImplDylibs::ProjectConfigurationDylib{};

Electron::ProjectConfiguration::ProjectConfiguration() {
    this->implFunction = ImplDylibs::ProjectConfigurationDylib.get_function<void(AppInstance*)>("ProjectConfigurationRender");
}

Electron::ProjectConfiguration::~ProjectConfiguration() {
}

void Electron::ProjectConfiguration::Render(AppInstance* instance) {
    this->implFunction(instance);
}