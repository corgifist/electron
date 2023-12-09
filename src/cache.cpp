#include "cache.h"

namespace Electron {
    int Cache::cacheIndex = 0;

    int Cache::GetCacheIndex() {
        cacheIndex += 2;
        
        return cacheIndex;
    }
}