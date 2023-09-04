#include "editor_core.h"

namespace Electron {
    dylib ImplDylibs::ProjectConfigurationDylib{};
    int UICounters::ProjectConfigurationCounter;

    ProjectConfiguration::ProjectConfiguration() {
        this->implFunction = ImplDylibs::ProjectConfigurationDylib.get_function<void(AppInstance*)>("ProjectConfigurationRender");
        UICounters::ProjectConfigurationCounter++;
    }

    ProjectConfiguration::~ProjectConfiguration() {
    }

    void ProjectConfiguration::Render(AppInstance* instance) {
        this->implFunction(instance);
    }
}