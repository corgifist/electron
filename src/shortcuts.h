#pragma once

#include "electron.h"

namespace Electron {

    class AppInstance;
    struct ProjectMap;

    class Shortcuts {
    public:
        AppInstance* owner;

        Shortcuts() {
            this->owner = nullptr;
        }

        void Ctrl_W_R();

        void Ctrl_P_O(ProjectMap projectMap);
        
        void Ctrl_P_E();

        void Ctrl_W_L();
    };
}

#include "app.h"