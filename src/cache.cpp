#include "cache.h"

namespace Electron {
    json_t* Cache::configReference;

    int Cache::GetCacheIndex() {
        int index = JSON_AS_TYPE(configReference->at("CacheIndex"), int);
        int retValue = index;
        configReference->at("CacheIndex") = index + 1;
        return retValue;
    }
}