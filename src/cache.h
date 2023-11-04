#pragma once
#include "electron.h"

namespace Electron {
    class Cache {
    public:
        static json_t* configReference;
        static int GetCacheIndex();
    };
}