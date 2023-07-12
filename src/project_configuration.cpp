#include "editor_core.h"


Electron::ProjectConfiguration::ProjectConfiguration() {
    this->implLibrary = dylib(".", "project_configuration_impl");

    this->implFunction = implLibrary.get_function<void(AppInstance*)>("ProjectConfigurationRender");

}

Electron::ProjectConfiguration::~ProjectConfiguration() {
}

void Electron::ProjectConfiguration::Render(AppInstance* instance) {
    this->implFunction(instance);
}