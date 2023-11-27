#include "async_ffmpeg.h"

namespace Electron {

    AsyncFFMpegOperation::AsyncFFMpegOperation(OperationProc_T proc, OperationArgs_T args, std::vector<std::string> cmd) {
        this->proc = proc;
        this->args = args;
        this->procCompleted = false;

        cmd.at(0) = boost::process::search_path(cmd[0]).c_str();
        this->process = new boost::process::child(cmd);
        this->cmd = cmd;
    }

    bool AsyncFFMpegOperation::Completed() {
        return !this->process->running();
    }

    std::string AsyncFFMpegOperation::Cmd() {
        std::string acc;
        for (auto& x : cmd) {
            acc += x + " ";
        }
        return acc;
    }

}