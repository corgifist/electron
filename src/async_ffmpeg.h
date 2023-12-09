#pragma once

#include "electron.h"
#include "shared.h"

namespace Electron {

    using OperationArgs_T = std::vector<std::any>;
    typedef void (*OperationProc_T)(OperationArgs_T&);
    static int AsyncFFMpegID = 0;

    // Used to execute FFMpeg operations without hanging whole editor
    struct AsyncFFMpegOperation {
        OperationProc_T proc;
        OperationArgs_T args;
        boost::process::child* process;
        std::string cmd;
        bool procCompleted;
        int id;
        float percentage;
        std::string info;

        AsyncFFMpegOperation(OperationProc_T proc, OperationArgs_T args, std::string cmd, std::string info = "");

        bool Completed();
    };
}