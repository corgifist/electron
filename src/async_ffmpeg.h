#pragma once

#include "electron.h"

namespace Electron {
    struct AsyncFFMpegOperation {
        std::thread operation;
        std::string cmd;

        AsyncFFMpegOperation(std::string cmd);

        bool Completed();
    };
}