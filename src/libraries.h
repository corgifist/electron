#pragma once
#include "electron.h"

namespace Electron {
    class Libraries {
    public:
        static std::unordered_map<std::string, internalDylib> registry;

        static void LoadLibrary(std::string path, std::string key) {
            if (registry.find(key) != registry.end()) return; // do not reload libraries
            registry[key] = internalDylib(path, key);
        }

        template<typename T>
        static T* GetFunction(std::string key, std::string name) {
            if (registry.find(key) == registry.end())
                LoadLibrary(".", key);
            internalDylib* d = &registry[key];
            return d->get_function<T>(name.c_str());
        }

        template<typename T>
        static T& GetVariable(std::string key, std::string name) {
            if (registry.find(key) == registry.end())
                LoadLibrary(".", key);
            internalDylib* d = &registry[key];
            return d->get_variable<T>(name.c_str());
        }
    };
};