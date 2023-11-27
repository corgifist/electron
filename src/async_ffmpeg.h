#pragma once

#include "electron.h"

namespace Electron {

    using OperationArgs_T = std::vector<std::any>;
    typedef void (*OperationProc_T)(OperationArgs_T&);

    // Used to execute FFMpeg operations without hanging whole editor
    struct AsyncFFMpegOperation {
        OperationProc_T proc;
        OperationArgs_T args;
        boost::process::child* process;
        std::vector<std::string> cmd;
        bool procCompleted;

        AsyncFFMpegOperation(OperationProc_T proc, OperationArgs_T args, std::vector<std::string> cmd);

        bool Completed();
        std::string Cmd();
    };
}