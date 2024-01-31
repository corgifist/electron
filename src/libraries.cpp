#include "libraries.h"

namespace Electron {
    std::unordered_map<std::string, internalDylib> Libraries::registry{};
};