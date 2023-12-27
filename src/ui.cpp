#include "editor_core.h"

namespace Electron {

    static int placeholder = 0;

    UIActivity::UIActivity(std::string libraryName, int* counter) {
        this->counter = counter;
        this->libraryName = libraryName;
        if (counter == nullptr) counter = &placeholder;
        *counter = 1;
        Libraries::LoadLibrary("libs", libraryName);
        this->impl = Libraries::GetFunction<void(AppInstance*)>(libraryName, "UIRender");
    }

    UIActivity::~UIActivity() {
    }

    void UIActivity::Render(AppInstance* instance) {
        impl(instance);
    }

}