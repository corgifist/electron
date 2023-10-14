#include "editor_core.h"

namespace Electron {
std::string ImplDylibs::ProjectConfigurationDylib = "project_configuration_impl";
int UICounters::ProjectConfigurationCounter;

ProjectConfiguration::ProjectConfiguration() {
    this->implFunction =
        Libraries::GetFunction<void(AppInstance *)>(
            ImplDylibs::ProjectConfigurationDylib, "ProjectConfigurationRender");
    UICounters::ProjectConfigurationCounter++;
}

ProjectConfiguration::~ProjectConfiguration() {}

void ProjectConfiguration::Render(AppInstance *instance) {
    this->implFunction(instance);
}
} // namespace Electron