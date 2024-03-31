#pragma once

namespace Electron {
    // Just a helper tool to get unique number
    struct Cache {
        static int cacheIndex;
        static int GetCacheIndex();
    };
}