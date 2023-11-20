#include "async_ffmpeg.h"

namespace Electron {
    AsyncFFMpegOperation::AsyncFFMpegOperation(std::string cmd) {
        this->cmd = cmd;
        this->operation = std::thread([](std::string cmd) {
            system(cmd.c_str());
        }, cmd);
    }

    bool AsyncFFMpegOperation::Completed() {
        return !operation.joinable();
    }
}