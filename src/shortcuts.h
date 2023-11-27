#pragma once

#include "electron.h"
#include "shared.h"

namespace Electron {


    struct Shortcuts {
        static void Ctrl_W_R();
        static void Ctrl_P_O(ProjectMap projectMap);
        static void Ctrl_P_E();
        static void Ctrl_W_L();
        static void Ctrl_W_A();
        static void Ctrl_W_T();
        static void Ctrl_E_C();
    };
}

#include "app.h"