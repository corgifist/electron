#include "async_ffmpeg.h"

namespace Electron {

    AsyncFFMpegOperation::AsyncFFMpegOperation(OperationProc_T proc, OperationArgs_T args, std::string cmd, std::string info) {
        if (info == "")
            info = ELECTRON_GET_LOCALIZATION("BACKGROUND_OPERATIONS");
        this->proc = proc;
        this->args = args;
        this->procCompleted = false;
        this->id = (++AsyncFFMpegID);
        this->percentage = 0.0f;
        this->info = info;

        this->cmd = cmd;
        this->process = new boost::process::child(
            boost::process::search_path("bash"), std::vector<std::string> {
                "-c", cmd
            }
        );
    }

    bool AsyncFFMpegOperation::Completed() {
        return !this->process->running();
    }
}